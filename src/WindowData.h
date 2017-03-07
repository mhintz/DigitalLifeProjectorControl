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

class BaseWindowData {
public:
	virtual bool isMainWindow() = 0;
};

class MainWindowData : public BaseWindowData {
public:
	bool isMainWindow() { return true; }
};

class SubWindowData : public BaseWindowData {
public:
	SubWindowData(int id, ci::app::WindowRef theWindow, ProjectorRef projector);

	void setupParamsList();

	bool isMainWindow() { return false; }

	int mId;
	ViewState mViewState = ViewState::PROJECTOR_VIEW;
	SphereRenderType mSphereRenderType = SphereRenderType::SYPHON_FRAME;

	ci::CameraPersp mCamera;
	ci::CameraUi mCameraUi;
	ProjectorRef mProjector;
	ci::params::InterfaceGlRef mParams;
};