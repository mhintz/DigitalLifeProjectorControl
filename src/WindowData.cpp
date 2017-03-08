#include "WindowData.h"

using namespace ci;

SubWindowData::SubWindowData(int id, app::WindowRef theWindow, ProjectorRef projector)
: mId(id), mProjector(projector) {
	theWindow->setTitle("Window " + std::to_string(mId) + " - Projector " + std::to_string(mProjector->getId()));
}