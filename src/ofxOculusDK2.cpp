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

////////////////////////////////////////
///DEPTH BUFFER
////////////////////////////////////////

DepthBuffer::DepthBuffer(OVR::Sizei size, int sampleCount):TextureBufferBase(0)
{
    assert(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLenum internalFormat = GL_DEPTH_COMPONENT32F;
    GLenum type = GL_FLOAT;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, size.w, size.h, 0, GL_DEPTH_COMPONENT, type, NULL);
}

DepthBuffer::~DepthBuffer()
{
   glDeleteTextures(1, &texId);
   texId = 0;
}

////////////////////////////////////////
///TEXTURE BUFFER
////////////////////////////////////////

TextureBuffer::TextureBuffer(ovrSession session, bool rendertarget, bool displayableOnHmd, OVR::Sizei size, int mipLevels, unsigned char * data, int sampleCount) :
    TextureBufferBase(0),
	Session(session),
    TextureChain(nullptr),
    fboId(0),
    texSize(0, 0),
	clearColor(0.,0.,0.,0.)
{
    assert(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

    texSize = size;

    if (displayableOnHmd)
    {
        // This texture isn't necessarily going to be a rendertarget, but it usually is.
        assert(session); // No HMD? A little odd.
        assert(sampleCount == 1); // ovr_CreateSwapTextureSetD3D11 doesn't support MSAA.

        ovrTextureSwapChainDesc desc = {};
        desc.Type = ovrTexture_2D;
        desc.ArraySize = 1;
        desc.Width = size.w;
        desc.Height = size.h;
        desc.MipLevels = 1; 
        desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
        //desc.Format = OVR_FORMAT_B8G8R8A8_UNORM;
		desc.SampleCount = 1;
        desc.StaticImage = ovrFalse;

        ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &TextureChain);

        int length = 0;
        ovr_GetTextureSwapChainLength(session, TextureChain, &length);

        if(OVR_SUCCESS(result))
        {
            for (int i = 0; i < length; ++i)
            {
                GLuint chainTexId;
                ovr_GetTextureSwapChainBufferGL(Session, TextureChain, i, &chainTexId);
                glBindTexture(GL_TEXTURE_2D, chainTexId);

                if (rendertarget)
                {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					float black_border[] = { 0.0f, 0.0f, 0.0f, 1.0f };
					glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, black_border);
                }
                else
                {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                }
            }
        }
    }
    else
    {
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);

        if (rendertarget)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        else
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, texSize.w, texSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    if (mipLevels > 1)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glGenFramebuffers(1, &fboId);
}

TextureBuffer::~TextureBuffer()
{
    if (TextureChain)
    {
        ovr_DestroyTextureSwapChain(Session, TextureChain);
        TextureChain = nullptr;
    }
    
	glDeleteTextures(1, &texId);
        
    glDeleteFramebuffers(1, &fboId);
    fboId = 0;
}

OVR::Sizei TextureBuffer::GetSize() const
{
    return texSize;
}

void TextureBuffer::setClearColor( const ofFloatColor & color ){
	clearColor = color;
}

void TextureBuffer::SetAndClearRenderSurface( const std::shared_ptr<DepthBuffer>& dbuffer)
{
    GLuint curTexId;
    if (TextureChain)
    {
        int curIndex;
        ovr_GetTextureSwapChainCurrentIndex(Session, TextureChain, &curIndex);
        ovr_GetTextureSwapChainBufferGL(Session, TextureChain, curIndex, &curTexId);
    }
    else
    {
        curTexId = texId;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fboId);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);

	if(dbuffer)
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dbuffer->texId, 0);

    glViewport(0, 0, texSize.w, texSize.h);
	glClearColor( clearColor.r, clearColor.g, clearColor.b, clearColor.a );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glEnable(GL_FRAMEBUFFER_SRGB);
}

void TextureBuffer::UnsetRenderSurface()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
}

void TextureBuffer::Commit()
{
    if (TextureChain)
    {
        ovr_CommitTextureSwapChain(Session, TextureChain);
    }
}

////////////////////////////////////////
///LAYERS - EYEFOV
////////////////////////////////////////


EyeFovLayer::EyeFovLayer(  ovrSession &session, const ovrHmdDesc &desc, bool monoscopic, bool usesdepth  ) : currentEye( ovrEyeType(0) ), isMonoscopic(monoscopic)
{
	layer.EyeFov.Header.Type  = ovrLayerType_EyeFov;
	layer.EyeFov.Header.Flags = ovrLayerFlag_HighQuality;

	ovrSizei idealTextureSize;
	idealTextureSize.w = 0;
	idealTextureSize.h = 0;

	if(monoscopic){

		for( int eye = 0; eye < 2; ++eye ){
			auto ideal = ovr_GetFovTextureSize(session, ovrEyeType(eye), desc.DefaultEyeFov[eye], 1);
			idealTextureSize.w = ideal.w;
			idealTextureSize.h = ideal.h;
		}

		eyeTextures[0] = std::shared_ptr<TextureBuffer>(  new TextureBuffer(session, true, true, idealTextureSize, 1, NULL, 1) );

		if(usesdepth)
			depthBuffers[0]   = std::shared_ptr<DepthBuffer>(  new DepthBuffer(eyeTextures[0]->GetSize(), 0) );
		else
			depthBuffers[0] = nullptr;
			
		eyeTextures[1] = nullptr;
		depthBuffers[1] = nullptr;
				
		layer.EyeFov.Viewport[0]     = OVR::Recti(eyeTextures[0]->GetSize());
		layer.EyeFov.Fov[0]          = desc.DefaultEyeFov[0];
		layer.EyeFov.ColorTexture[0] = eyeTextures[0]->TextureChain;

		layer.EyeFov.Viewport[1]     = OVR::Recti(eyeTextures[0]->GetSize());
		layer.EyeFov.Fov[1]          = desc.DefaultEyeFov[1];
		layer.EyeFov.ColorTexture[1] = eyeTextures[0]->TextureChain;

	}else{
	
		for( int eye = 0; eye < 2; ++eye ){

			idealTextureSize = ovr_GetFovTextureSize(session, ovrEyeType(eye), desc.DefaultEyeFov[eye], 1);

			eyeTextures[eye] = std::shared_ptr<TextureBuffer>( new TextureBuffer(session, true, true, idealTextureSize, 1, NULL, 1) );

			if(usesdepth)
				depthBuffers[eye]   = std::shared_ptr<DepthBuffer>( new DepthBuffer(eyeTextures[eye]->GetSize(), 0) );
			else
				depthBuffers[eye] = nullptr;

			layer.EyeFov.ColorTexture[eye] = eyeTextures[eye]->TextureChain;

			if (!eyeTextures[eye]->TextureChain)
				{
				ofLogError() << "Failed to create texture chain for eye : " << eye;
				}

			layer.EyeFov.Viewport[eye]     = OVR::Recti(eyeTextures[eye]->GetSize());
			layer.EyeFov.Fov[eye]          = desc.DefaultEyeFov[eye];
				
		}
	}
}

EyeFovLayer::~EyeFovLayer()
{
	if(eyeTextures[0])eyeTextures[0].reset();
	if(eyeTextures[1])eyeTextures[1].reset();
	if(depthBuffers[0])depthBuffers[0].reset();
	if(depthBuffers[1])depthBuffers[1].reset();
}

ovrLayerHeader& EyeFovLayer::getHeader() { return layer.EyeFov.Header; }; 

void EyeFovLayer::setClearColor( const ofFloatColor & color )
{
	eyeTextures[0]->setClearColor(color);
	if(!isMonoscopic)
		eyeTextures[1]->setClearColor(color);
}

void EyeFovLayer::setPose(  const ovrPosef &pose, ovrEyeType eye )
{
	layer.EyeFov.RenderPose[eye] = pose;
	currentEye = isMonoscopic ? ovrEyeType::ovrEye_Left : eye;
}

void EyeFovLayer::update( double sampleTime)
{
	layer.EyeFov.SensorSampleTime = sampleTime;
}

void EyeFovLayer::begin()
{
	eyeTextures[currentEye]->SetAndClearRenderSurface( depthBuffers[currentEye] );
}

void EyeFovLayer::end()
{
	eyeTextures[currentEye]->UnsetRenderSurface();
	eyeTextures[currentEye]->Commit();
}


////////////////////////////////////////
///LAYERS - QUAD
////////////////////////////////////////

QuadLayer::QuadLayer( ovrSession s ) : currentEye(ovrEyeType::ovrEye_Left), bIsAllocated(false), session(s)
{
	layer.Quad.Header.Type  = ovrLayerType_Quad;
	layer.Quad.Header.Flags = ovrLayerFlag_HighQuality | ovrLayerFlag_TextureOriginAtBottomLeft;

	layer.Quad.Viewport.Pos.x = 0;
	layer.Quad.Viewport.Pos.y = 0;

	layer.Quad.Viewport.Size.w = 1;
	layer.Quad.Viewport.Size.h = 1;

	layer.Quad.QuadSize.x = 0;
	layer.Quad.QuadSize.y = 0;

	layer.Quad.QuadPoseCenter.Position.x = 0;
	layer.Quad.QuadPoseCenter.Position.y = 0;
	layer.Quad.QuadPoseCenter.Position.z = 0;

	layer.Quad.QuadPoseCenter.Orientation.x = 0;
	layer.Quad.QuadPoseCenter.Orientation.y = 0;
	layer.Quad.QuadPoseCenter.Orientation.z = 0;
	layer.Quad.QuadPoseCenter.Orientation.w = 1;
}

QuadLayer::~QuadLayer()
{
	quadTexture.reset();
}

void QuadLayer::setHeadLocked()
{
	layer.Quad.Header.Flags |= ovrLayerFlag_HeadLocked;
}

void QuadLayer::setQuadSize( const ofVec2f & size )
{
	ovrVector2f s;
	s.x = size.x;
	s.y = size.y;
	layer.Quad.QuadSize = s;
}

void QuadLayer::setClearColor( const ofFloatColor & color )
{
	if( quadTexture )quadTexture->setClearColor(color);
}

ofVec2f QuadLayer::getWorldQuadSize(){
	return ofVec2f( layer.Quad.QuadSize.x, layer.Quad.QuadSize.y );
}

ofVec2f QuadLayer::getQuadResolution(){
	return ofVec2f( layer.Quad.Viewport.Size.w, layer.Quad.Viewport.Size.h );
}

void QuadLayer::allocate( int allocation_width, int allocation_height, int mip_levels )
{
	if( layer.Quad.Viewport.Size.w != allocation_width || layer.Quad.Viewport.Size.h != allocation_height || !bIsAllocated ){
		layer.Quad.Viewport.Size.w = allocation_width;
		layer.Quad.Viewport.Size.h = allocation_height;
		quadTexture = std::shared_ptr<TextureBuffer>(new TextureBuffer(session, true, true, OVR::Sizei(allocation_width, allocation_height), mip_levels, NULL, 1));
		layer.Quad.ColorTexture = quadTexture->TextureChain;
		bIsAllocated = true;
	}
}

void QuadLayer::setPose(  const ovrPosef &pose, ovrEyeType eye  ) {
	layer.Quad.QuadPoseCenter = pose;
}

void QuadLayer::begin()
{
	if( !bIsAllocated )return;
	quadTexture->SetAndClearRenderSurface(nullptr);
}

void QuadLayer::end()
{
	quadTexture->UnsetRenderSurface();
	quadTexture->Commit();
}


////////////////////////////////////////
///OCULUS SDK
////////////////////////////////////////

ofxOculusDK2::ofxOculusDK2() {
    
	bSetFrameData = false;
	mirrorTexture = nullptr;
	isVisible = true;
	baseCamera = nullptr;

	currentPlayerScale = 1.0;

    bSetup = false;
    insideFrame = false;
	fadeColor = ofFloatColor(0.,0.,0.,1.);
	fadeAmt = 0;

    bUseBackground = false;
    bUseOverlay = false;
	bDisplayConnected = false;
	bRenderError = false;

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
	mirrorTexture = nullptr;
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

	eyeLayer = std::unique_ptr<EyeFovLayer>( new EyeFovLayer( session, hmdDesc ) );
	backgroundLayer = std::unique_ptr<EyeFovLayer>( new EyeFovLayer( session, hmdDesc, true, false ) ); //monoscopic, no depth
	transitionLayer =  std::unique_ptr<EyeFovLayer>( new EyeFovLayer( session, hmdDesc, true, false ) ); // mponoscopic, no depth
	hudLayer = std::unique_ptr<QuadLayer>( new QuadLayer( session ) );
	hudLayer->setHeadLocked();

	initializeMirrorTexture( ofGetWidth(), ofGetHeight() );

	bDisplayConnected = true;
	bSetup = true;
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
	eyeLayer = nullptr;
	hudLayer.reset();
	hudLayer = nullptr;
	transitionLayer.reset();
	transitionLayer = nullptr;
	backgroundLayer.reset();
	backgroundLayer = nullptr;
	bSetup = false;
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
	
	createSession();
	initialize();

	ofAddListener( ofEvents().windowResized, this, &ofxOculusDK2::onWindowResizeEvent );

    return true;
}

bool ofxOculusDK2::isSetup() {
    return bSetup;
}

float ofxOculusDK2::getUserEyeHeight() {
    return ovr_GetFloat(session, OVR_KEY_EYE_HEIGHT, OVR_DEFAULT_EYE_HEIGHT);
}

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
	if (!bSetup || !bDisplayConnected) return;

    bUseBackground = true;
    insideFrame = true;
	backgroundLayer->begin();
}

void ofxOculusDK2::endBackground() {
	if (!bSetup || !bDisplayConnected) return;

	backgroundLayer->end();
}

void ofxOculusDK2::setFade( float fade )
{
	fadeAmt = 1.0 - ofClamp(fade,0.0,1.0);

	transitionLayer->setClearColor( ofFloatColor(fadeColor.r,fadeColor.g, fadeColor.b, fadeAmt ) );
	
	if (!bSetup || !bDisplayConnected) return;

	if( fadeAmt > 0.f ){
		transitionLayer->begin();
		transitionLayer->end();
	}

}


void ofxOculusDK2::beginOverlay(float world_z , float scale,  int pixel_size_x, int pixel_size_y){
	
	if (!bSetup || !bDisplayConnected) return;
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

	if (!bSetup || !bDisplayConnected) return;
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

	ovr_GetEyePoses(session, frameIndex, ovrTrue, hmdToEyeOffset, eyeRenderPose, &sensorSampleTime);

	eyeLayer->update( sensorSampleTime );
	backgroundLayer->update( sensorSampleTime );
	transitionLayer->update( sensorSampleTime );

	bSetFrameData = true;
}

void ofxOculusDK2::beginLeftEye() {

	if (!bSetup || !bDisplayConnected) return;
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

	if (!bSetup || !bDisplayConnected) return;
	//if(!isVisible) return;

	eyeLayer->end();

    ofPopMatrix();
    ofPopView();
}

void ofxOculusDK2::beginRightEye() {

	if (!bSetup || !bDisplayConnected) return;
	//if(!isVisible) return;

	if(!bSetFrameData){
		grabFrameData();
	}

    ofPushView();
    ofPushMatrix();

    setupEyeParams(ovrEye_Right);
}

void ofxOculusDK2::endRightEye() {

	if (!bSetup || !bDisplayConnected) return;

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

	if( !bRenderError ){

		if( bUseBackground ){
			layers.push_back(&backgroundLayer->getHeader());
		}

		layers.push_back( &eyeLayer->getHeader() );

		if( bUseOverlay ){
			layers.push_back(&hudLayer->getHeader());
		}

		if( fadeAmt > 0.f ){
			layers.push_back( &transitionLayer->getHeader() );
		}

	}

	ovrResult result = ovr_SubmitFrame(session, frameIndex, &viewScaleDesc, layers.data(), layers.size() );
	bSetFrameData = false;

    if (!OVR_SUCCESS(result)){
		ofLogError() << "OVR Render Error";
		bRenderError = true;
	}else{
		bRenderError = false;
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
		if(bDisplayConnected){
			uninitialize();
			ovr_Destroy(session);
			session = nullptr;
			ofLogNotice() << "Destroyed session, attempting to reconnect...";
			Sleep(1000);
			ofAddListener( ofEvents().update, this, &ofxOculusDK2::reconnectHMD );
			bDisplayConnected = false;
		}
	}
	
	bUseOverlay = false;
	insideFrame = false;
}

void ofxOculusDK2::reconnectHMD( ofEventArgs& args )
{
	if( detectHDM() ){
		createSession();
		initialize();
		Sleep(1000);
		bDisplayConnected = false;
		bRenderError = false;
		bDisplayConnected = true;
		ofRemoveListener( ofEvents().update, this, &ofxOculusDK2::reconnectHMD );
		beginBackground();
		endBackground();
		ofLogNotice() << "Reconnected HMD!";
	}
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
