#include <vector>

#include <glm/gtx/color_space.hpp>

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/TriMesh.h"
#include "cinder/ObjLoader.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"

#include "Syphon.h"

#include "Projector.h"
#include "FboCubeMapLayered.h"

using namespace ci;
using namespace ci::app;
using std::vector;

enum class ViewState {
	EXTERNAL_VIEW,
	PROJECTOR_VIEW
};

enum class SphereRenderType {
	WIREFRAME,
	TEXTURE,
	PROJECTOR_COVERAGE,
	SYPHON_FRAME
};

class WindowData {
public:
	WindowData(uint32_t id, WindowRef theWindow) : mId(id) {
		mParams = params::InterfaceGl::create(theWindow, "Params", theWindow->toPixels(ivec2(200, theWindow->getHeight() - 20)));

		mParams->hide();

		mParams->addParam("View Mode", {
			"External View",
			"Projector View"
		}, [this] (int viewMode) {
			switch (viewMode) {
				case 0: mViewState = ViewState::EXTERNAL_VIEW; break;
				case 1: mViewState = ViewState::PROJECTOR_VIEW; break;
				default: throw std::invalid_argument("invalid view mode parameter");
			}
		}, [this] () {
			switch (mViewState) {
				case ViewState::EXTERNAL_VIEW: return 0;
				case ViewState::PROJECTOR_VIEW: return 1;
			}
		});

		mParams->addParam("Render Mode", {
			"Wireframe",
			"Texture",
			"Projector Coverage",
			"Syphon Frame"
		}, [this] (int renderMode) {
			switch (renderMode) {
				case 0: mSphereRenderType = SphereRenderType::WIREFRAME; break;
				case 1: mSphereRenderType = SphereRenderType::TEXTURE; break;
				case 2: mSphereRenderType = SphereRenderType::PROJECTOR_COVERAGE; break;
				case 3: mSphereRenderType = SphereRenderType::SYPHON_FRAME; break;
				default: throw std::invalid_argument("invalid render mode parameter");
			}
		}, [this] () {
			switch (mSphereRenderType) {
				case SphereRenderType::WIREFRAME : return 0;
				case SphereRenderType::TEXTURE : return 1;
				case SphereRenderType::PROJECTOR_COVERAGE : return 2;
				case SphereRenderType::SYPHON_FRAME : return 3;
			}
		});

		mCamera.setAspectRatio(theWindow->getAspectRatio());
		mCamera.lookAt(vec3(0, 0, 4), vec3(0), vec3(0, 1, 0));
		mCameraUi = CameraUi(& mCamera, theWindow);
		mProjector = getAcerP5515MinZoom();
		vec3 randColor = glm::rgbColor(vec3(randFloat(360), 0.95, 0.95));
		mProjector
			.moveTo(vec3(1, 0, randFloat() * 3.14))
			.setColor(Color(randColor.x, randColor.y, randColor.z));
	}

	uint32_t mId;
	ViewState mViewState = ViewState::EXTERNAL_VIEW;
	SphereRenderType mSphereRenderType = SphereRenderType::PROJECTOR_COVERAGE;

	CameraPersp mCamera;
	CameraUi mCameraUi;
	Projector mProjector;
	params::InterfaceGlRef mParams;
};

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

	ciSyphon::ClientRef mSyphonClient;
	gl::TextureRef mLatestFrame;
	FboCubeMapLayeredRef mFrameDestination;

	gl::VboMeshRef mScanSphereMesh;
	gl::TextureRef mScanSphereTexture;
	gl::GlslProgRef mProjectorCoverageShader;
};

void DigitalLifeProjectorControlApp::prepSettings(Settings * settings) {

}

void DigitalLifeProjectorControlApp::setup() {
	getWindow()->setUserData<WindowData>(new WindowData(mNumWindowsCreated++, getWindow()));

	mSyphonClient = ciSyphon::Client::create();
	mSyphonClient->set("DigitalLifeServer", "DigitalLifeClient");
	mSyphonClient->setup();

	mFrameDestination = FboCubeMapLayered::create(1600, 1600, FboCubeMapLayered::Format().depth(false));

	// Set up the sphere mesh projection target
	ObjLoader meshLoader(loadAsset("sphere_scan_2017_03_02/sphere_scan_2017_03_02_edited.obj"));
	TriMesh sphereMesh(meshLoader, TriMesh::Format().positions().normals().texCoords0(2).texCoords1(3));

	// Do some fancy stuff to set up the mesh's 3D texture coordinates
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
	}
}

void DigitalLifeProjectorControlApp::createNewWindow() {
	app:WindowRef newWindow = createWindow(Window::Format());
	newWindow->setUserData<WindowData>(new WindowData(mNumWindowsCreated++, newWindow));
}

void DigitalLifeProjectorControlApp::update()
{
	mLatestFrame = mSyphonClient->fetchFrame();

	// Wrap the frame to the cubemap
}

void DigitalLifeProjectorControlApp::draw()
{
	WindowRef thisWindow = getWindow();
	WindowData * windowUserData = thisWindow->getUserData<WindowData>();

	gl::enableDepth();

	gl::enableFaceCulling(windowUserData->mViewState == ViewState::PROJECTOR_VIEW);

	gl::ScopedViewport scpView(0, 0, thisWindow->getWidth(), thisWindow->getHeight());

	gl::ScopedMatrices scpMat;
	gl::setMatricesWindow(thisWindow->getWidth(), thisWindow->getHeight());

	{
		gl::ScopedMatrices innerScope;

		if (windowUserData->mViewState == ViewState::EXTERNAL_VIEW) {
			gl::setMatrices(windowUserData->mCamera);
		} else if (windowUserData->mViewState == ViewState::PROJECTOR_VIEW) {
			gl::setViewMatrix(windowUserData->mProjector.getViewMatrix());
			gl::setProjectionMatrix(windowUserData->mProjector.getProjectionMatrix());
		}

		gl::clear(Color(0, 0, 0));

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
		// gl::draw(mLatestFrame, Rectf(0, 0, getWindowWidth(), getWindowHeight()));
	}
}

CINDER_APP( DigitalLifeProjectorControlApp, RendererGl, & DigitalLifeProjectorControlApp::prepSettings )
