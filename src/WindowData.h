#pragma once

#include "cinder/Json.h"

#include "Projector.h"

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
	WindowData(uint32_t id, ci::app::WindowRef theWindow, ci::JsonTree paramData);

	void setupParamsList();

	uint32_t mId;
	ViewState mViewState = ViewState::PROJECTOR_VIEW;
	SphereRenderType mSphereRenderType = SphereRenderType::SYPHON_FRAME;

	ci::CameraPersp mCamera;
	ci::CameraUi mCameraUi;
	Projector mProjector;
	ci::params::InterfaceGlRef mParams;
};