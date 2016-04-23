//
//  ofxOculusDK2.cpp
//  OculusRiftRendering
//
//  Created by Andreas Müller on 30/04/2013.
//  Updated by James George September 27th 2013
//  Updated by Jason Walters October 22 2013
//  Adapted to DK2 by James George and Elie Zananiri August 2014
//  Updated for DK2 by Matt Ebb October 2014


#include "ofxOculusDK2.h"


ofQuaternion toOf(const ovrQuatf& q) {
    return ofQuaternion(q.x, q.y, q.z, q.w);
}

ofMatrix4x4 toOf(const ovrMatrix4f& m) {
    return ofMatrix4x4(m.M[0][0], m.M[1][0], m.M[2][0], m.M[3][0],
        m.M[0][1], m.M[1][1], m.M[2][1], m.M[3][1],
        m.M[0][2], m.M[1][2], m.M[2][2], m.M[3][2],
        m.M[0][3], m.M[1][3], m.M[2][3], m.M[3][3]);
}

ovrMatrix4f toOVR(const ofMatrix4x4& m) {
    const float* cm = m.getPtr();
    ovrMatrix4f om;
    om.M[0][0] = cm[0]; om.M[1][0] = cm[1]; om.M[2][0] = cm[2]; om.M[3][0] = cm[3];
    om.M[0][1] = cm[4]; om.M[1][1] = cm[5]; om.M[2][1] = cm[6]; om.M[3][1] = cm[7];
    om.M[0][2] = cm[8]; om.M[1][2] = cm[9]; om.M[2][2] = cm[10]; om.M[3][2] = cm[11];
    om.M[0][3] = cm[12]; om.M[1][3] = cm[13]; om.M[2][3] = cm[14]; om.M[3][3] = cm[15];
    return om;
}

ofRectangle toOf(const ovrRecti& vp) {
    return ofRectangle(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
}

ovrRecti toOVR(const ofRectangle& vp) {
	ovrRecti r;
	r.Pos.x = vp.x;
	r.Pos.y = vp.y;
	r.Size.w = vp.width;
	r.Size.h = vp.height;
    return r;
}

ofVec3f toOf(const ovrVector3f& v) {
    return ofVec3f(v.x, v.y, v.z);
}

ovrVector3f toOVR(const ofVec3f& v) {
    ovrVector3f ov;
    ov.x = v.x;
    ov.y = v.y;
    ov.z = v.z;
    return ov;
}

ovrVector2i toOVRVector2i(const int x, const int y) {
    ovrVector2i v;
    v.x = x;
    v.y = y;
    return v;
}

ovrSizei toOVRSizei(const int w, const int h) {
    ovrSizei s;
    s.w = w;
    s.h = h;
    return s;
}

ofxOculusDK2::ofxOculusDK2() {
    //memset(&sceneLayer, 0, sizeof(sceneLayer));
    	
	mirrorTexture = nullptr;
	isVisible = true;
	isFadedDown = false;
	baseCamera = nullptr;
	lockView = false;
	oculusScreenSpaceScale = 2.0f;

	//JG added default values
    bSetup = false;
    insideFrame = false;
	isFading = false;
	fadeColor = ofFloatColor(0.,0.,0.,1.);
	fadeAmt = 0;

	pixelDensity = 1.0;

    bPositionTracking = true;

    // distortion caps
    bSRGB = true;
    bHqDistortion = true;

    bUseBackground = false;
    bUseOverlay = false;
    overlayZDistance = -200;

    session = nullptr;
    frameIndex = 0;;
}

ofxOculusDK2::~ofxOculusDK2() {
    if (bSetup) {

		if (mirrorFBO) glDeleteFramebuffers(1, &mirrorFBO);
		if (mirrorTexture) ovr_DestroyMirrorTexture(session, mirrorTexture);
		
		delete eyeLayer;

		if(backgroundLayer)
			delete backgroundLayer;

		if(transitionLayer != nullptr)
			delete transitionLayer;

		//Platform.ReleaseDevice();
		ovr_Destroy(session);
		
	    ovr_Shutdown();

		session = nullptr;
        bSetup = false;
    }
}

/*
//TODO: Reimplement this for window resizing
void ofxOculusDK2::initializeMirrorTexture( ovrSizei size )
{
	if( mirrorTexture ){
		destroyMirrorTexture();
	}

	ovr_CreateMirrorTextureGL( session, GL_SRGB8_ALPHA8, size.w, size.h, (ovrTexture**)&mirrorTexture );

	glGenFramebuffers( 1, &mirrorFBO );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, mirrorFBO );
	glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTexture->OGL.TexId, 0 );
	glFramebufferRenderbuffer( GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0 );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
}
*/

/*
void ofxOculusDK2::destroyMirrorTexture()
{
	glDeleteFramebuffers( 1, &mirrorFBO );
	ovr_DestroyMirrorTexture( session, (ovrTexture*)mirrorTexture );
}
*/

bool ofxOculusDK2::setup() {

    if (bSetup) {
        ofLogError("ofxOculusDK2::setup") << "Already set up";
        return false;
    }

    // Oculus HMD & Sensor Initialization
    ovrResult result;
	result = ovr_Initialize(nullptr);
    if(!OVR_SUCCESS(result)){
		ofLogError("ofxOculusDK2::setup") << "Failed to initialize libOVR." << result;
		return false;
	}

    result = ovr_Create(&session, &luid);
    if (!OVR_SUCCESS(result)) {
		ovrErrorInfo info;
		ovr_GetLastErrorInfo(&info);

		ofLogError("ofxOculusDK2::setup") << "Setup failed! " << result << " " << info.ErrorString;
        return false;
    }

	hmdDesc = ovr_GetHmdDesc( session );

    // In Direct App-rendered mode, we can use smaller window size,
    // as it can have its own contents and isn't tied to the buffer.
    ovrSizei wsize;
    wsize.w = ofGetWidth(); 
	wsize.h = ofGetHeight();
    windowSize = wsize; //Sizei(960, 540); avoid rotated output bug.

    //if (!Platform.InitDevice(windowSize.w, windowSize.h, reinterpret_cast<LUID*>(&luid))){
//        ofLogError() << "Couldn't init platform device";
	//}

    // WARNING: Slightly dangerous!
    // We need to disable double buffering when using SDK rendering,
    // Since the oculus SDK takes care of drawing to screen at VSync time
    // If OF tries to swap buffers too, it conflicts and messes up the timing,
    // preventing the Oculus SDK from Vsyncing properly and causing all kinds of judder.

    // OF uses GLFW by default on PC-ish platforms, so we assume it here.
    // If it's not using GLFW, maybe this might cause a nasty crash :)
    
	//((ofAppGLFWWindow*)ofGetWindowPtr())->setDoubleBuffering(false);
	//ofSetVerticalSync(false);

    //updateHmdSettings();

	for( int eye = 0; eye < 2; eye++ ){
		eyeViewports[eye] = toOf(OVR::Recti(ovr_GetFovTextureSize(session, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], 1)));
	}

	eyeLayer = new EyeFovLayer( session, hmdDesc );
	backgroundLayer = new EyeFovLayer( session, hmdDesc, true, false ); //monoscopic, no depth
	transitionLayer = new EyeFovLayer( session, hmdDesc, true, false ); // mponoscopic, no depth

	ovrMirrorTextureDesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = windowSize.w;
    desc.Height = windowSize.h;
    desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

    // Create mirror texture and an FBO used to copy mirror texture to back buffer
    result = ovr_CreateMirrorTextureGL(session, &desc, &mirrorTexture);
    if (!OVR_SUCCESS(result))
    {
		ofLogError() << "Failed to create mirror texture";
    }

    // Configure the mirror read buffer
    GLuint texId;
    ovr_GetMirrorTextureBufferGL(session, mirrorTexture, &texId);

    glGenFramebuffers(1, &mirrorFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	renderTargetSize = hmdDesc.Resolution;

	//draw into background

	overlayTarget = ofPtr<ofFbo>( new ofFbo() );

	bSetup = true;
    return true;
}

bool ofxOculusDK2::isSetup() {
    return bSetup;
}

float ofxOculusDK2::getUserEyeHeight(void) {
    return ovr_GetFloat(session, OVR_KEY_EYE_HEIGHT, OVR_DEFAULT_EYE_HEIGHT);
}

bool ofxOculusDK2::getPositionTracking(void) {
    return bPositionTracking;
}

/*
TODO: RE-implement
void ofxOculusDK2::setPositionTracking(bool state) {
    bPositionTracking = state;
    bHmdSettingsChanged = true;
    updateHmdSettings();
}

void ofxOculusDK2::setSRGB(bool state) {
    bSRGB = state;
    bHmdSettingsChanged = true;
    updateHmdSettings();
}

void ofxOculusDK2::setHqDistortion(bool state) {
    bHqDistortion = state;
    bHmdSettingsChanged = true;
    updateHmdSettings();
}

float ofxOculusDK2::getPixelDensity(void) {
    return pixelDensity;
}

void ofxOculusDK2::setPixelDensity(float density) {
    pixelDensity = density;
    bHmdSettingsChanged = true;
    bRenderTargetSizeChanged = true;
    updateHmdSettings();
}
*/

void ofxOculusDK2::reset() {
//TODO: figure out how to do recentering
//    if (bSetup) {
//        ovr_RecenterPose(session);
//    }
}

ofQuaternion ofxOculusDK2::getOrientationQuat() {
    ovrTrackingState ts = ovr_GetTrackingState(session, ovr_GetTimeInSeconds(), true);
    if (ts.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked)) {
        return toOf(ts.HeadPose.ThePose.Orientation);
    }
    return ofQuaternion();
}

ofMatrix4x4 ofxOculusDK2::getProjectionMatrix(ovrEyeType eye) {
	//TODO: respect near/far plans
	unsigned int projectionModifier = ovrProjection_None | ovrProjection_ClipRangeOpenGL;
//	unsigned int projectionModifier = ovrProjection_ClipRangeOpenGL;
	return toOf(ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, baseCamera->getNearClip(), baseCamera->getFarClip(), projectionModifier));
}

ofMatrix4x4 ofxOculusDK2::getViewMatrix(ovrEyeType eye) {

	ofMatrix4x4 baseCameraMatrix = baseCamera->getModelViewMatrix();
	
	//cout << "POSE POSITION " << toOf(eyeRenderPose[eye].Position) << endl;

    ofMatrix4x4 hmdView =  ofMatrix4x4::newTranslationMatrix(toOf(eyeRenderPose[eye].Position) )  * 
		ofMatrix4x4::newRotationMatrix(toOf(eyeRenderPose[eye].Orientation));

	return baseCameraMatrix * hmdView.getInverse();
}

void ofxOculusDK2::setupEyeParams(ovrEyeType eye) {

	transitionLayer->update(eye, eyeRenderPose[eye], sensorSampleTime);
	backgroundLayer->update( eye, eyeRenderPose[eye], sensorSampleTime );

	eyeLayer->update( eye, eyeRenderPose[eye], sensorSampleTime );

	eyeLayer->begin( eye );

	ofSetMatrixMode(OF_MATRIX_PROJECTION);
    ofLoadIdentityMatrix();
    ofLoadMatrix(getProjectionMatrix(eye));

    ofSetMatrixMode(OF_MATRIX_MODELVIEW);
    ofLoadIdentityMatrix();
    ofLoadMatrix(getViewMatrix(eye));
}

ofRectangle ofxOculusDK2::getOculusViewport(ovrEyeType eye) {
	return eyeViewports[eye];
}

void ofxOculusDK2::beginBackground() {
    bUseBackground = true;
    insideFrame = true;
	backgroundLayer->begin( ovrEyeType(0) );
}

void ofxOculusDK2::endBackground() {
	backgroundLayer->end();
}

void ofxOculusDK2::setFade( float fade )
{
	fadeAmt = 1.0 - ofClamp(fade,0.0,1.0);
	
	transitionLayer->setClearColor( ofFloatColor(fadeColor.r,fadeColor.g, fadeColor.b, fadeAmt ) );
	
	if( !isFadedDown && isFading ){
		transitionLayer->begin( ovrEyeType(0) );
		transitionLayer->end();
	}

	if(fadeAmt == 1.0 ){ isFadedDown = true; isFading = false; }
	else if( fadeAmt == 0.0 ){ isFadedDown = false; isFading = false; }
	else { isFading = true; isFadedDown = false; }

}


void ofxOculusDK2::beginOverlay(float overlayZ, float scale,  float width, float height){
	
	bUseOverlay = true;
	overlayZDistance = overlayZ;
	
	if((int)overlayTarget->getWidth() != (int)width || (int)overlayTarget->getHeight() != (int)height || !overlayTarget->isAllocated() ){
		overlayTarget->allocate(width, height, GL_RGBA);
	}
	
	overlayMesh.clear();
	ofRectangle overlayrect = ofRectangle(-width/2*scale,-height/2*scale,width*scale,height*scale);
	overlayMesh.addVertex( ofVec3f(overlayrect.getMinX(), overlayrect.getMinY(), overlayZ) );
	overlayMesh.addVertex( ofVec3f(overlayrect.getMaxX(), overlayrect.getMinY(), overlayZ) );
	overlayMesh.addVertex( ofVec3f(overlayrect.getMinX(), overlayrect.getMaxY(), overlayZ) );
	overlayMesh.addVertex( ofVec3f(overlayrect.getMaxX(), overlayrect.getMaxY(), overlayZ) );

	overlayMesh.addTexCoord( ofVec2f(0, height ) );
	overlayMesh.addTexCoord( ofVec2f(width, height) );
	overlayMesh.addTexCoord( ofVec2f(0,0) );
	overlayMesh.addTexCoord( ofVec2f(width, 0) );
	
	overlayMesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
	
	overlayTarget->begin();
    ofClear(0.0, 0.0, 0.0, 0.0);
	
    ofPushView();
    ofPushMatrix();
}
	

void ofxOculusDK2::endOverlay() {
    ofPopMatrix();
    ofPopView();
	overlayTarget->end();
}

void ofxOculusDK2::grabFrameData()
{
	baseCamera->begin(getOculusViewport());
    baseCamera->end();

	eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

	hmdToEyeOffset[0] = eyeRenderDesc[0].HmdToEyeOffset; 
	hmdToEyeOffset[1] =	eyeRenderDesc[1].HmdToEyeOffset;

	ovr_GetEyePoses(session, frameIndex, ovrTrue, hmdToEyeOffset, eyeRenderPose, &sensorSampleTime);
	bSetFrameData = true;
}

void ofxOculusDK2::beginLeftEye() {

    if (!bSetup) return;

	//TODO:
	//if(!isVisible) return;

	if(!bSetFrameData){
		grabFrameData();
	}



	/* 

	eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

	hmdToEyeOffset[0] = eyeRenderDesc[0].HmdToEyeOffset; 
	hmdToEyeOffset[1] =	eyeRenderDesc[1].HmdToEyeOffset;
	*/

//	double ftiming = ovr_GetPredictedDisplayTime( session, 0 );
//	sensorSampleTime = ovr_GetTimeInSeconds();
//	ovrTrackingState hmdState = ovr_GetTrackingState( session, ftiming, ovrTrue );
//	ovr_CalcEyePoses( hmdState.HeadPose.ThePose, hmdToEyeViewOffsets, sceneLayer.RenderPose );

//	int currentIndex = 0;
//    ovr_GetTextureSwapChainCurrentIndex(session, textureSwapChain, &currentIndex);

	/*
	if( renderBuffer ) {
		auto* set = renderBuffer->TextureSet;
		set->CurrentIndex = ( set->CurrentIndex + 1 ) % set->TextureCount;
		renderBuffer->setAndClearRenderSurface( depthBuffer.get() );
	}
	*/
	

    insideFrame = true;


    ofPushView();
    ofPushMatrix();

    setupEyeParams(ovrEye_Left);
}

void ofxOculusDK2::endLeftEye() {
    if (!bSetup) return;

    if (bUseOverlay) {
        renderOverlay();
    }

	eyeLayer->end();

    ofPopMatrix();
    ofPopView();
}

void ofxOculusDK2::beginRightEye() {
    if (!bSetup) return;

	if(!bSetFrameData)
		grabFrameData();

    ofPushView();
    ofPushMatrix();

    setupEyeParams(ovrEye_Right);
}

void ofxOculusDK2::endRightEye() {
    if (!bSetup) return;

    if (bUseOverlay) {
        renderOverlay();
    }

	eyeLayer->end();

    ofPopMatrix();
    ofPopView();
	
	//SUBMITE FRAME

	ovrViewScaleDesc viewScaleDesc;
	viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;
	viewScaleDesc.HmdToEyeOffset[0] = hmdToEyeOffset[0];
	viewScaleDesc.HmdToEyeOffset[1] = hmdToEyeOffset[1];

	std::vector<ovrLayerHeader*> layers;
	if( bUseBackground ){
		layers.push_back(&backgroundLayer->getHeader());
	}
	layers.push_back( &eyeLayer->getHeader() );
	if( isFading || isFadedDown ){
		layers.push_back( &transitionLayer->getHeader() );
	}
	ovrResult result = ovr_SubmitFrame(session, frameIndex, &viewScaleDesc, layers.data(), layers.size() );
	bSetFrameData = false;

    // exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
    if (!OVR_SUCCESS(result)){
		ofLogError() << "OVR Render Error";
	}

    isVisible = (result == ovrSuccess);

    ovrSessionStatus sessionStatus;
    ovr_GetSessionStatus(session, &sessionStatus);
    if (sessionStatus.ShouldQuit){
		ofLogError() << "Quit!!";
	}

    if (sessionStatus.ShouldRecenter){
        ovr_RecenterTrackingOrigin(session);
	}

	/*
	ovrViewScaleDesc viewScaleDesc;
	viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;

	sceneLayer.Header.Type = ovrLayerType_EyeFov;
	//sceneLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
	for_each_eye([&](ovrEyeType eye){
		viewScaleDesc.HmdToEyeViewOffset[eye] = hmdToEyeViewOffsets[eye];
		sceneLayer.Fov[eye]			= eyeRenderDesc[eye].Fov;
		sceneLayer.RenderPose[eye]	= headPose[eye];
	});

	sceneLayer.ColorTexture[0] = renderBuffer->TextureSet;
	sceneLayer.ColorTexture[1] = NULL;
	auto size = renderBuffer->getSize();
	sceneLayer.Viewport[0].Pos.x = 0; 
	sceneLayer.Viewport[0].Pos.y = 0; 
	sceneLayer.Viewport[0].Size.w = size.w / 2;
	sceneLayer.Viewport[0].Size.h = size.h;
	sceneLayer.Viewport[1].Pos.x = (size.w + 1) / 2; 
	sceneLayer.Viewport[1].Pos.y = 0; 
	sceneLayer.Viewport[1].Size.w = size.w / 2;
	sceneLayer.Viewport[1].Size.h = size.h;

	sceneLayer.SensorSampleTime = sensorSampleTime;

	ovrLayerHeader* layers = &sceneLayer.Header;
	auto result = ovr_SubmitFrame( session, 0, &viewScaleDesc, &layers, 1 );
	
	skipFrame  = !(result == ovrSuccess);
	*/

}


void ofxOculusDK2::renderOverlay() {

    // cout << "renering overlay!" << endl;

    ofPushStyle();
    ofPushMatrix();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    ofDisableDepthTest();

    if (baseCamera != NULL) {
        ofTranslate(baseCamera->getPosition());
        ofMatrix4x4 baseRotation;
        baseRotation.makeRotationMatrix(baseCamera->getOrientationQuat());
        if (lockView) {
            ofMultMatrix(baseRotation);
        } else {
            ofMultMatrix(orientationMatrix*baseRotation);
        }
    }

    ofEnableAlphaBlending();
    overlayTarget->getTextureReference().bind();
    overlayMesh.draw();
    overlayTarget->getTextureReference().unbind();

    glPopAttrib();
    ofPopMatrix();
    ofPopStyle();

}



ofVec3f ofxOculusDK2::worldToScreen(ofVec3f worldPosition, bool considerHeadOrientation) {

    if (baseCamera == NULL) {
        return ofVec3f(0, 0, 0);
    }

    ofRectangle viewport = getOculusViewport();

    //ofMatrix4x4 projectedLeft = getViewMatrix(ovrEye_Left) * getProjectionMatrix(ovrEye_Right);

    if (considerHeadOrientation) {
        // We'll combine both left and right eye projections to get a midpoint.

        ofMatrix4x4 projectionMatrixLeft = toOf(ovrMatrix4f_Projection(eyeRenderDesc[ovrEye_Left].Fov, 0.01f, 10000.0f, true));
        ofMatrix4x4 projectionMatrixRight = toOf(ovrMatrix4f_Projection(eyeRenderDesc[ovrEye_Right].Fov, 0.01f, 10000.0f, true));

        ofMatrix4x4 modelViewMatrix = getOrientationMat();
        modelViewMatrix = modelViewMatrix * baseCamera->getGlobalTransformMatrix();
        baseCamera->begin(viewport);
        baseCamera->end();
        modelViewMatrix = modelViewMatrix.getInverse();

        ofVec3f cameraXYZ = worldPosition * (modelViewMatrix * projectionMatrixLeft);
        cameraXYZ.interpolate(worldPosition * (modelViewMatrix * projectionMatrixRight), 0.5f);

        ofVec3f screenXYZ((cameraXYZ.x + 1.0f) / 2.0f * viewport.width + viewport.x,
            (1.0f - cameraXYZ.y) / 2.0f * viewport.height + viewport.y,
            cameraXYZ.z);
        return screenXYZ;

    }

    return baseCamera->worldToScreen(worldPosition, viewport);
}

ofMatrix4x4 ofxOculusDK2::getOrientationMat() {
	//set to true
    ovrTrackingState ts = ovr_GetTrackingState(session, ovr_GetTimeInSeconds(), true);

    if (ts.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked)) {
        return ofMatrix4x4(toOf(ts.HeadPose.ThePose.Orientation));
    }
    return ofMatrix4x4();
}

//TODO head orientation not considered
ofVec3f ofxOculusDK2::screenToWorld(ofVec3f screenPt, bool considerHeadOrientation) {

    if (baseCamera == NULL) {
        return ofVec3f(0, 0, 0);
    }

    ofVec3f oculus2DPt = screenToOculus2D(screenPt, considerHeadOrientation);
    ofRectangle viewport = getOculusViewport();
    return baseCamera->screenToWorld(oculus2DPt, viewport);
}

//TODO head orientation not considered
ofVec3f ofxOculusDK2::screenToOculus2D(ofVec3f screenPt, bool considerHeadOrientation) {

    ofRectangle viewport = getOculusViewport();
    //  viewport.x -= viewport.width  / 2;
    //	viewport.y -= viewport.height / 2;
    viewport.scaleFromCenter(oculusScreenSpaceScale);
    return ofVec3f(ofMap(screenPt.x, 0, windowSize.w, viewport.getMinX(), viewport.getMaxX()),
        ofMap(screenPt.y, 0, windowSize.h, viewport.getMinY(), viewport.getMaxY()),
        screenPt.z);
}

//TODO: head position!
ofVec3f ofxOculusDK2::mousePosition3D(float z, bool considerHeadOrientation) {
    //	ofVec3f cursor3D = screenToWorld(cursor2D);
    return screenToWorld(ofVec3f(ofGetMouseX(), ofGetMouseY(), z));
}

float ofxOculusDK2::distanceFromMouse(ofVec3f worldPoint) {
    //map the current 2D position into oculus space
    return distanceFromScreenPoint(worldPoint, ofVec3f(ofGetMouseX(), ofGetMouseY()));
}

float ofxOculusDK2::distanceFromScreenPoint(ofVec3f worldPoint, ofVec2f screenPoint) {
    ofVec3f cursorRiftSpace = screenToOculus2D(screenPoint);
    ofVec3f targetRiftSpace = worldToScreen(worldPoint);

    float dist = ofDist(cursorRiftSpace.x, cursorRiftSpace.y,
        targetRiftSpace.x, targetRiftSpace.y);
    return dist;
}


void ofxOculusDK2::multBillboardMatrix() {
    multBillboardMatrix(mousePosition3D());
}

void ofxOculusDK2::multBillboardMatrix(ofVec3f objectPosition, ofVec3f updirection) {

    if (baseCamera == NULL) {
        return;
    }

    ofNode n;
    n.setPosition(objectPosition);
    n.lookAt(baseCamera->getPosition(), updirection);
    ofVec3f axis; float angle;
    n.getOrientationQuat().getRotate(angle, axis);
    // Translate the object to its position.
    ofTranslate(objectPosition);
    // Perform the rotation.
    ofRotate(angle, axis.x, axis.y, axis.z);
}
ofVec2f ofxOculusDK2::gazePosition2D() {
    ofVec3f angles = getOrientationQuat().getEuler();
	return ofVec2f(ofMap(angles.y, 90, -90, 0, getOculusViewport().width),
				   ofMap(angles.z, 90, -90, 0, getOculusViewport().height));
}

void ofxOculusDK2::draw() {
    if (!bSetup) return;
    if (!insideFrame) return;
	
	ofDisableDepthTest();

	if( mirrorTexture ) {
		
        // Blit mirror texture to back buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		GLint w = ofGetWidth();
		GLint h = ofGetHeight();
        glBlitFramebuffer(0, h, w, 0,
                          0, 0, w, h,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		
		ofViewport(0,0,ofGetWidth(), ofGetHeight());

	}

	/*
    renderTarget.bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    renderTarget.unbind();

    ovrLayerHeader* layers = &sceneLayer.Header;
    ovrResult result = ovr_SubmitFrame(hmd, frameIndex, nullptr, &layers, 1);

    auto swapTexture = sceneLayer.ColorTexture[0];
    ++(swapTexture->CurrentIndex) %= swapTexture->TextureCount;
	*/

    bUseOverlay = false;
    //bUseBackground = false;
    insideFrame = false;
}


void ofxOculusDK2::recenterPose(void) {
	//TODO: figure out how to force this
    //ovr_RecenterPose(session);
}

bool ofxOculusDK2::isHD() {
    if (bSetup) {
        return hmdDesc.Type == ovrHmd_DK2 || hmdDesc.Type == ovrHmd_DKHD;
    }
    return false;
}
