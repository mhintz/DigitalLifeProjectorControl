#include "WindowData.h"

using namespace ci;

vec3 parseVector(JsonTree vec) {
	return vec3(vec.getValueAtIndex<float>(0), vec.getValueAtIndex<float>(1), vec.getValueAtIndex<float>(2));
}

Color parseColor(JsonTree color) {
	return Color(color.getValueAtIndex<float>(0), color.getValueAtIndex<float>(1), color.getValueAtIndex<float>(2));
}

Projector parseProjectorParams(JsonTree const & params) {
	return Projector()
		.setHorFOV(params.getValueForKey<float>("horFOV"))
		.setVertFOV(params.getValueForKey<float>("vertFOV"))
		.setVertBaseAngle(params.getValueForKey<float>("baseAngle"))
		.moveTo(parseVector(params.getChild("position")))
		.setUpsideDown(params.getValueForKey<bool>("isUpsideDown"))
		.setYRotation(params.getValueForKey<float>("yRotation"))
		.setColor(parseColor(params.getChild("color")));
}

WindowData::WindowData(uint32_t id, app::WindowRef theWindow, JsonTree paramData) {
	mId = id;

	mParams = params::InterfaceGl::create(theWindow, "Params", theWindow->toPixels(ivec2(200, theWindow->getHeight() - 20)));

	mParams->hide();

	setupParamsList();

	mCamera.setAspectRatio(theWindow->getAspectRatio());
	mCamera.lookAt(vec3(0, 0, 4), vec3(0), vec3(0, 1, 0));
	mCameraUi = CameraUi(& mCamera, theWindow);

	if (paramData.hasChildren()) {
		mProjector = parseProjectorParams(paramData);
	} else {
		mProjector = getAcerP5515MinZoom();
		vec3 randColor = glm::rgbColor(vec3(randFloat(360), 0.95, 0.95));
		mProjector.moveTo(vec3(2, 0, randFloat() * 6.28)).setColor(Color(randColor.x, randColor.y, randColor.z));
	}
}

void WindowData::setupParamsList() {
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

	mParams->addSeparator();

	mParams->addText("Projector " + std::to_string(mId));

	// Dividing by 10 makes the positioning more precise by that factor
	// (precision and step functions don't work for this control)
	mParams->addParam<vec3>("Position",
		[this] (vec3 projPos) {
			mProjector.moveTo(projPos / 10.0f); },
		[this] () {
			return mProjector.getPos() * 10.0f;
		});

	mParams->addParam<bool>("Flipped",
		[this] (bool isFlipped) {
			mProjector.setUpsideDown(isFlipped); },
		[this] () {
			return mProjector.getUpsideDown();
		});

	mParams->addParam<float>("Y Rotation",
		[this] (float rotation) {
			mProjector.setYRotation(rotation); },
		[this] () {
			return mProjector.getYRotation();
		}).min(-M_PI / 2).max(M_PI / 2).precision(4).step(0.001f);

	mParams->addParam<float>("Horizontal FoV",
		[this] (float fov) {
			mProjector.setHorFOV(fov);
		}, [this] () {
			return mProjector.getHorFOV();
		}).min(M_PI / 16.0f).max(M_PI * 3.0 / 4.0).precision(4).step(0.001f);

	mParams->addParam<float>("Vertical FoV",
		[this] (float fov) {
			mProjector.setVertFOV(fov);
		}, [this] () {
			return mProjector.getVertFOV();
		}).min(M_PI / 16.0f).max(M_PI * 3.0 / 4.0).precision(4).step(0.001f);

	mParams->addParam<float>("Vertical Offset Angle",
		[this] (float angle) {
			mProjector.setVertBaseAngle(angle);
		}, [this] () {
			return mProjector.getVertBaseAngle();
		}).min(0.0f).max(M_PI / 2.0f).precision(4).step(0.001f);

	mParams->addParam<Color>("Color",
		[this] (Color color) {
			mProjector.setColor(color);
		}, [this] () {
			return mProjector.getColor();
		});
}