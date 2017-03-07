#pragma once

#include <string>

#include "cinder/app/App.h"

#include "Projector.h"

ci::JsonTree loadProjectorParams(ci::app::App * theApp, std::string paramFileName);
Projector parseProjectorParams(ci::JsonTree const & params);
ci::JsonTree serializeProjector(Projector const & proj);
void saveProjectorParams(ci::app::App * theApp, std::vector<ProjectorRef> const & theData, std::string paramFileName);