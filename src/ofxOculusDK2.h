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

struct TextureBufferBase {
	TextureBufferBase( GLuint id ):texId(id){}
	GLuint texId;
};

//---------------------------------------------------------------------------------------
struct DepthBuffer : public TextureBufferBase
{

	DepthBuffer(OVR::Sizei size, int sampleCount):TextureBufferBase(0)
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
struct TextureBuffer : public TextureBufferBase
{
    ovrSession           Session;
    ovrTextureSwapChain  TextureChain;
    GLuint               fboId;
    OVR::Sizei           texSize;
	ofFloatColor		 clearColor;

    TextureBuffer(ovrSession session, bool rendertarget, bool displayableOnHmd, OVR::Sizei size, int mipLevels, unsigned char * data, int sampleCount) :
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

	void setClearColor( const ofFloatColor & color ){
		clearColor = color;
	}

    void SetAndClearRenderSurface( const std::shared_ptr<DepthBuffer>& dbuffer)
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

class LayerBase {

public:

	virtual ~LayerBase(){}
	virtual void update( double sampleTime ) = 0 ;
	virtual void setPose( const ovrPosef &pose, ovrEyeType eye = ovrEyeType::ovrEye_Left ) = 0;
	virtual void begin() = 0 ;
	virtual void end() = 0;
	virtual ovrLayerHeader& getHeader() = 0; 
	virtual void setClearColor( const ofFloatColor & color ) = 0;

protected:

	LayerBase(){}

	ovrLayer_Union layer;
};

class EyeFovLayer : public LayerBase {

public:

	EyeFovLayer(  ovrSession &session, const ovrHmdDesc &desc, bool monoscopic = false, bool usesdepth = true ) : currentEye( ovrEyeType(0) ), isMonoscopic(monoscopic)
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

	~EyeFovLayer()
	{
	}

	ovrLayerHeader& getHeader()override { return layer.EyeFov.Header; }; 

	void setClearColor( const ofFloatColor & color )override
	{
		eyeTextures[0]->setClearColor(color);
		if(!isMonoscopic)
			eyeTextures[1]->setClearColor(color);
	}

	void setPose(  const ovrPosef &pose, ovrEyeType eye )override {
		layer.EyeFov.RenderPose[eye] = pose;
		currentEye = isMonoscopic ? ovrEyeType::ovrEye_Left : eye;
	}

	void update( double sampleTime)override{
		layer.EyeFov.SensorSampleTime = sampleTime;
	}

	void begin()override{
		eyeTextures[currentEye]->SetAndClearRenderSurface( depthBuffers[currentEye] );
	 }

	void end()override
	{
		eyeTextures[currentEye]->UnsetRenderSurface();
		eyeTextures[currentEye]->Commit();
	}

private:

	ovrEyeType currentEye;
	std::shared_ptr<TextureBuffer> eyeTextures[2];
	std::shared_ptr<DepthBuffer> depthBuffers[2];
	bool isMonoscopic;
	friend class ofxOculusDK2;
};


class QuadLayer : public LayerBase {

public:

	void setHeadLocked()
	{
		layer.Quad.Header.Flags |= ovrLayerFlag_HeadLocked;
	}

	void setQuadSize( const ofVec2f & size )
	{
			ovrVector2f s;
			s.x = size.x;
			s.y = size.y;
			//TODO check this
			layer.Quad.QuadSize = s;
	}

	ovrLayerHeader& getHeader()override { return layer.Quad.Header; }; 

	void setClearColor( const ofFloatColor & color )override
	{
		if( quadTexture )quadTexture->setClearColor(color);
	}

	ofVec2f getWorldQuadSize(){
		return ofVec2f( layer.Quad.QuadSize.x, layer.Quad.QuadSize.y );
	}

	ofVec2f getQuadResolution(){
		return ofVec2f( layer.Quad.Viewport.Size.w, layer.Quad.Viewport.Size.h );
	}

	void allocate( int allocation_width, int allocation_height, int mip_levels = 1 )
	{
		if( layer.Quad.Viewport.Size.w != allocation_width || layer.Quad.Viewport.Size.h != allocation_height || !bIsAllocated ){
			layer.Quad.Viewport.Size.w = allocation_width;
			layer.Quad.Viewport.Size.h = allocation_height;
			quadTexture = std::shared_ptr<TextureBuffer>(new TextureBuffer(rSession, true, true, OVR::Sizei(allocation_width, allocation_height), mip_levels, NULL, 1));
			layer.Quad.ColorTexture = quadTexture->TextureChain;
			bIsAllocated = true;
		}
	}

	QuadLayer( ovrSession &session ) : currentEye(ovrEyeType::ovrEye_Left), bIsAllocated(false), rSession(session)
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

	~QuadLayer()
	{
	}

	void setPose(  const ovrPosef &pose, ovrEyeType eye = ovrEyeType::ovrEye_Left )override {
		layer.Quad.QuadPoseCenter = pose;
	}

	void update( double sampleTime )override{}

	void begin()override{
		if( !bIsAllocated )return;
		quadTexture->SetAndClearRenderSurface(nullptr);
	 }

	void end()override
	{
		quadTexture->UnsetRenderSurface();
		quadTexture->Commit();
	}

	bool isAllocated(){ return bIsAllocated; }

private:

	ovrSession & rSession;
	bool bIsAllocated;
	ovrEyeType currentEye;
	std::shared_ptr<TextureBuffer> quadTexture;

	friend class ofxOculusDK2;

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

    bool isSetup();
    void reset();
    bool lockView;

    //draw background, before rendering eyes
    void beginBackground();
    void endBackground();

	//fade
	void setFade( float fade );
	void setFadeColor( const ofFloatColor & color ){ fadeColor = color; }

    //draw overlay, before rendering eyes
    void beginOverlay(float world_z , float scale, int pixel_size_x, int pixel_size_y );
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
		return ofRectangle(0, 0, hudLayer->getQuadResolution().x, hudLayer->getQuadResolution().y);
    }

  //  ofFbo& getOverlayTarget() {
   //     return *overlayTarget;
  //  }
	
	ofRectangle getOculusViewport(ovrEyeType eye = ovrEye_Left);
    bool isHD();
    //allows you to disable moving the camera based on inner ocular distance

private:

    bool bSetup;
    bool insideFrame;

    bool bPositionTracking;

    bool bSRGB;
    bool bHqDistortion;

    bool bUseBackground;
    bool bUseOverlay;
    float overlayZDistance;

	float fadeAmt;
	ofFloatColor fadeColor;
	bool isFading;
	bool isFadedDown;

	bool bSetFrameData;
	void grabFrameData();

	bool skipFrame;

    ovrSession			session;
    ovrHmdDesc          hmdDesc;
    ovrGraphicsLuid     luid;

	unsigned int        frameIndex;
    ovrEyeRenderDesc	eyeRenderDesc[ovrEye_Count];
    ovrPosef            eyeRenderPose[ovrEye_Count];
    ovrVector3f         hmdToEyeOffset[ovrEye_Count];

	//Layers
	ofPtr<EyeFovLayer>	eyeLayer, backgroundLayer, transitionLayer;
	ofPtr<QuadLayer>	hudLayer;

	//MirrorTexture
	ovrMirrorTexture	mirrorTexture;
	GLuint				mirrorFBO;

	bool				isVisible;
	double				sensorSampleTime;

	void initializeMirrorTexture( ovrSizei size );
	void destroyMirrorTexture();

    ovrSizei               windowSize;
	ofRectangle			   eyeViewports[ 2 ];

    ofMesh overlayMesh;
    ofMatrix4x4 orientationMatrix;

    ofFbo::Settings renderTargetFboSettings();

    float pixelDensity;
    ovrSizei renderTargetSize;

    ofPtr<ofFbo> overlayTarget;

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
