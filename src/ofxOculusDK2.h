//
//  ofxOculusRift.cpp
//  OculusRiftRendering
//
//  Created by Andreas Müller on 30/04/2013.
//  Updated by James George September 27th 2013
//  Updated by Jason Walters October 22nd 2013
//

#pragma once 

#include "ofMain.h"
#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"

#include <iostream>



// TODO: openFrameworks FBOs currently don't allow for changing the texture attachments at every frame.
// We thus use the provided sample implementation by Oculus.

//---------------------------------------------------------------------------------------
struct DepthBuffer
{
	GLuint        texId;

	DepthBuffer( ovrSizei size, int sampleCount )
	{
		if( sampleCount > 1 ){
			ofLogError("DepthBuffer") << "MSAA is unsupported";
		}
		glGenTextures( 1, &texId );
		glBindTexture( GL_TEXTURE_2D, texId );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size.w, size.h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL );
	}

	~DepthBuffer() {
		glDeleteTextures( 1, &texId );
	}
};

//--------------------------------------------------------------------------
struct TextureBuffer
{
	ovrSwapTextureSet* TextureSet;
	GLuint        texId;
	GLuint        fboId;
	ovrSizei       texSize;

	TextureBuffer( ovrSession session, ovrSizei size, int mipLevels, int sampleCount )
	{
		if( sampleCount > 1 ){
			ofLogError("TextureBuffer") << "MSAA is unsupported";
		}
		texSize = size;

		// This texture isn't necessarily going to be a rendertarget, but it usually is.

		auto result = ovr_CreateSwapTextureSetGL( session, GL_SRGB8_ALPHA8, size.w, size.h, &TextureSet );
		if(result != ovrSuccess ){
			ofLogError("TextureBuffer") << "ovr_CreateSwapTextureSetGL failed";
		}

		for( int i = 0; i < TextureSet->TextureCount; ++i ) {
			ovrGLTexture* tex = (ovrGLTexture*)&TextureSet->Textures[i];
			glBindTexture( GL_TEXTURE_2D, tex->OGL.TexId );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		}

		if( mipLevels > 1 ) {
			glGenerateMipmap( GL_TEXTURE_2D );
		}

		glGenFramebuffers( 1, &fboId );
	}

	~TextureBuffer() {
		glDeleteFramebuffers( 1, &fboId );
	}

	ovrSizei getSize() const
	{
		return texSize;
	}

	void setAndClearRenderSurface( DepthBuffer * dbuffer )
	{
		ovrGLTexture* tex = (ovrGLTexture*)&TextureSet->Textures[TextureSet->CurrentIndex];

		glBindFramebuffer( GL_FRAMEBUFFER, fboId );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->OGL.TexId, 0 );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dbuffer->texId, 0 );

		glViewport( 0, 0, texSize.w, texSize.h );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	void unsetRenderSurface()
	{
		glBindFramebuffer( GL_FRAMEBUFFER, fboId );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0 );
	}
};

class ofxOculusDK2 {
public:

    ofxOculusDK2();
    ~ofxOculusDK2();

    //set a pointer to the camera you want as the base perspective
    //the oculus rendering will create a stereo pair from this camera
    //and mix in the head transformation
    ofCamera* baseCamera;
	ofCamera riftCamera;

    bool setup();
    //bool setup(ofFbo::Settings& render_settings);

    bool isSetup();
    void reset();
    bool lockView;

    //draw background, before rendering eyes
    void beginBackground();
    void endBackground();

    //draw overlay, before rendering eyes
    void beginOverlay(float overlayZDistance = -150, float scale = 1.0, float width = 256, float height = 256);
    void endOverlay();

    void beginLeftEye();
    void endLeftEye();

    void beginRightEye();
    void endRightEye();

    void draw();

    void dismissSafetyWarning();
    void recenterPose();

    float getUserEyeHeight(); // from standing height configured in oculus config user profile

    bool getPositionTracking();
    void setPositionTracking(bool state);

    bool getNoMirrorToWindow();
    void setNoMirrorToWindow(bool state);
    bool getSRGB();
    void setSRGB(bool state);
    bool getOverdrive();
    void setOverdrive(bool state);
    bool getHqDistortion();
    void setHqDistortion(bool state);
    bool getTimewarpJitDelay();
    void setTimewarpJitDelay(bool state);

    float getPixelDensity();
    void setPixelDensity(float density);

    ofQuaternion getOrientationQuat();
    ofMatrix4x4 getOrientationMat();
    ofVec3f getTranslation();

    //default 1 has more constrained mouse movement,
    //while turning it up increases the reach of the mouse
    float oculusScreenSpaceScale;

    //projects a 3D point into 2D, optionally accounting for the head orientation
    ofVec3f worldToScreen(ofVec3f worldPosition, bool considerHeadOrientation = false);
    ofVec3f screenToWorld(ofVec3f screenPt, bool considerHeadOrientation = false);
    ofVec3f screenToOculus2D(ofVec3f screenPt, bool considerHeadOrientation = false);

    //returns a 3d position of the mouse projected in front of the camera, at point z
    ofVec3f mousePosition3D(float z = 0, bool considerHeadOrientation = false);

    ofVec2f gazePosition2D();

    //sets up the view so that things drawn in 2D are billboarded to the caemra,
    //centered at the mouse
    //Good way to draw custom cursors. don't forget to push/pop matrix around the call
    void multBillboardMatrix();
    void multBillboardMatrix(ofVec3f objectPosition, ofVec3f upDirection = ofVec3f(0, 1, 0));

    float distanceFromMouse(ofVec3f worldPoint);
    float distanceFromScreenPoint(ofVec3f worldPoint, ofVec2f screenPoint);

    ofRectangle getOverlayRectangle() {
        return ofRectangle(0, 0,
            overlayTarget.getWidth(),
            overlayTarget.getHeight());
    }
    ofFbo& getOverlayTarget() {
        return overlayTarget;
    }
    ofFbo& getBackgroundTarget() {
        return backgroundTarget;
    }
//    ofFbo& getRenderTarget() {
 //       return renderTarget;
 //   }

	ofRectangle getOculusViewport(ovrEyeType eye = ovrEye_Left);
    bool isHD();
    //allows you to disable moving the camera based on inner ocular distance
    bool applyTranslation();

private:

    bool bSetup;
    bool insideFrame;
    //unsigned startTrackingCaps;

    //bool bHmdSettingsChanged;
   // bool bRenderTargetSizeChanged;

    bool bPositionTracking;

    // hmd capabilities
//    bool bNoMirrorToWindow; 

    // distortion caps
    bool bSRGB;
    bool bHqDistortion;

    bool bUseBackground;
    bool bUseOverlay;
    float overlayZDistance;

	bool skipFrame;

    ovrSession			session;
    ovrHmdDesc          hmdDesc;
    ovrGraphicsLuid     luid;
    //ovrFrameTiming      frameTiming;
    unsigned int        frameIndex;
    ovrEyeRenderDesc	eyeRenderDesc[ovrEye_Count];
    ovrPosef            headPose[ovrEye_Count];
    ovrVector3f         hmdToEyeViewOffsets[ovrEye_Count];
    ovrLayerEyeFov      sceneLayer;

	std::unique_ptr<TextureBuffer>	renderBuffer;
	std::unique_ptr<DepthBuffer>	depthBuffer;

	ovrGLTexture*		mirrorTexture;
	GLuint				mirrorFBO;

	double				sensorSampleTime;

	void initializeMirrorTexture( ovrSizei size );
	void destroyMirrorTexture();

    ovrSizei               windowSize;

    ofMesh overlayMesh;
    ofMatrix4x4 orientationMatrix;

    ofFbo::Settings renderTargetFboSettings();

    float pixelDensity;
    ovrSizei renderTargetSize;
    ofFbo renderTarget;
    ofFbo backgroundTarget;
    ofFbo overlayTarget;
    ofShader distortionShader;

    ofShader debugShader;   // XXX mattebb
    ofMesh debugMesh;
    ofImage debugImage;

    bool getHmdCap(unsigned int cap);

    void setupEyeParams(ovrEyeType eye);
    void setupShaderUniforms(ovrEyeType eye);

    ofMatrix4x4 getProjectionMatrix(ovrEyeType eye);
    ofMatrix4x4 getViewMatrix(ovrEyeType eye);

    void renderOverlay();

    void updateHmdSettings();
    unsigned int setupDistortionCaps();
    unsigned int setupHmdCaps();
};
