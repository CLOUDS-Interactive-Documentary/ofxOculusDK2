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

//#include "Win32_GLAppUtil.h"
//#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"
#include "Extras/OVR_Math.h"

#include <iostream>

//---------------------------------------------------------------------------------------
struct DepthBuffer
{
    GLuint        texId;

    DepthBuffer(OVR::Sizei size, int sampleCount)
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
//        if (GLE_ARB_depth_buffer_float)
//        {
//            internalFormat = GL_DEPTH_COMPONENT32F;
//            type = GL_FLOAT;
//        }

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, size.w, size.h, 0, GL_DEPTH_COMPONENT, type, NULL);
    }
    ~DepthBuffer()
    {
        if (texId)
        {
            glDeleteTextures(1, &texId);
            texId = 0;
        }
    }
};

//--------------------------------------------------------------------------
struct TextureBuffer
{
    ovrSession          Session;
    ovrTextureSwapChain  TextureChain;
    GLuint              texId;
    GLuint              fboId;
    OVR::Sizei             texSize;

    TextureBuffer(ovrSession session, bool rendertarget, bool displayableOnHmd, OVR::Sizei size, int mipLevels, unsigned char * data, int sampleCount) :
        Session(session),
        TextureChain(nullptr),
        texId(0),
        fboId(0),
        texSize(0, 0)
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

    ~TextureBuffer()
    {
        if (TextureChain)
        {
            ovr_DestroyTextureSwapChain(Session, TextureChain);
            TextureChain = nullptr;
        }
        if (texId)
        {
            glDeleteTextures(1, &texId);
            texId = 0;
        }
        if (fboId)
        {
            glDeleteFramebuffers(1, &fboId);
            fboId = 0;
        }
    }

    OVR::Sizei GetSize() const
    {
        return texSize;
    }

    void SetAndClearRenderSurface(DepthBuffer* dbuffer)
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
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dbuffer->texId, 0);

        glViewport(0, 0, texSize.w, texSize.h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //glEnable(GL_FRAMEBUFFER_SRGB);
    }

    void UnsetRenderSurface()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    }

    void Commit()
    {
        if (TextureChain)
        {
            ovr_CommitTextureSwapChain(Session, TextureChain);
        }
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
    ovrPosef            eyeRenderPose[ovrEye_Count];
    ovrVector3f         hmdToEyeOffset[ovrEye_Count];
    ovrLayerEyeFov      layer;

	TextureBuffer*		eyeRenderTexture[ovrEye_Count];
	DepthBuffer*		eyeDepthBuffer[ovrEye_Count];

	ovrTextureSwapChain textureSwapChain;
	ovrMirrorTexture	mirrorTexture;
	GLuint				mirrorFBO;

	bool				isVisible;
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
