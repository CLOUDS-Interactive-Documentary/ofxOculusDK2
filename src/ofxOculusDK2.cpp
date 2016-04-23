//
//  ofxOculusDK2.cpp
//  OculusRiftRendering
//
//  Created by Andreas Müller on April 2013
//  Updated by James George September 27th 2013
//  Updated by Jason Walters October 22 2013
//  Adapted to DK2 by James George and Elie Zananiri August 2014
//  Updated for DK2 by Matt Ebb October 2014
//  Updated for CV1 by James George & Mike Allison April 2016


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

OVR::Matrix4f toOVR(const ofMatrix4x4& m) {
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
    
	bSetFrameData = false;
	mirrorTexture = nullptr;
	isVisible = true;
	isFadedDown = false;
	baseCamera = nullptr;

	currentPlayerScale = 1.0;

    bSetup = false;
    insideFrame = false;
	isFading = false;
	fadeColor = ofFloatColor(0.,0.,0.,1.);
	fadeAmt = 0;

//    bPositionTracking = true;
//    bSRGB = true;

    bUseBackground = false;
    bUseOverlay = false;

    session = nullptr;
    frameIndex = 0;;
}

ofxOculusDK2::~ofxOculusDK2() {
    if (bSetup) {

		uninitialize();

		ovr_Destroy(session);
		
	    ovr_Shutdown();

		session = nullptr;
        bSetup = false;
    }
}


void ofxOculusDK2::initializeMirrorTexture( int width, int height )
{

	if( mirrorTexture ){
		destroyMirrorTexture();
	}

	ovrMirrorTextureDesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

    // Create mirror texture and an FBO used to copy mirror texture to back buffer
	ovrResult result = ovr_CreateMirrorTextureGL(session, &desc, &mirrorTexture);
    if (!OVR_SUCCESS(result))
    {
		ofLogError() << "Failed to create mirror texture";
    }

	GLuint texId;
    ovr_GetMirrorTextureBufferGL(session, mirrorTexture, &texId);

    glGenFramebuffers(1, &mirrorFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void ofxOculusDK2::blitMirrorTexture()
{
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
}

void ofxOculusDK2::destroyMirrorTexture()
{
	if (mirrorFBO) glDeleteFramebuffers(1, &mirrorFBO);
	if (mirrorTexture) ovr_DestroyMirrorTexture(session, mirrorTexture);
}


bool ofxOculusDK2::detectHDM(){
	ovrDetectResult detection = ovr_Detect(0);
	return detection.IsOculusHMDConnected;
}

void ofxOculusDK2::initialize()
{
	hmdDesc = ovr_GetHmdDesc( session );

	for( int eye = 0; eye < 2; eye++ ){
		eyeViewports[eye] = toOf(OVR::Recti(ovr_GetFovTextureSize(session, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], 1)));
	}

	eyeLayer = ofPtr<EyeFovLayer>( new EyeFovLayer( session, hmdDesc ) );
	backgroundLayer = ofPtr<EyeFovLayer>( new EyeFovLayer( session, hmdDesc, true, false ) ); //monoscopic, no depth
	transitionLayer =  ofPtr<EyeFovLayer>( new EyeFovLayer( session, hmdDesc, true, false ) ); // mponoscopic, no depth
	hudLayer = ofPtr<QuadLayer>( new QuadLayer( session ) );
	hudLayer->setHeadLocked();

	mirrorTexture = nullptr;
	initializeMirrorTexture( ofGetWidth(), ofGetHeight() );

}

bool ofxOculusDK2::createSession()
{
	ovrResult result = ovr_Create(&session, &luid);
    if (!OVR_SUCCESS(result)) {
		ovrErrorInfo info;
		ovr_GetLastErrorInfo(&info);
		ofLogError("ofxOculusDK2::setup") << "Setup failed! " << result << " " << info.ErrorString;
        return false;
    }
	else
		return true;
}

void ofxOculusDK2::uninitialize()
{
	destroyMirrorTexture();
	eyeLayer.reset();
	hudLayer.reset();
	transitionLayer.reset();
	backgroundLayer.reset();
}

void ofxOculusDK2::onWindowResizeEvent( ofResizeEventArgs& args )
{
	initializeMirrorTexture( args.width, args.height );
}


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
	
	while(!createSession()){ Sleep(10); }
	initialize();

	ofAddListener( ofEvents().windowResized, this, &ofxOculusDK2::onWindowResizeEvent );

	bSetup = true;
    return true;
}

bool ofxOculusDK2::isSetup() {
    return bSetup;
}

float ofxOculusDK2::getUserEyeHeight() {
    return ovr_GetFloat(session, OVR_KEY_EYE_HEIGHT, OVR_DEFAULT_EYE_HEIGHT);
}

//bool ofxOculusDK2::getPositionTracking(void) {
//    return bPositionTracking;
//}

void ofxOculusDK2::setPlayerScale(float scale){
	currentPlayerScale = scale;
}

float ofxOculusDK2::getPlayerScale(){
	return currentPlayerScale;
}

void ofxOculusDK2::reset() {
   if (bSetup) {
        ovr_RecenterTrackingOrigin(session);
    }
}

ofMatrix4x4 ofxOculusDK2::getOrientationMat() {
	return ofMatrix4x4(getOrientationQuat());
}

ofQuaternion ofxOculusDK2::getOrientationQuat() {
	return toOf(eyeRenderPose[0].Orientation);

	/*
    ovrTrackingState ts = ovr_GetTrackingState(session, ovr_GetTimeInSeconds(), true);
    if (ts.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked)) {
        return toOf(ts.HeadPose.ThePose.Orientation);
    }
    return ofQuaternion();
	*/
}

ofMatrix4x4 ofxOculusDK2::getProjectionMatrix(ovrEyeType eye) {
	unsigned int projectionModifier = ovrProjection_None | ovrProjection_ClipRangeOpenGL;
	return toOf(ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, baseCamera->getNearClip(), baseCamera->getFarClip(), projectionModifier));
}

ofMatrix4x4 ofxOculusDK2::getViewMatrix(ovrEyeType eye) {

	OVR::Matrix4f finalRollPitchYaw = toOVR(ofMatrix4x4( baseCamera->getOrientationQuat())) * OVR::Matrix4f(eyeRenderPose[eye].Orientation);
	OVR::Vector3f finalUp = finalRollPitchYaw.Transform( OVR::Vector3f(0, 1, 0)); // 
	OVR::Vector3f finalForward = finalRollPitchYaw.Transform( OVR::Vector3f(0, 0, -1)); //
	OVR::Vector3f shiftedEyePos = toOVR(ofMatrix4x4( baseCamera->getOrientationQuat())).Transform( eyeRenderPose[eye].Position );
	//apply scale
	shiftedEyePos *= currentPlayerScale;
	shiftedEyePos += toOVR( baseCamera->getPosition() );

    OVR::Matrix4f view = OVR::Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
	return toOf(view);
}

void ofxOculusDK2::setupEyeParams(ovrEyeType eye) {

	transitionLayer->setPose( eyeRenderPose[eye], eye );
	backgroundLayer->setPose( eyeRenderPose[eye], eye );

	eyeLayer->setPose( eyeRenderPose[eye], eye );

	eyeLayer->begin();

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
	backgroundLayer->begin();
}

void ofxOculusDK2::endBackground() {
	backgroundLayer->end();
}

void ofxOculusDK2::setFade( float fade )
{
	fadeAmt = 1.0 - ofClamp(fade,0.0,1.0);
	
	transitionLayer->setClearColor( ofFloatColor(fadeColor.r,fadeColor.g, fadeColor.b, fadeAmt ) );
	
	if( !isFadedDown && isFading ){
		transitionLayer->begin();
		transitionLayer->end();
	}

	if(fadeAmt == 1.0 ){ isFadedDown = true; isFading = false; }
	else if( fadeAmt == 0.0 ){ isFadedDown = false; isFading = false; }
	else { isFading = true; isFadedDown = false; }

}


void ofxOculusDK2::beginOverlay(float world_z , float scale,  int pixel_size_x, int pixel_size_y){
	
	if (!bSetup) return;
	//if(!isVisible) return;

	auto res = hudLayer->getQuadResolution();
	hudLayer->allocate( pixel_size_x, pixel_size_y, 1 );

	float world_width = (float)pixel_size_x / (float)pixel_size_y;
	float world_height =  1.f; 

	auto z = -abs(world_z);

	ovrPosef pose;

	pose.Position.x = 0.0f;
	pose.Position.y = 0.0f;
	pose.Position.z = z;

	pose.Orientation.x = 0.0f;
	pose.Orientation.y = 0.0f;
	pose.Orientation.z = 0.0f;
	pose.Orientation.w = 1.0f;

	hudLayer->setPose( pose );

	bUseOverlay = true;
	auto size = scale * ofVec2f( world_width, world_height );
	hudLayer->setQuadSize( size );
	hudLayer->begin();

	ofPushView();
    ofPushMatrix();
	
	ofSetupScreenOrtho(res.x,res.y);
	glViewport( 0, 0, res.x, res.y );

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    ofDisableDepthTest();
    ofEnableAlphaBlending();

}
	

void ofxOculusDK2::endOverlay() {

	if (!bSetup) return;
	//if(!isVisible) return;

	glPopAttrib();
    ofPopStyle();
    ofPopMatrix();
    ofPopView();
	hudLayer->end();
}

void ofxOculusDK2::grabFrameData()
{
	baseCamera->begin(getOculusViewport());
    baseCamera->end();

	eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

	hmdToEyeOffset[0] = eyeRenderDesc[0].HmdToEyeOffset; 
	hmdToEyeOffset[1] =	eyeRenderDesc[1].HmdToEyeOffset;

	auto recent_tracked_state = ovr_GetTrackingState(session, 0.0, false);
	//bPositionTracking = recent_tracked_state.StatusFlags & ovrStatusBits::ovrStatus_PositionTracked;

	ovr_GetEyePoses(session, frameIndex, ovrTrue, hmdToEyeOffset, eyeRenderPose, &sensorSampleTime);

	eyeLayer->update( sensorSampleTime );
	backgroundLayer->update( sensorSampleTime );
	transitionLayer->update( sensorSampleTime );

	bSetFrameData = true;
}

void ofxOculusDK2::beginLeftEye() {

    if (!bSetup) return;
	//if(!isVisible) return;

	if(!bSetFrameData){
		grabFrameData();
	}

    insideFrame = true;

    ofPushView();
    ofPushMatrix();

    setupEyeParams(ovrEye_Left);
}

void ofxOculusDK2::endLeftEye() {

    if (!bSetup) return;
	//if(!isVisible) return;

	eyeLayer->end();

    ofPopMatrix();
    ofPopView();
}

void ofxOculusDK2::beginRightEye() {

    if (!bSetup) return;
	//if(!isVisible) return;

	if(!bSetFrameData){
		grabFrameData();
	}

    ofPushView();
    ofPushMatrix();

    setupEyeParams(ovrEye_Right);
}

void ofxOculusDK2::endRightEye() {

    if (!bSetup) return;

	//if(!isVisible){
		eyeLayer->end();
		ofPopMatrix();
		ofPopView();
	//}
	
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

	if( bUseOverlay ){
		layers.push_back(&hudLayer->getHeader());
	}

	if( isFading || isFadedDown ){
		layers.push_back( &transitionLayer->getHeader() );
	}

	ovrResult result = ovr_SubmitFrame(session, frameIndex, &viewScaleDesc, layers.data(), layers.size() );
	bSetFrameData = false;

    if (!OVR_SUCCESS(result)){
		ofLogError() << "OVR Render Error";
	}

    ovrSessionStatus sessionStatus;
    ovr_GetSessionStatus(session, &sessionStatus);

    if (sessionStatus.ShouldQuit){
		ofLogError() << "Quit!!";
	}

    if (sessionStatus.ShouldRecenter){
        ovr_RecenterTrackingOrigin(session);
	}

	isVisible = sessionStatus.IsVisible;

	if( sessionStatus.DisplayLost ){
		uninitialize();
        ovr_Destroy(session);
		//spin thread until HMD detected
		while( !detectHDM() ){ Sleep(10); }
		//spin thread until session created
		while( !createSession() ){ Sleep(10); }
		initialize();
	}
	
	bUseOverlay = false;
	insideFrame = false;
}


//TODO: 
// work with positional tracking
ofVec3f ofxOculusDK2::worldToScreen(ofVec3f worldPosition) {

    if (baseCamera == NULL) {
        return ofVec3f(0, 0, 0);
    }

    ofRectangle viewport = getOculusViewport();

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

//TODO: respond to Positional tracking!
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
	blitMirrorTexture();
}

void ofxOculusDK2::recenterPose(void) {
	reset();
}
