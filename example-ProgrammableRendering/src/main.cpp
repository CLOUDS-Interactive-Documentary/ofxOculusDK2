#include "testApp.h"
#include "ofGLProgrammableRenderer.h"

//--------------------------------------------------------------
int main() {
    ofSetCurrentRenderer(ofGLProgrammableRenderer::TYPE);

    ofSetupOpenGL(1200, 800, OF_WINDOW);
    ofRunApp(new testApp()); // start the app
}
