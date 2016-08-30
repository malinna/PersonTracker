#pragma once

#include <d2d1.h>

class SceneRenderer
{
public:
	SceneRenderer();
	virtual ~SceneRenderer();

    HRESULT Initialize(HWND hwnd, ID2D1Factory* pD2DFactory, int sourceWidth, int sourceHeight, int sourceStride, int depthWidth, int depthHeight);

	HRESULT Draw(BYTE* pImage, unsigned long cbImage, int nBodyCount, IBody** ppBodies, ICoordinateMapper* pCoordinateMapper);

	ID2D1HwndRenderTarget* RenderTarget()	{ return m_pRenderTarget; }

private:
    HWND                     m_hWnd;

    UINT                     m_sourceHeight;
    UINT                     m_sourceWidth;
    LONG                     m_sourceStride;
	UINT                     m_depthHeight;
	UINT                     m_depthWidth;

    ID2D1Factory*            m_pD2DFactory;
    ID2D1HwndRenderTarget*   m_pRenderTarget;
    ID2D1Bitmap*             m_pBitmap;

	ID2D1SolidColorBrush*   m_pBrushJointTracked;
	ID2D1SolidColorBrush*   m_pBrushJointInferred;
	ID2D1SolidColorBrush*   m_pBrushBoneTracked;
	ID2D1SolidColorBrush*   m_pBrushBoneInferred;
	ID2D1SolidColorBrush*   m_pBrushHandClosed;
	ID2D1SolidColorBrush*   m_pBrushHandOpen;
	ID2D1SolidColorBrush*   m_pBrushHandLasso;
	
    HRESULT EnsureResources();
    void DiscardResources();

	void DrawBody(const Joint* pJoints, const D2D1_POINT_2F* pJointPoints);
	void DrawHand(HandState handState, const D2D1_POINT_2F& handPosition);
	void DrawBone(const Joint* pJoints, const D2D1_POINT_2F* pJointPoints, JointType joint0, JointType joint1);
};