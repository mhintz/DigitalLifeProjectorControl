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

enum class SphereRenderType {
	WIREFRAME,
	TEXTURE,
	SYPHON_FRAME
};

enum class ViewState {
	PROJECTOR_VIEW,
	EXTERNAL_VIEW
};

class WindowData {
public:
	WindowData(WindowRef theWindow) : mId(app::getNumWindows()) {
		mParams = params::InterfaceGl::create(theWindow, "Params", theWindow->toPixels(ivec2(200, theWindow->getHeight() - 20)));
		mParams->hide();
		mCamera.setAspectRatio(theWindow->getAspectRatio());
		mCamera.lookAt(vec3(0, 0, 4), vec3(0), vec3(0, 1, 0));
		mCameraUi = CameraUi(& mCamera, theWindow);
		mProjector = getAcerP5515MinZoom();
		vec3 randColor = glm::rgbColor(vec3(randFloat(360), 0.95, 0.95));
		mProjector.moveTo(vec3(1, 0, randFloat() * 3.14)).setColor(Color(randColor.x, randColor.y, randColor.z));
	}

	int mId;
	SphereRenderType mSphereRenderType = SphereRenderType::WIREFRAME;
	ViewState mViewState = ViewState::EXTERNAL_VIEW;
	CameraPersp mCamera;
	CameraUi mCameraUi;
	Projector mProjector;
	params::InterfaceGlRef mParams;
};

class DigitalLifeProjectorControlApp : public App {
  public:
	static void prepSettings(Settings * settings);

	void setup() override;
	void keyDown(KeyEvent evt) override;
	void update() override;
	void draw() override;

	void createNewWindow();

	ciSyphon::ClientRef mSyphonClient;
	gl::TextureRef mLatestFrame;
	FboCubeMapLayeredRef mFrameDestination;
	gl::VboMeshRef mScanSphereMesh;
	gl::TextureRef mScanSphereTexture;
};

void DigitalLifeProjectorControlApp::prepSettings(Settings * settings) {

}

void DigitalLifeProjectorControlApp::setup() {
	getWindow()->setUserData<WindowData>(new WindowData(getWindow()));

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
	newWindow->setUserData<WindowData>(new WindowData(newWindow));
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

		gl::enableFaceCulling(windowUserData->mViewState == ViewState::PROJECTOR_VIEW);

		if (windowUserData->mSphereRenderType == SphereRenderType::WIREFRAME) {
			gl::ScopedColor scpColor(Color(1, 0, 0));
			gl::ScopedPolygonMode scpPoly(GL_LINE);
			gl::draw(mScanSphereMesh);
		} else if (windowUserData->mSphereRenderType == SphereRenderType::TEXTURE) {
			gl::ScopedTextureBind scpTex(mScanSphereTexture);
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
