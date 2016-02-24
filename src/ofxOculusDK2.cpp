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
#include "ofAppGLFWWindow.h"

#include <stdio.h>  // XXX mattebb for testing, printf

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
    memset(&sceneLayer, 0, sizeof(sceneLayer));
    sceneLayer.Header.Type = ovrLayerType_EyeFov;
    sceneLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

	baseCamera = nullptr;
	lockView = false;
	oculusScreenSpaceScale = 2.0f;
	//JG added default values
    bSetup = false;
    insideFrame = false;
    startTrackingCaps = 0;

    bHmdSettingsChanged = true;
    bRenderTargetSizeChanged = true;
	
	pixelDensity = 1.0;

    bPositionTracking = true;

    // hmd capabilities
    bNoMirrorToWindow = false; 

    // distortion caps
    bSRGB = true;
    bHqDistortion = true;

    bUseBackground = false;
    bUseOverlay = false;
    overlayZDistance = -200;

    hmd = nullptr;
    frameIndex = 0;;
}

ofxOculusDK2::~ofxOculusDK2() {
    if (bSetup) {
        if (hmd) {
            ovr_Destroy(hmd);
            hmd = 0;
        }

        ovr_Shutdown();
        bSetup = false;
    }
}

ofFbo::Settings ofxOculusDK2::renderTargetFboSettings() {
    ofFbo::Settings settings;
    //settings.numSamples = 4;
    settings.numSamples = 0;
    settings.internalformat = GL_RGBA;
    settings.useDepth = true;
    settings.textureTarget = GL_TEXTURE_2D;
    settings.minFilter = GL_LINEAR;
    settings.maxFilter = GL_LINEAR;
    settings.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
    settings.wrapModeVertical = GL_CLAMP_TO_EDGE;
    settings.depthStencilInternalFormat = GL_DEPTH_COMPONENT24;
    return settings;
}

unsigned int ofxOculusDK2::setupDistortionCaps() {
    unsigned int caps = 0;

    //if (bTimeWarp)
    //    caps |= ovrDistortionCap_TimeWarp;
    //if (bVignette)
    //    caps |= ovrDistortionCap_Vignette;
    //if (bSRGB)
    //    caps |= ovrDistortionCap_SRGB;
    //if (bOverdrive)
    //    caps |= ovrDistortionCap_Overdrive;
    //if (bHqDistortion)
    //    caps |= ovrDistortionCap_HqDistortion;
    //if (bTimewarpJitDelay)
    //    caps |= ovrDistortionCap_TimewarpJitDelay;
    return caps;
}

unsigned int ofxOculusDK2::setupHmdCaps() {
    unsigned int caps = 0;

    //if (bNoMirrorToWindow)
    //    caps |= ovrHmdCap_NoMirrorToWindow;
    //if (bDisplayOff)
    //    caps |= ovrHmdCap_DisplayOff;
    //if (bLowPersistence)
    //    caps |= ovrHmdCap_LowPersistence;
    //if (bDynamicPrediction)
    //    caps |= ovrHmdCap_DynamicPrediction;
    //if (bNoVsync)
    //    caps |= ovrHmdCap_NoVSync;
    return caps;
}

// Convenience method for looping over each eye with a lambda
template <typename Function>
inline void for_each_eye(Function function) {
    for (ovrEyeType eye = ovrEyeType::ovrEye_Left;
        eye < ovrEyeType::ovrEye_Count;
        eye = static_cast<ovrEyeType>(eye + 1)) {
        function(eye);
    }
}


void ofxOculusDK2::updateHmdSettings() {
    if (!bHmdSettingsChanged) return;

    // Initialise the sensor which provides the Rift’s pose and motion.
    unsigned int trackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection;
    if (bPositionTracking)
        trackingCaps |= ovrTrackingCap_Position;

    // only update trackingCaps if they've changed
    if (trackingCaps != startTrackingCaps) {
        ovr_ConfigureTracking(hmd, trackingCaps, 0);
        startTrackingCaps = trackingCaps;
    }

    // Initialize eye rendering information for ovr_Configure.
    // The viewport sizes are re-computed in case RenderTargetSize changed due to HW limitations.
    for_each_eye([&](ovrEyeType eye) {
        sceneLayer.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
    });

    if (bRenderTargetSizeChanged) {
        ovrSizei recommendedSizes[2];
        for_each_eye([&](ovrEyeType eye) {
            recommendedSizes[eye] = ovr_GetFovTextureSize(hmd, eye, sceneLayer.Fov[eye], pixelDensity);
        });
        renderTargetSize.w = recommendedSizes[0].w + recommendedSizes[1].w;
        renderTargetSize.h = max(recommendedSizes[0].h, recommendedSizes[1].h);

        ofFbo::Settings render_settings = renderTargetFboSettings();
        render_settings.width = renderTargetSize.w;
        render_settings.height = renderTargetSize.h;

        renderTarget.allocate(render_settings);
        backgroundTarget.allocate(renderTargetSize.w / 2, renderTargetSize.h);

        bRenderTargetSizeChanged = false;
    }

    for_each_eye([&](ovrEyeType eye){
        eyeRenderDesc[eye] = ovr_GetRenderDesc(hmd, ovrEye_Left, sceneLayer.Fov[eye]);
        hmdToEyeViewOffsets[eye] = eyeRenderDesc[eye].HmdToEyeViewOffset;
        sceneLayer.Viewport[eye].Pos.x = 0;
        sceneLayer.Viewport[eye].Pos.y = 0;
        sceneLayer.Viewport[eye].Size.w = renderTargetSize.w / 2;
		sceneLayer.Viewport[eye].Size.h = renderTargetSize.h;
    });

    sceneLayer.Viewport[ovrEye_Right].Pos = toOVRVector2i((renderTargetSize.w + 1) / 2, 0);

    if (bHqDistortion) {
        sceneLayer.Header.Flags |= ovrLayerFlag_HighQuality;
    } else {
        sceneLayer.Header.Flags &= ~ovrLayerFlag_HighQuality;
    }

    if (sceneLayer.ColorTexture) {
        ovr_DestroySwapTextureSet(hmd, sceneLayer.ColorTexture[0]);
        sceneLayer.ColorTexture[0] = nullptr;
    }
    if (!OVR_SUCCESS(ovr_CreateSwapTextureSetGL(hmd, GL_RGBA, renderTargetSize.w, renderTargetSize.h, &sceneLayer.ColorTexture[0]))) {
        return;
    }

    for (int i = 0; i < sceneLayer.ColorTexture[0]->TextureCount; ++i) {
        ovrGLTexture& ovrTex = (ovrGLTexture&)sceneLayer.ColorTexture[0]->Textures[i];
        glBindTexture(GL_TEXTURE_2D, ovrTex.OGL.TexId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    bHmdSettingsChanged = false;
}

bool ofxOculusDK2::setup() {

    if (bSetup) {
        ofLogError("ofxOculusDK2::setup") << "Already set up";
        return false;
    }

    // Oculus HMD & Sensor Initialization
    ovr_Initialize(nullptr);

    ovrResult result = ovr_Create(&hmd, &luid);
    if (!OVR_SUCCESS(result)) {
        return false;
    }

    // In Direct App-rendered mode, we can use smaller window size,
    // as it can have its own contents and isn't tied to the buffer.
    ovrSizei wsize;
    wsize.w = ofGetWidth(); wsize.h = ofGetHeight();
    windowSize = wsize; //Sizei(960, 540); avoid rotated output bug.

    // WARNING: Slightly dangerous!
    // We need to disable double buffering when using SDK rendering,
    // Since the oculus SDK takes care of drawing to screen at VSync time
    // If OF tries to swap buffers too, it conflicts and messes up the timing,
    // preventing the Oculus SDK from Vsyncing properly and causing all kinds of judder.

    // OF uses GLFW by default on PC-ish platforms, so we assume it here.
    // If it's not using GLFW, maybe this might cause a nasty crash :)
    ((ofAppGLFWWindow*)ofGetWindowPtr())->setDoubleBuffering(false);

    updateHmdSettings();

    bSetup = true;
    return true;
}

bool ofxOculusDK2::isSetup() {
    return bSetup;
}

float ofxOculusDK2::getUserEyeHeight(void) {
    return ovr_GetFloat(hmd, OVR_KEY_EYE_HEIGHT, OVR_DEFAULT_EYE_HEIGHT);
}

bool ofxOculusDK2::getPositionTracking(void) {
    return bPositionTracking;
}
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

void ofxOculusDK2::reset() {
    if (bSetup) {
        ovr_RecenterPose(hmd);
    }
}

ofQuaternion ofxOculusDK2::getOrientationQuat() {
    ovrTrackingState ts = ovr_GetTrackingState(hmd, ovr_GetTimeInSeconds(), true);
    if (ts.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked)) {
        return toOf(ts.HeadPose.ThePose.Orientation);
    }
    return ofQuaternion();
}

ofMatrix4x4 ofxOculusDK2::getProjectionMatrix(ovrEyeType eye) {
    return toOf(ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, .01f, 10000.0f, true));
}

ofMatrix4x4 ofxOculusDK2::getViewMatrix(ovrEyeType eye) {

    ofMatrix4x4 baseCameraMatrix = baseCamera->getModelViewMatrix();

    // head orientation and position
    ofMatrix4x4 hmdView = ofMatrix4x4::newRotationMatrix(toOf(headPose[eye].Orientation)) * \
        ofMatrix4x4::newTranslationMatrix(toOf(headPose[eye].Position));

    // final multiplication of everything
    return baseCameraMatrix * hmdView.getInverse();
}

void ofxOculusDK2::setupEyeParams(ovrEyeType eye) {
    /*
    if(bUseBackground){
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    ofDisableDepthTest();
    backgroundTarget.getTextureReference().draw(toOf(eyeRenderViewport[eye]));
    glPopAttrib();
    }*/

    ofViewport(toOf(sceneLayer.Viewport[eye]));

    ofSetMatrixMode(OF_MATRIX_PROJECTION);
    ofLoadIdentityMatrix();
    ofLoadMatrix(getProjectionMatrix(eye));

    ofSetMatrixMode(OF_MATRIX_MODELVIEW);
    ofLoadIdentityMatrix();
    ofLoadMatrix(getViewMatrix(eye));
}

ofRectangle ofxOculusDK2::getOculusViewport() {
    //	OVR::Util::Render::StereoEyeParams eyeRenderParams = stereo.GetEyeRenderParams( OVR::Util::Render::StereoEye_Left );
    //	return toOf(eyeRenderParams.VP);
    return toOf(sceneLayer.Viewport[0]);
}

void ofxOculusDK2::beginBackground() {
    bUseBackground = true;
    insideFrame = true;
    backgroundTarget.begin(false);
    ofClear(0.0, 0.0, 0.0);
    ofPushView();
    ofPushMatrix();
    ofViewport(getOculusViewport());
}

void ofxOculusDK2::endBackground() {
    ofPopMatrix();
    ofPopView();
    backgroundTarget.end();
}

void ofxOculusDK2::beginOverlay(float overlayZ, float width, float height) {
    bUseOverlay = true;
    overlayZDistance = overlayZ;

    if ((int)overlayTarget.getWidth() != (int)width || (int)overlayTarget.getHeight() != (int)height) {
        overlayTarget.allocate(width, height, GL_RGBA, 4);
    }

    overlayMesh.clear();
    ofRectangle overlayrect = ofRectangle(-width / 2, -height / 2, width, height);
    overlayMesh.addVertex(ofVec3f(overlayrect.getMinX(), overlayrect.getMinY(), overlayZ));
    overlayMesh.addVertex(ofVec3f(overlayrect.getMaxX(), overlayrect.getMinY(), overlayZ));
    overlayMesh.addVertex(ofVec3f(overlayrect.getMinX(), overlayrect.getMaxY(), overlayZ));
    overlayMesh.addVertex(ofVec3f(overlayrect.getMaxX(), overlayrect.getMaxY(), overlayZ));

    overlayMesh.addTexCoord(ofVec2f(0, height));
    overlayMesh.addTexCoord(ofVec2f(width, height));
    overlayMesh.addTexCoord(ofVec2f(0, 0));
    overlayMesh.addTexCoord(ofVec2f(width, 0));

    overlayMesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);

    overlayTarget.begin(false);
    ofClear(0.0, 0.0, 0.0, 0.0);

    ofPushView();
    ofPushMatrix();
}

void ofxOculusDK2::endOverlay() {
    ofPopMatrix();
    ofPopView();
    overlayTarget.end();
}

void ofxOculusDK2::beginLeftEye() {

    if (!bSetup) return;

    //ovr_GetEyePoses(hmd, 0, hmdToEyeViewOffsets, headPose, NULL);
    ovr_GetEyePoses(hmd, 0, true, hmdToEyeViewOffsets, headPose, NULL);
    for_each_eye([&](ovrEyeType eye) {
        sceneLayer.RenderPose[eye] = headPose[eye];
    });

    insideFrame = true;

    renderTarget.begin(false);
    auto swapTextures = sceneLayer.ColorTexture[0];
    ovrGLTexture& texture = (ovrGLTexture&)(swapTextures->Textures[swapTextures->CurrentIndex]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.OGL.TexId, 0);

    ofClear(0);

    ofPushView();
    ofPushMatrix();

    setupEyeParams(ovrEye_Left);

}

void ofxOculusDK2::endLeftEye() {
    if (!bSetup) return;

    if (bUseOverlay) {
        renderOverlay();
    }

    ofPopMatrix();
    ofPopView();
}

void ofxOculusDK2::beginRightEye() {
    if (!bSetup) return;

    ofPushView();
    ofPushMatrix();

    setupEyeParams(ovrEye_Right);
}

void ofxOculusDK2::endRightEye() {
    if (!bSetup) return;

    if (bUseOverlay) {
        renderOverlay();
    }

    ofPopMatrix();
    ofPopView();
    renderTarget.end();
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
    overlayTarget.getTextureReference().bind();
    overlayMesh.draw();
    overlayTarget.getTextureReference().unbind();

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
        baseCamera->begin();
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
    ovrTrackingState ts = ovr_GetTrackingState(hmd, ovr_GetTimeInSeconds(), true);

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
    return ofVec2f(ofMap(angles.y, 90, -90, 0, windowSize.w),
        ofMap(angles.z, 90, -90, 0, windowSize.h));
}

void ofxOculusDK2::draw() {
    if (!bSetup) return;
    if (!insideFrame) return;

    renderTarget.bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    renderTarget.unbind();

    ovrLayerHeader* layers = &sceneLayer.Header;
    ovrResult result = ovr_SubmitFrame(hmd, frameIndex, nullptr, &layers, 1);

    auto swapTexture = sceneLayer.ColorTexture[0];
    ++(swapTexture->CurrentIndex) %= swapTexture->TextureCount;

    bUseOverlay = false;
    bUseBackground = false;
    insideFrame = false;
}


void ofxOculusDK2::recenterPose(void) {
    ovr_RecenterPose(hmd);
}

bool ofxOculusDK2::isHD() {
    if (bSetup) {
        return hmdDesc.Type == ovrHmd_DK2 || hmdDesc.Type == ovrHmd_DKHD;
    }
    return false;
}
