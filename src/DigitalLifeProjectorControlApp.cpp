#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class DigitalLifeProjectorControlApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
};

void DigitalLifeProjectorControlApp::setup()
{
}

void DigitalLifeProjectorControlApp::mouseDown( MouseEvent event )
{
}

void DigitalLifeProjectorControlApp::update()
{
}

void DigitalLifeProjectorControlApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP( DigitalLifeProjectorControlApp, RendererGl )
