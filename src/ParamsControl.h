#pragma once

#include <string>

#include "cinder/app/App.h"

#include "Projector.h"
#include "WindowData.h"

ci::JsonTree loadProjectorParams(ci::app::App * theApp, std::string paramFileName);
void saveProjectorParams(ci::app::App * theApp, std::vector<WindowData *> const & theData, std::string paramFileName);