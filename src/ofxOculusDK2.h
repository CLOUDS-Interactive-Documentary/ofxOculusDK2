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
	DepthBuffer(OVR::Sizei size, int sampleCount);
    ~DepthBuffer();
};

//--------------------------------------------------------------------------
struct TextureBuffer : public TextureBufferBase
{
    ovrSession           Session;
    ovrTextureSwapChain  TextureChain;
    GLuint               fboId;
    OVR::Sizei           texSize;
	ofFloatColor		 clearColor;

    TextureBuffer(ovrSession session, bool rendertarget, bool displayableOnHmd, OVR::Sizei size, int mipLevels, unsigned char * data, int sampleCount);

    ~TextureBuffer();

    OVR::Sizei GetSize() const;
 
	void setClearColor( const ofFloatColor & color );

    void SetAndClearRenderSurface( const std::shared_ptr<DepthBuffer>& dbuffer);
   
    void Commit();
  
	void UnsetRenderSurface();
};

//---------------------------------------------------------------------------------------
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

//---------------------------------------------------------------------------------------

class EyeFovLayer : public LayerBase {

public:

	EyeFovLayer(  ovrSession &session, const ovrHmdDesc &desc, bool monoscopic = false, bool usesdepth = true );

	~EyeFovLayer();

	ovrLayerHeader& getHeader()override;

	void setClearColor( const ofFloatColor & color )override;

	void setPose(  const ovrPosef &pose, ovrEyeType eye )override;

	void update( double sampleTime)override;

	void begin()override;

	void end()override;
	
private:

	ovrEyeType currentEye;
	std::shared_ptr<TextureBuffer> eyeTextures[2];
	std::shared_ptr<DepthBuffer> depthBuffers[2];
	bool isMonoscopic;
	friend class ofxOculusDK2;
};

//---------------------------------------------------------------------------------------
class QuadLayer : public LayerBase {

public:

	void setHeadLocked();
	
	void setQuadSize( const ofVec2f & size );

	ovrLayerHeader& getHeader()override { return layer.Quad.Header; }; 

	void setClearColor( const ofFloatColor & color )override;

	ofVec2f getWorldQuadSize();

	ofVec2f getQuadResolution();

	void allocate( int allocation_width, int allocation_height, int mip_levels = 1 );

	QuadLayer( ovrSession s );

	~QuadLayer();

	void setPose(  const ovrPosef &pose, ovrEyeType eye = ovrEyeType::ovrEye_Left )override; 

	void update( double sampleTime )override{}

	void begin()override;

	void end()override;

	bool isAllocated(){ return bIsAllocated; }

private:

	ovrSession session;
	bool bIsAllocated;
	ovrEyeType currentEye;
	std::shared_ptr<TextureBuffer> quadTexture;

	friend class ofxOculusDK2;

};


//---------------------------------------------------------------------------------------
class ofxOculusDK2 {
public:

    ofxOculusDK2();
    ~ofxOculusDK2();

    //set a pointer to the camera you want as the base perspective
    //the oculus rendering will create a stereo pair from this camera
    //and mix in the head transformation
    ofCamera* baseCamera;
	ofCamera& getTransformedCamera();

    bool setup();

    bool isSetup();
    void reset();

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

    void recenterPose();

    float getUserEyeHeight(); // from standing height configured in oculus config user profile

    bool getPositionTracking();

	//HACK for buttons
	bool getButtonClicked(ovrButton button);
	/////

    ofQuaternion getOrientationQuat();
    ofMatrix4x4 getOrientationMat();
    ofVec3f getTranslation();

    //projects a 3D point into 2D, optionally accounting for the head orientation
    ofVec3f worldToScreen(ofVec3f worldPosition);

    //returns a 3d position of the mouse projected in front of the camera, at point z
    ofVec2f				gazePosition2D();

    //Sets up the view so that things drawn in 2D are billboarded to the camera,
    //Good way to draw custom cursors. don't forget to push/pop matrix around the call
    void				multBillboardMatrix(ofVec3f objectPosition, ofVec3f upDirection = ofVec3f(0, 1, 0));

	void				setPlayerScale(float scale);
	float				getPlayerScale();

    ofRectangle			getOverlayRectangle() { return ofRectangle(0, 0, hudLayer->getQuadResolution().x, hudLayer->getQuadResolution().y); }

	ofRectangle			getViewport(ovrEyeType eye = ovrEye_Left);
	ofRectangle			getDistortedViewport(ovrEyeType eye = ovrEye_Left);
	ofMatrix4x4			getProjectionMatrix(ovrEyeType eye = ovrEye_Left);
    ofMatrix4x4			getViewMatrix(ovrEyeType eye = ovrEye_Left);

private:

	void				onWindowResizeEvent( ofResizeEventArgs& args );

	bool				createSession();
	bool				detectHDM();
	void				initialize();
	void				uninitialize();
	void				initializeMirrorTexture( int width, int height );
	void				destroyMirrorTexture();
	void				blitMirrorTexture();
	void				reconnectHMD( ofEventArgs & args );

	//how much to scale the player by with positional tracking
	float				currentPlayerScale;

    bool				bSetup;
    bool				insideFrame;
    bool				bUseBackground;
    bool				bUseOverlay;
	bool				bDisplayConnected;
	bool				bRenderError;

	float				fadeAmt;
	ofFloatColor		fadeColor;

	bool				bSetFrameData;
	void				grabFrameData();

    ovrSession			session;
    ovrHmdDesc          hmdDesc;
    ovrGraphicsLuid     luid;

	unsigned int        frameIndex;
    ovrEyeRenderDesc	eyeRenderDesc[ovrEye_Count];
    ovrPosef            eyeRenderPose[ovrEye_Count];
    ovrVector3f         hmdToEyeOffset[ovrEye_Count];
	
	ofCamera			transformedCamera;

	map<ovrButton,bool> buttonStates; 
	map<ovrButton,bool> buttonChangedStates; 

	//Layers
	std::unique_ptr<EyeFovLayer>	eyeLayer, backgroundLayer, transitionLayer;
	std::unique_ptr<QuadLayer>		hudLayer;

	//MirrorTexture
	ovrMirrorTexture	mirrorTexture;
	GLuint				mirrorFBO;

	bool				isVisible;
	double				sensorSampleTime;

	ofRectangle			eyeViewports[ 2 ];

    ofMatrix4x4			orientationMatrix;

    void				setupEyeParams(ovrEyeType eye);

};
