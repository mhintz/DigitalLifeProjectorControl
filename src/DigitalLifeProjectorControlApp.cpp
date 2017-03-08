#include <memory>
#include <map>
#include <algorithm>

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
	// App functions
	static void prepSettings(Settings * settings);

	void setup() override;
	void update() override;
	void draw() override;
	void keyDown(KeyEvent evt) override;

	// Drawing commands
	void drawSphere(SphereRenderType sphereType);
	void renderProjectorAlignmentView();

	// Main params list function
	void setupViewParams(params::InterfaceGlRef theParams);

	// Window and projector data management and helpers
	void createNewWindow();
	void closeThisWindow();
	ProjectorRef getProjectorForWindow(int windowId);
	vector<SubWindowData *> getSubWindowDataVec();

	int mNumWindowsCreated = 0;
	uint32_t mDestinationCubeMapSide = 1600;
	string mParamsFile = "projectorControlParams.json";

	// Params and windows management
	JsonTree mParamsTree;
	vector<ProjectorRef> mProjectorParams;
	std::map<int, int> mProjectorWindowMap;
	params::InterfaceGlRef mMenu;

	// Main window render stuff
	SphereRenderType mSphereRenderType = SphereRenderType::TEXTURE;
	ci::CameraPersp mCamera;
	ci::CameraUi mCameraUi;

	// Syphon input
	ciSyphon::ClientRef mSyphonClient;
	gl::TextureRef mLatestFrame;
	FboCubeMapLayeredRef mFrameDestinationCubeMap;
	gl::VboMeshRef mFrameToCubeMapConvertMesh;
	gl::GlslProgRef mFrameToCubeMapConvertShader;
	gl::BatchRef mFrameToCubeMapConvertBatch;

	// Objects for rendering
	gl::VboMeshRef mScanSphereMesh;
	gl::TextureRef mScanSphereTexture;
	gl::GlslProgRef mProjectorCoverageShader;
	gl::GlslProgRef mSyphonFrameAsCubeMapRenderShader_projector;
	gl::GlslProgRef mSyphonFrameAsCubeMapRenderShader_external;
};

void DigitalLifeProjectorControlApp::prepSettings(Settings * settings) {
	settings->setTitle("Main Config Window");
	settings->setWindowSize(1200, 800);
}

void DigitalLifeProjectorControlApp::setup() {
	getWindow()->setUserData<MainWindowData>(new MainWindowData());

	mParamsTree = loadProjectorParams(this, mParamsFile);

	for (int projIdx = 0; projIdx < mParamsTree.getNumChildren(); projIdx++) {
		mProjectorParams.push_back(ProjectorRef(new Projector(parseProjectorParams(mParamsTree.getChild(projIdx)))));
		// Put the projector ID into the projector-window map without a window assigned
		mProjectorWindowMap[mProjectorParams.back()->getId()] = -1;
	}

	mMenu = params::InterfaceGl::create(getWindow(), "Params", toPixels(ivec2(400, getWindowHeight() - 40)));

	setupViewParams(mMenu);

	mCamera.setAspectRatio(getWindowAspectRatio());
	mCamera.lookAt(vec3(0, 0, 4), vec3(0), vec3(0, 1, 0));
	mCameraUi = CameraUi(& mCamera, getWindow());

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
	auto syphonFrameOnModel_v = loadAsset("syphonFrameAsCubeMapRender_v.glsl");
	auto syphonFrameOnModel_f = loadAsset("syphonFrameAsCubeMapRender_f.glsl");
	mSyphonFrameAsCubeMapRenderShader_projector = gl::GlslProg::create(
		gl::GlslProg::Format()
			.vertex(syphonFrameOnModel_v).fragment(syphonFrameOnModel_f)
	);
	mSyphonFrameAsCubeMapRenderShader_external = gl::GlslProg::create(
		gl::GlslProg::Format()
			.vertex(syphonFrameOnModel_v).fragment(syphonFrameOnModel_f).define("EXTERNAL_VIEW")
	);
}

void DigitalLifeProjectorControlApp::keyDown(KeyEvent evt) {
	if (evt.getCode() == KeyEvent::KEY_ESCAPE) {
		quit();
	} else if (evt.getCode() == KeyEvent::KEY_n) {
		createNewWindow();
	} else if (evt.getCode() == KeyEvent::KEY_f) {
		setFullScreen(!isFullScreen());
	} else if (evt.getCode() == KeyEvent::KEY_w) {
		closeThisWindow();
	} else if (evt.getCode() == KeyEvent::KEY_m) {
		mMenu->show(!mMenu->isVisible());
	} else if (evt.getCode() == KeyEvent::KEY_s) {
		saveProjectorParams(this, mProjectorParams, mParamsFile);
	}
}

ProjectorRef DigitalLifeProjectorControlApp::getProjectorForWindow(int windowId) {
	// Find the first projector ID that matches the provided window id
	auto projMapPosition = std::find_if(mProjectorWindowMap.begin(), mProjectorWindowMap.end(), [=] (std::pair<int, int> const & element) { return element.second == windowId; });
	// Find the projector reference in the array of projector refs based on the returned projector map position
	auto projVecPosition = std::find_if(mProjectorParams.begin(), mProjectorParams.end(), [=] (ProjectorRef const & proj) { return proj->getId() == projMapPosition->first; });
	return * projVecPosition;
}

vector<SubWindowData *> DigitalLifeProjectorControlApp::getSubWindowDataVec() {
	vector<SubWindowData *> dataVec;
	for (int winIdx = 1; winIdx < getNumWindows(); winIdx++) {
		dataVec.push_back(getWindowIndex(winIdx)->getUserData<SubWindowData>());
	}
	std::sort(dataVec.begin(), dataVec.end(), [] (SubWindowData * winA, SubWindowData * winB) { return winA->mProjector->getId() < winB->mProjector->getId(); });
	return std::move(dataVec);
}

void DigitalLifeProjectorControlApp::createNewWindow() {
	ProjectorRef newWindowProj;
	if (mProjectorWindowMap.size() > getNumWindows() - 1) { // Subtract 1 for the main window
		// Find the first projector ID that has no window assigned
		newWindowProj = getProjectorForWindow(-1);
	} else {
		// Create a new projector config
		// Note: the only way to "delete" a projector from the config is to manually edit the saved params file
		newWindowProj = std::make_shared<Projector>(getAcerP5515MinZoom());
		// Random position and color, just for initialization
		vec3 randColor = glm::rgbColor(vec3(randFloat(360), 0.95, 0.95));
		newWindowProj->moveTo(vec3(2, 0, randFloat() * 6.28))
			.setColor(Color(randColor.x, randColor.y, randColor.z))
			.setId(mProjectorParams.size()); // Assign the projector an ID corresponding to its eventual position in the projectors array

		// Add the projector to the app's stored data
		mProjectorParams.push_back(newWindowProj);
	}
	// mNumWindowsCreated is the window unique ID. It always increases, unlike getNumWindows()
	mProjectorWindowMap[newWindowProj->getId()] = mNumWindowsCreated;
	app::WindowRef newWindow = createWindow(Window::Format());
	newWindow->setUserData<SubWindowData>(new SubWindowData(mNumWindowsCreated, newWindow, newWindowProj));
	mNumWindowsCreated += 1;
	setupViewParams(mMenu);
}

void DigitalLifeProjectorControlApp::closeThisWindow() {
	WindowRef theWindow = getWindow();
	if (theWindow->getUserData<BaseWindowData>()->isMainWindow()) {
		// If the main window is closed, close the entire app
		quit();
	} else {
		// If a subwindow is closed, decouple the associated projector from the subwindow assignment
		// But the projector stays in the array of projectors, and its id stays in the map
		// The only way to delete a projector once it's been saved to the params file is to manually edit the params file
		SubWindowData * windowUserData = theWindow->getUserData<SubWindowData>();

		mProjectorWindowMap[windowUserData->mProjector->getId()] = -1;

		theWindow->close();

		setupViewParams(mMenu);
	}
}

void DigitalLifeProjectorControlApp::update()
{
	mLatestFrame = mSyphonClient->fetchFrame();

	// Render the frame onto a cubemap
	{	
		gl::ScopedFramebuffer scpFbo(GL_FRAMEBUFFER, mFrameDestinationCubeMap->getId());

		gl::ScopedFaceCulling scpCull(false);

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

	// Do this in the draw function so that it's enabled for every window
	gl::enableDepth();

	gl::ScopedViewport scpView(0, 0, thisWindow->getWidth(), thisWindow->getHeight());

	gl::clear(Color(0, 0, 0));

	if (thisWindow->getUserData<BaseWindowData>()->isMainWindow()) {
		// Rendering for the main window

		gl::ScopedFaceCulling scpCull(false);

		{
			gl::ScopedMatrices scpMat;

			gl::setMatrices(mCamera);

			drawSphere(mSphereRenderType);

			gl::ScopedColor scpColor(Color(0.2, 0.4, 0.8));
			gl::draw(geom::WirePlane().subdivisions(ivec2(10, 10)).size(vec2(10.0, 10.0)));

			for (auto winData : getSubWindowDataVec()) {
				winData->mProjector->draw();
			}

		}

		mMenu->draw();

		// Debug zone
		{
			gl::drawString(std::to_string(getAverageFps()), vec2(getWindowWidth() - 100.0f, getWindowHeight() - 30.0f), ColorA(1.0f, 1.0f, 1.0f, 1.0f));

			// gl::drawHorizontalCross(mFrameDestinationCubeMap->getColorTex(), Rectf(0, 0, getWindowWidth(), getWindowHeight()));
			// gl::draw(mLatestFrame, Rectf(0, 0, getWindowWidth(), getWindowHeight()));
		}
	} else {
		// Rendering for the sub windows

		SubWindowData * windowUserData = thisWindow->getUserData<SubWindowData>();

		if (!windowUserData) {
			console() << "ERROR: window has no data to be used for rendering" << std::endl;
			return;
		}

		gl::ScopedFaceCulling scpCull(true, GL_BACK);

		if (windowUserData->mRenderArrow) {
			renderProjectorAlignmentView();
		} else {
			gl::ScopedMatrices scpMat;

			gl::setViewMatrix(windowUserData->mProjector->getViewMatrix());
			gl::setProjectionMatrix(windowUserData->mProjector->getProjectionMatrix());

			drawSphere(windowUserData->mSphereRenderType);			
		}
	}
}

void DigitalLifeProjectorControlApp::drawSphere(SphereRenderType sphereType) {
	// Draw the sphere itself
	if (sphereType == SphereRenderType::WIREFRAME) {
		gl::ScopedColor scpColor(Color(1, 0, 0));
		gl::ScopedPolygonMode scpPoly(GL_LINE);
		gl::draw(mScanSphereMesh);
	} else if (sphereType == SphereRenderType::TEXTURE) {
		gl::ScopedGlslProg scpShader(gl::getStockShader(gl::ShaderDef().texture(mScanSphereTexture)));
		gl::ScopedTextureBind scpTex(mScanSphereTexture);
		gl::draw(mScanSphereMesh);
	} else if (sphereType == SphereRenderType::PROJECTOR_COVERAGE) {
		vector<ProjectorUploadData> projectorList;
		for (auto winData : getSubWindowDataVec()) {
			projectorList.emplace_back(winData->mProjector->getWorldPos(), winData->mProjector->getTarget(), winData->mProjector->getColor());
		}
		gl::UboRef projectorsUbo = gl::Ubo::create(sizeof(ProjectorUploadData) * projectorList.size(), projectorList.data());

		projectorsUbo->bindBufferBase(0);
		mProjectorCoverageShader->uniformBlock("uProjectors", 0);
		mProjectorCoverageShader->uniform("uNumProjectors", (int) projectorList.size());

		gl::ScopedGlslProg scpShader(mProjectorCoverageShader);
		gl::draw(mScanSphereMesh);
	} else if (sphereType == SphereRenderType::SYPHON_FRAME) {
		if (getWindow()->getUserData<BaseWindowData>()->isMainWindow()) {
			vector<ProjectorUploadData> projectorList;
			for (auto winData : getSubWindowDataVec()) {
				projectorList.emplace_back(winData->mProjector->getWorldPos(), winData->mProjector->getTarget(), winData->mProjector->getColor());
			}
			gl::UboRef projectorsUbo = gl::Ubo::create(sizeof(ProjectorUploadData) * projectorList.size(), projectorList.data());

			projectorsUbo->bindBufferBase(0);
			mSyphonFrameAsCubeMapRenderShader_external->uniformBlock("uProjectors", 0);
			mSyphonFrameAsCubeMapRenderShader_external->uniform("uNumProjectors", (int) projectorList.size());

			gl::ScopedGlslProg scpShader(mSyphonFrameAsCubeMapRenderShader_external);

			mSyphonFrameAsCubeMapRenderShader_external->uniform("uCubeMapTex", 0);
			gl::ScopedTextureBind scpTex(mFrameDestinationCubeMap->getColorTex(), 0);

			gl::draw(mScanSphereMesh);
		} else {
			gl::ScopedGlslProg scpShader(mSyphonFrameAsCubeMapRenderShader_projector);
			mSyphonFrameAsCubeMapRenderShader_projector->uniform("uCubeMapTex", 0);
			mSyphonFrameAsCubeMapRenderShader_projector->uniform("uProjectorPos", getWindow()->getUserData<SubWindowData>()->mProjector->getWorldPos());
			gl::ScopedTextureBind scpTex(mFrameDestinationCubeMap->getColorTex(), 0);
			gl::draw(mScanSphereMesh);
		}
	}
}

void drawRectInPlace(float thickness, float length) {
	gl::drawSolidRect(Rectf(-thickness, -thickness, length + thickness, thickness));
}

void drawArrow(float thickness, float length, bool pointsRight=true) {
	float offset = sqrt(length * length / 2);

	gl::pushModelMatrix();
		if (pointsRight) { gl::translate(-offset, -offset); }
		gl::rotate(M_PI / 4);
		drawRectInPlace(thickness, length);
	gl::popModelMatrix();
	gl::pushModelMatrix();
		if (pointsRight) { gl::translate(-offset, offset); }
		gl::rotate(-M_PI / 4);
		drawRectInPlace(thickness, length);
	gl::popModelMatrix();
}

void DigitalLifeProjectorControlApp::renderProjectorAlignmentView() {
	gl::ScopedDepth scpDepth(false);

	gl::ScopedMatrices scpMat;
	gl::setMatricesWindow(getWindowSize());

	{
		gl::ScopedColor scpColor(1, 0, 0);
		gl::drawSolidRect(Rectf(0, 0, getWindowWidth(), getWindowHeight()));
	}

	{
		vec2 size = vec2(getWindowSize());
		gl::ScopedColor scpColor(Color(1, 1, 1));

		float diagonalAngle = atan2(size.y, size.x);
		float diagonalLength = length(size);
		float diagonalThickness = 2;
		int numTicks = 20;
		float tickLength = 25;

		gl::pushModelMatrix();

			gl::translate(vec2(0, 0));
			gl::rotate(diagonalAngle);
			drawRectInPlace(diagonalThickness, diagonalLength);

			for (int i = 0; i < numTicks; i++) {
				gl::pushModelMatrix();
					float tval = (float) i / (float) (numTicks - 1);
					vec2 position = lerp(vec2(0, 0), vec2(diagonalLength, 0), tval);
					gl::translate(position);
					drawArrow(diagonalThickness, tickLength, tval <= 0.5);
				gl::popModelMatrix();
			}

		gl::popModelMatrix();

		gl::pushModelMatrix();

			gl::translate(vec2(0, size.y));
			gl::rotate(-diagonalAngle);

			drawRectInPlace(diagonalThickness, diagonalLength);

			for (int i = 0; i < numTicks; i++) {
				gl::pushModelMatrix();
					float tval = (float) i / (float) (numTicks - 1);
					vec2 position = lerp(vec2(0, 0), vec2(diagonalLength, 0), tval);
					gl::translate(position);
					drawArrow(diagonalThickness, tickLength, tval <= 0.5);
				gl::popModelMatrix();
			}

		gl::popModelMatrix();
	}
}

void DigitalLifeProjectorControlApp::setupViewParams(params::InterfaceGlRef theParams) {
	// Clear the params first - this makes this function idempotent, so it can be called whenever a window is added or removed
	theParams->clear();

	theParams->addParam("Main View Render Mode", {
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
			default: console() << "ERROR: invalid render mode parameter provided" << std::endl; break;
		}
	}, [this] () {
		switch (mSphereRenderType) {
			case SphereRenderType::WIREFRAME : return 0;
			case SphereRenderType::TEXTURE : return 1;
			case SphereRenderType::PROJECTOR_COVERAGE : return 2;
			case SphereRenderType::SYPHON_FRAME : return 3;
		}
	});

	// Set up a params group for each (extra) window the app currently has open (starting at 1 because 0 is the main window)
	for (auto windowData : getSubWindowDataVec()) {
		// I really really hope C++ is smart enough to correctly destroy copies of this projector
		// reference once the functions which capture it are deleted by InterfaceGl::clear()
		// (I also hope InterfaceGl::clear() is smart enough to destroy all attached function objects)
		// (I'm pretty sure both are the case)
		ProjectorRef theProjector = getProjectorForWindow(windowData->mId);

		// Just a lil sanity check
		assert(theProjector == windowData->mProjector);

		string pname = "P" + std::to_string(windowData->mProjector->getId());

		theParams->addSeparator();

		theParams->addText("Projector " + std::to_string(windowData->mProjector->getId()) + " - Window " + std::to_string(windowData->mId));

		theParams->addParam(pname + " Render Mode", {
			"Wireframe",
			"Texture",
			"Projector Coverage",
			"Syphon Frame",
			"Projector Alignment"
		}, [windowData] (int renderMode) {
			if (renderMode == 4) {
				windowData->mRenderArrow = true;
			} else {
				windowData->mRenderArrow = false;
			}
			switch (renderMode) {
				case 0: windowData->mSphereRenderType = SphereRenderType::WIREFRAME; break;
				case 1: windowData->mSphereRenderType = SphereRenderType::TEXTURE; break;
				case 2: windowData->mSphereRenderType = SphereRenderType::PROJECTOR_COVERAGE; break;
				case 3: windowData->mSphereRenderType = SphereRenderType::SYPHON_FRAME; break;
				case 4: break;
				default: app::console() << "ERROR: invalid render mode parameter provided" << std::endl; break;
			}
		}, [windowData] () {
			if (windowData->mRenderArrow) { return 4; }
			switch (windowData->mSphereRenderType) {
				case SphereRenderType::WIREFRAME : return 0;
				case SphereRenderType::TEXTURE : return 1;
				case SphereRenderType::PROJECTOR_COVERAGE : return 2;
				case SphereRenderType::SYPHON_FRAME : return 3;
			}
		});

		// Dividing by 10 makes the positioning more precise by that factor
		// (precision and step functions don't work for this control)
		theParams->addParam<vec3>(pname + " Position",
			[theProjector] (vec3 projPos) {
				theProjector->moveTo(projPos / 10.0f);
			}, [theProjector] () {
				return theProjector->getPos() * 10.0f;
			});

		theParams->addParam<bool>(pname + " Flipped",
			[theProjector] (bool isFlipped) {
				theProjector->setUpsideDown(isFlipped);
			}, [theProjector] () {
				return theProjector->getUpsideDown();
			});

		theParams->addParam<float>(pname + " Y Rotation",
			[theProjector] (float rotation) {
				theProjector->setYRotation(rotation);
			}, [theProjector] () {
				return theProjector->getYRotation();
			}).min(-M_PI / 2).max(M_PI / 2).precision(4).step(0.0002f);

		theParams->addParam<float>(pname + " Horizontal FoV",
			[theProjector] (float fov) {
				theProjector->setHorFOV(fov);
			}, [theProjector] () {
				return theProjector->getHorFOV();
			}).min(M_PI / 16.0f).max(M_PI * 3.0 / 4.0).precision(4).step(0.001f);

		theParams->addParam<float>(pname + " Vertical FoV",
			[theProjector] (float fov) {
				theProjector->setVertFOV(fov);
			}, [theProjector] () {
				return theProjector->getVertFOV();
			}).min(M_PI / 16.0f).max(M_PI * 3.0 / 4.0).precision(4).step(0.001f);

		theParams->addParam<float>(pname + " Vertical Offset Angle",
			[theProjector] (float angle) {
				theProjector->setVertBaseAngle(angle);
			}, [theProjector] () {
				return theProjector->getVertBaseAngle();
			}).min(0.0f).max(M_PI / 2.0f).precision(4).step(0.001f);

		theParams->addParam<Color>(pname + " Color",
			[theProjector] (Color color) {
				theProjector->setColor(color);
			}, [theProjector] () {
				return theProjector->getColor();
			});
	}
}

CINDER_APP( DigitalLifeProjectorControlApp, RendererGl, & DigitalLifeProjectorControlApp::prepSettings )
