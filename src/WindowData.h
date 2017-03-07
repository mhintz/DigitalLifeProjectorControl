#pragma once

#include "cinder/Json.h"

#include "Projector.h"

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

	bool isMainWindow() { return false; }

	int mId;

	SphereRenderType mSphereRenderType = SphereRenderType::SYPHON_FRAME;

	ProjectorRef mProjector;
};