#include "Syphon.h"

#include "Projector.h"
#include "FboCubeMapLayered.h"
#include "MeshHelpers.h"

#include "WindowData.h"
#include "ParamsControl.h"

using namespace ci;
using namespace ci::app;
using std::vector;
using std::string;

struct ProjectorUploadData {
	ProjectorUploadData() {};
	ProjectorUploadData(vec3 _p, vec3 _t, Color _c) : position(_p), target(_t), color(_c.r, _c.g, _c.b) {}

	vec3 position;
	float pad1;
	vec3 target;
	float pad2;
	vec3 color;
	float pad3;
};

class DigitalLifeProjectorControlApp : public App {
  public:
	static void prepSettings(Settings * settings);

	void setup() override;
	void keyDown(KeyEvent evt) override;
	void update() override;
	void draw() override;

	void createNewWindow();

	uint32_t mNumWindowsCreated = 0;
	uint32_t mDestinationCubeMapSide = 1600;
	string mParamsFile = "projectorControlParams.json";

	JsonTree mParamsTree;

	ciSyphon::ClientRef mSyphonClient;
	gl::TextureRef mLatestFrame;
	FboCubeMapLayeredRef mFrameDestinationCubeMap;
	gl::VboMeshRef mFrameToCubeMapConvertMesh;
	gl::GlslProgRef mFrameToCubeMapConvertShader;
	gl::BatchRef mFrameToCubeMapConvertBatch;

	gl::VboMeshRef mScanSphereMesh;
	gl::TextureRef mScanSphereTexture;
	gl::GlslProgRef mProjectorCoverageShader;
	gl::GlslProgRef mSyphonFrameAsCubeMapRenderShader;
};

void DigitalLifeProjectorControlApp::prepSettings(Settings * settings) {

}

void DigitalLifeProjectorControlApp::setup() {
	mParamsTree = loadProjectorParams(this, mParamsFile);

	JsonTree mainWindowParams = mParamsTree.getNumChildren() > 0 ? mParamsTree.getChild(0) : JsonTree();
	getWindow()->setUserData<WindowData>(new WindowData(mNumWindowsCreated++, getWindow(), mainWindowParams));

	mSyphonClient = ciSyphon::Client::create();
	mSyphonClient->set("DigitalLifeServer", "DigitalLifeClient");
	mSyphonClient->setup();

	mFrameDestinationCubeMap = FboCubeMapLayered::create(mDestinationCubeMapSide, mDestinationCubeMapSide, FboCubeMapLayered::Format().depth(false));
	mFrameToCubeMapConvertMesh = makeRowLayoutToCubeMapMesh(mDestinationCubeMapSide);
	mFrameToCubeMapConvertShader = gl::GlslProg::create(loadAsset("convertFrameToCubeMap_v.glsl"), loadAsset("convertFrameToCubeMap_f.glsl"), loadAsset("convertFrameToCubeMap_g.glsl"));
	mFrameToCubeMapConvertBatch = gl::Batch::create(mFrameToCubeMapConvertMesh, mFrameToCubeMapConvertShader, { { geom::CUSTOM_0, "faceIndex" } });

	// Set up the sphere mesh projection target
	ObjLoader meshLoader(loadAsset("sphere_scan_2017_03_02/sphere_scan_2017_03_02_edited.obj"));
	TriMesh sphereMesh(meshLoader, TriMesh::Format().positions().normals().texCoords0(2).texCoords1(3));

	// Do some fancy stuff to set up the mesh's 3D texture coordinates
	// TODO: do this in a script and bake these coordinates into the mesh
	sphereMesh.getBufferTexCoords1().resize(sphereMesh.getNumVertices() * 3); // 3-dimensional tex coord

	vec3 MAGIC_SPHERE_ORIGIN(0.00582, 0.31940, -0.01190); // I know this because of magic

	vec3 const * positions = sphereMesh.getPositions<3>();
	vec3 * cubeMapTexCoords = sphereMesh.getTexCoords1<3>();
	for (size_t idx = 0; idx < sphereMesh.getNumVertices(); idx++) {
		cubeMapTexCoords[idx] = normalize(positions[idx] - MAGIC_SPHERE_ORIGIN);
	}

	mScanSphereMesh = gl::VboMesh::create(sphereMesh);
	mScanSphereTexture = gl::Texture::create(loadImage(loadAsset("sphere_scan_2017_03_02/sphere_scan_2017_03_02.png")));

	mProjectorCoverageShader = gl::GlslProg::create(loadAsset("projectorCoverage_v.glsl"), loadAsset("projectorCoverage_f.glsl"));
	mSyphonFrameAsCubeMapRenderShader = gl::GlslProg::create(loadAsset("syphonFrameAsCubeMapRender_v.glsl"), loadAsset("syphonFrameAsCubeMapRender_f.glsl"));
}

void DigitalLifeProjectorControlApp::keyDown(KeyEvent evt) {
	if (evt.getCode() == KeyEvent::KEY_ESCAPE) {
		quit();
	} else if (evt.getCode() == KeyEvent::KEY_n) {
		createNewWindow();
	} else if (evt.getCode() == KeyEvent::KEY_f) {
		setFullScreen(!isFullScreen());
	} else if (evt.getCode() == KeyEvent::KEY_w) {
		getWindow()->close();
	} else if (evt.getCode() == KeyEvent::KEY_m) {
		auto params = getWindow()->getUserData<WindowData>()->mParams;
		params->show(!params->isVisible());
	} else if (evt.getCode() == KeyEvent::KEY_s) {
		vector<WindowData *> winData;
		for (int winIdx = 0; winIdx < getNumWindows(); winIdx++) {
			winData.push_back(getWindowIndex(winIdx)->getUserData<WindowData>());
		}
		saveProjectorParams(this, winData, mParamsFile);
	}
}

void DigitalLifeProjectorControlApp::createNewWindow() {
	JsonTree newWindowParams = mParamsTree.getNumChildren() > getNumWindows() ? mParamsTree.getChild(getNumWindows()) : JsonTree();
	app::WindowRef newWindow = createWindow(Window::Format());
	newWindow->setUserData<WindowData>(new WindowData(mNumWindowsCreated++, newWindow, newWindowParams));
}

void DigitalLifeProjectorControlApp::update()
{
	mLatestFrame = mSyphonClient->fetchFrame();

	// Render the frame onto a cubemap
	{	
		gl::ScopedFramebuffer scpFbo(GL_FRAMEBUFFER, mFrameDestinationCubeMap->getId());

		gl::enableFaceCulling(false);

		gl::ScopedViewport scpView(0, 0, mFrameDestinationCubeMap->getWidth(), mFrameDestinationCubeMap->getHeight());

		gl::ScopedMatrices scpMat;
		// TODO: I'm not actually sure why I need to switch the y-axis here? But it does need to happen to get the tex coords right
		gl::setMatricesWindow(mFrameDestinationCubeMap->getWidth(), mFrameDestinationCubeMap->getHeight(), false);

		gl::clear(Color(0, 0, 0));

		gl::ScopedTextureBind scpTex(mLatestFrame, 0);
		mFrameToCubeMapConvertShader->uniform("uSourceTex", 0);
		mFrameToCubeMapConvertShader->uniform("uSourceTexDims", vec2(mLatestFrame->getWidth(), mLatestFrame->getHeight()));

		mFrameToCubeMapConvertBatch->draw();
	}
}

void DigitalLifeProjectorControlApp::draw()
{
	WindowRef thisWindow = getWindow();
	WindowData * windowUserData = thisWindow->getUserData<WindowData>();

	// Do this in the draw function so that it's enabled for every window
	gl::enableDepth();

	gl::enableFaceCulling(windowUserData->mViewState == ViewState::PROJECTOR_VIEW);

	gl::ScopedViewport scpView(0, 0, thisWindow->getWidth(), thisWindow->getHeight());

	gl::clear(Color(0, 0, 0));

	{
		gl::ScopedMatrices scpMat;

		if (windowUserData->mViewState == ViewState::EXTERNAL_VIEW) {
			gl::setMatrices(windowUserData->mCamera);
		} else if (windowUserData->mViewState == ViewState::PROJECTOR_VIEW) {
			gl::setViewMatrix(windowUserData->mProjector.getViewMatrix());
			gl::setProjectionMatrix(windowUserData->mProjector.getProjectionMatrix());
		}

		if (windowUserData->mSphereRenderType == SphereRenderType::WIREFRAME) {
			gl::ScopedColor scpColor(Color(1, 0, 0));
			gl::ScopedPolygonMode scpPoly(GL_LINE);
			gl::draw(mScanSphereMesh);
		} else if (windowUserData->mSphereRenderType == SphereRenderType::TEXTURE) {
			gl::ScopedGlslProg scpShader(gl::getStockShader(gl::ShaderDef().texture(mScanSphereTexture)));
			gl::ScopedTextureBind scpTex(mScanSphereTexture);
			gl::draw(mScanSphereMesh);
		} else if (windowUserData->mSphereRenderType == SphereRenderType::PROJECTOR_COVERAGE) {
			vector<ProjectorUploadData> projectorList(getNumWindows());
			for (int winIdx = 0; winIdx < getNumWindows(); winIdx++) {
				Projector const & proj = getWindowIndex(winIdx)->getUserData<WindowData>()->mProjector;
				projectorList[winIdx] = ProjectorUploadData(proj.getWorldPos(), proj.getTarget(), proj.getColor());
			}
			gl::UboRef projectorsUbo = gl::Ubo::create(sizeof(ProjectorUploadData) * getNumWindows(), projectorList.data());

			projectorsUbo->bindBufferBase(0);
			mProjectorCoverageShader->uniformBlock("uProjectors", 0);
			mProjectorCoverageShader->uniform("uNumProjectors", (int) getNumWindows());

			gl::ScopedGlslProg scpShader(mProjectorCoverageShader);
			gl::draw(mScanSphereMesh);
		} else if (windowUserData->mSphereRenderType == SphereRenderType::SYPHON_FRAME) {
			// TODO: Make it so that this view renders the combined influence of all projectors on the object (in the external view but not the projector view)
			gl::ScopedGlslProg scpShader(mSyphonFrameAsCubeMapRenderShader);
			mSyphonFrameAsCubeMapRenderShader->uniform("uCubeMapTex", 0);
			mSyphonFrameAsCubeMapRenderShader->uniform("uProjectorPos", windowUserData->mProjector.getWorldPos());
			gl::ScopedTextureBind scpTex(mFrameDestinationCubeMap->getColorTex(), 0);
			gl::draw(mScanSphereMesh);
		}

		if (windowUserData->mViewState == ViewState::EXTERNAL_VIEW) {
			gl::ScopedColor scpColor(Color(0.2, 0.4, 0.8));
			gl::draw(geom::WirePlane().subdivisions(ivec2(10, 10)).size(vec2(10.0, 10.0)));

			for (int winIdx = 0; winIdx < getNumWindows(); winIdx++) {
				getWindowIndex(winIdx)->getUserData<WindowData>()->mProjector.draw();
			}
		}
	}

	windowUserData->mParams->draw();

	// Debug zone
	{
		// gl::drawHorizontalCross(mFrameDestinationCubeMap->getColorTex(), Rectf(0, 0, getWindowWidth(), getWindowHeight()));
		// gl::draw(mLatestFrame, Rectf(0, 0, getWindowWidth(), getWindowHeight()));
	}
}

CINDER_APP( DigitalLifeProjectorControlApp, RendererGl, & DigitalLifeProjectorControlApp::prepSettings )
