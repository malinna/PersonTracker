#pragma once

#include "resource.h"
#include "SceneRenderer.h"
#include "DataWriter.h"
#include <opencv2\opencv.hpp>

class PersonTracker
{
    static const int cDepthWidth  = 512;
    static const int cDepthHeight = 424;
	static const int cColorWidth = 1920;
	static const int cColorHeight = 1080;
	static const int cFPS = 30;

public:
	PersonTracker(const std::wstring& szDataDir);
	~PersonTracker();

    static LRESULT CALLBACK MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static int CALLBACK BrowseFolderCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

    int Run(HINSTANCE hInstance, int nCmdShow);

private:
    HWND                    m_hWnd;
    INT64                   m_nStartTime;
    INT64                   m_nLastCounter;
    double                  m_fFreq;
    INT64                   m_nNextStatusTime;
    DWORD                   m_nFramesSinceUpdate;

    IKinectSensor*          m_pKinectSensor;
    ICoordinateMapper*      m_pCoordinateMapper;
    IBodyFrameReader*       m_pBodyFrameReader;
	IColorFrameReader*      m_pColorFrameReader;

	SceneRenderer*          m_pSceneRenderer;
    ID2D1Factory*           m_pD2DFactory;
	RGBQUAD*                m_pColorRGBX;

    ID2D1SolidColorBrush*   m_pBrushJointTracked;
    ID2D1SolidColorBrush*   m_pBrushJointInferred;
    ID2D1SolidColorBrush*   m_pBrushBoneTracked;
    ID2D1SolidColorBrush*   m_pBrushBoneInferred;
    ID2D1SolidColorBrush*   m_pBrushHandClosed;
    ID2D1SolidColorBrush*   m_pBrushHandOpen;
    ID2D1SolidColorBrush*   m_pBrushHandLasso;

	std::wstring			m_szDataDir;
	DataWriter*				m_pDataWriter;

	float					m_jointScaleX;
	float					m_jointScaleY;

	HRESULT Update();

    HRESULT InitializeDefaultSensor();
	void CleanupSensor();

	void ProcessScene(INT64 nTime, RGBQUAD* pBuffer, int nWidth, int nHeight, int nBodyCount, IBody** ppBodies);

    bool SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce);

	void StartRecording();	
	void StopRecording();

	FrameData CreateAndValidateFrame(RGBQUAD* pBuffer, int nWidth, int nHeight, int nBodyCount, IBody** ppBodies);
};

