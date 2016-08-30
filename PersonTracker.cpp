#include "stdafx.h"
#include <strsafe.h>
#include <Shlobj.h>
#include "resource.h"
#include "PersonTracker.h"

static const float c_JointThickness = 3.0f;
static const float c_TrackedBoneThickness = 6.0f;
static const float c_InferredBoneThickness = 1.0f;
static const float c_HandSize = 30.0f;

int APIENTRY wWinMain(    
	_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	std::wstring szDataDir = GetUserDocumentsPath() + L"\\PersonTracker";

	PersonTracker application(szDataDir);
    application.Run(hInstance, nShowCmd);
}

PersonTracker::PersonTracker(const std::wstring& szDataDir) :
    m_hWnd(NULL),
    m_nStartTime(0),
    m_nLastCounter(0),
    m_nFramesSinceUpdate(0),
    m_fFreq(0),
    m_nNextStatusTime(0LL),
    m_pKinectSensor(NULL),
    m_pCoordinateMapper(NULL),
    m_pBodyFrameReader(NULL),
	m_pSceneRenderer(NULL),
	m_pColorFrameReader(NULL),
    m_pD2DFactory(NULL),
    m_pBrushJointTracked(NULL),
    m_pBrushJointInferred(NULL),
    m_pBrushBoneTracked(NULL),
    m_pBrushBoneInferred(NULL),
    m_pBrushHandClosed(NULL),
    m_pBrushHandOpen(NULL),
    m_pBrushHandLasso(NULL),
	m_szDataDir(szDataDir),
	m_pDataWriter(NULL)
{
    LARGE_INTEGER qpf = {0};
    if (QueryPerformanceFrequency(&qpf))
    {
        m_fFreq = double(qpf.QuadPart);
    }

	// create heap storage for color pixel data in RGBX format
	m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];

	m_jointScaleX = static_cast<float>(DataWriter::videoWidth()) / cColorWidth;
	m_jointScaleY = static_cast<float>(DataWriter::videoHeight()) / cColorHeight;
}
  
PersonTracker::~PersonTracker()
{
	if (m_pDataWriter)
	{
		delete m_pDataWriter;
		m_pDataWriter = NULL;
	}

	if (m_pSceneRenderer)
	{
		delete m_pSceneRenderer;
		m_pSceneRenderer = NULL;
	}

	if (m_pColorRGBX)
	{
		delete[] m_pColorRGBX;
		m_pColorRGBX = NULL;
	}

    SafeRelease(m_pD2DFactory);

	CleanupSensor();
}

int PersonTracker::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = {0};
    WNDCLASS  wc;

    // Dialog custom window class
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra    = DLGWINDOWEXTRA;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc   = DefDlgProcW;
    wc.lpszClassName = L"PersonTrackerAppDlgWndClass";

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        NULL,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
		(DLGPROC)PersonTracker::MessageRouter,
        reinterpret_cast<LPARAM>(this));

    // Show window
    ShowWindow(hWndApp, nCmdShow);

    // Main message loop
    while (WM_QUIT != msg.message)
    {
        Update();

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if (hWndApp && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

HRESULT PersonTracker::Update()
{
	if (!m_pBodyFrameReader || !m_pColorFrameReader)
    {
        return E_FAIL;
    }

	IColorFrame* pColorFrame = NULL;
	HRESULT hr = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);

	if (FAILED(hr))
	{
		return hr;
	}

	INT64 nTime = 0;
	IFrameDescription* pFrameDescription = NULL;
	int nWidth = 0;
	int nHeight = 0;
	ColorImageFormat imageFormat = ColorImageFormat_None;
	UINT nBufferSize = 0;
	RGBQUAD *pBuffer = NULL;
	IBody* ppBodies[BODY_COUNT] = { 0 };

	hr = pColorFrame->get_RelativeTime(&nTime);

	if (SUCCEEDED(hr))
	{
		hr = pColorFrame->get_FrameDescription(&pFrameDescription);
	}

	if (SUCCEEDED(hr))
	{
		hr = pFrameDescription->get_Width(&nWidth);
	}

	if (SUCCEEDED(hr))
	{
		hr = pFrameDescription->get_Height(&nHeight);
	}

	if (SUCCEEDED(hr))
	{
		hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
	}

	if (SUCCEEDED(hr))
	{
		if (imageFormat == ColorImageFormat_Bgra)
		{
			hr = pColorFrame->AccessRawUnderlyingBuffer(&nBufferSize, reinterpret_cast<BYTE**>(&pBuffer));
		}
		else if (m_pColorRGBX)
		{
			pBuffer = m_pColorRGBX;
			nBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
			hr = pColorFrame->CopyConvertedFrameDataToArray(nBufferSize, reinterpret_cast<BYTE*>(pBuffer), ColorImageFormat_Bgra);
		}
		else
		{
			hr = E_FAIL;
		}
	}

	SafeRelease(pColorFrame);
	SafeRelease(pFrameDescription);

	if (FAILED(hr))
	{
		return hr;
	}

	IBodyFrame* pBodyFrame = NULL;
	hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);

	if (FAILED(hr))
	{
		return hr;
	}

    hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);

	SafeRelease(pBodyFrame);

	if (FAILED(hr))
	{
		return hr;
	}

	ProcessScene(nTime, pBuffer, nWidth, nHeight, BODY_COUNT, ppBodies);

    for (int i = 0; i < _countof(ppBodies); ++i)
    {
        SafeRelease(ppBodies[i]);
    }

	return S_OK;
}

LRESULT CALLBACK PersonTracker::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PersonTracker* pThis = NULL;
    
    if (WM_INITDIALOG == uMsg)
    {
		pThis = reinterpret_cast<PersonTracker*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
		pThis = reinterpret_cast<PersonTracker*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

LRESULT CALLBACK PersonTracker::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {
            m_hWnd = hWnd;

			D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

			m_pSceneRenderer = new SceneRenderer();
			HRESULT hr = m_pSceneRenderer->Initialize(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), m_pD2DFactory, cColorWidth, cColorHeight, cColorWidth * sizeof(RGBQUAD), cDepthWidth, cDepthHeight);
			if (FAILED(hr))
			{
				SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
			}

            InitializeDefaultSensor();

			SetDlgItemText(hWnd, IDC_STATIC_RECORD_PATH, m_szDataDir.c_str());
        }
        break;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

		case WM_COMMAND:
			if (BN_CLICKED == HIWORD(wParam))
			{
				if (IDC_BUTTON_RECORD == LOWORD(wParam))
				{
					StartRecording();
				}
				else if (IDC_BUTTON_STOP == LOWORD(wParam))
				{
					StopRecording();
				}
				else if (IDC_BUTTON_SELECT_PATH == LOWORD(wParam))
				{
					HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);

					if (hr == S_OK || hr == S_FALSE)
					{
						BROWSEINFO   bi;
						ZeroMemory(&bi, sizeof(bi));

						WCHAR szBuffer[MAX_PATH];

						BOOL ret = GetDlgItemText(hWnd, IDC_STATIC_RECORD_PATH, szBuffer, MAX_PATH);

						if (ret)
						{
							std::wstring currentPath = szBuffer;

							bi.hwndOwner = hWnd;
							bi.pszDisplayName = szBuffer;
							bi.lpszTitle = L"Select target path recordings.";
							bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_RETURNFSANCESTORS;
							bi.lpfn = BrowseFolderCallback;
							bi.lParam = reinterpret_cast<LPARAM>(currentPath.c_str());

							LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
							if (NULL != pidl)
							{
								if (SHGetPathFromIDList(pidl, szBuffer))
								{
									SetDlgItemText(hWnd, IDC_STATIC_RECORD_PATH, szBuffer);
								}

								CoTaskMemFree(pidl);
							}
						}

						CoUninitialize();
					}
				}
				else if (IDC_CHECK_AUTO_RECORD == LOWORD(wParam))
				{

				}
			}
			break;
    }

    return FALSE;
}

int CALLBACK PersonTracker::BrowseFolderCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		LPCTSTR path = reinterpret_cast<LPCTSTR>(lpData);
		::SendMessage(hwnd, BFFM_SETSELECTION, true, (LPARAM)path);
	}
	return 0;
}

HRESULT PersonTracker::InitializeDefaultSensor()
{
    HRESULT hr;

    hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
		OutputDebugStringA("GetDefaultKinectSensor() failed.\n");
        return hr;
    }

    if (m_pKinectSensor)
    {
        IBodyFrameSource* pBodyFrameSource = NULL;
		IColorFrameSource* pColorFrameSource = NULL;

        hr = m_pKinectSensor->Open();

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
        }

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_BodyFrameSource(&pBodyFrameSource);
        }

        if (SUCCEEDED(hr))
        {
            hr = pBodyFrameSource->OpenReader(&m_pBodyFrameReader);
        }

		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_ColorFrameSource(&pColorFrameSource);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameSource->OpenReader(&m_pColorFrameReader);
		}

        SafeRelease(pBodyFrameSource);
		SafeRelease(pColorFrameSource);
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
		OutputDebugStringA("Failed to initialize kinect.\n");
        SetStatusMessage(L"Kinect not found!", 10000, true);

        return E_FAIL;
    }

    return hr;
}

void PersonTracker::CleanupSensor()
{
	SafeRelease(m_pBodyFrameReader);
	SafeRelease(m_pColorFrameReader);
	SafeRelease(m_pCoordinateMapper);

	if (m_pKinectSensor)
	{
		m_pKinectSensor->Close();
	}

	SafeRelease(m_pKinectSensor);
}

void PersonTracker::ProcessScene(INT64 nTime, RGBQUAD* pBuffer, int nWidth, int nHeight, int nBodyCount, IBody** ppBodies)
{
	unsigned long dataWriterBufferSize = 0;

	LRESULT result = ::SendMessage(GetDlgItem(m_hWnd, IDC_CHECK_AUTO_RECORD), BM_GETCHECK, 0, 0);
	bool autoRecordEnabled = result == 1 ? true : false;

	if (autoRecordEnabled)
	{
		FrameData data = CreateAndValidateFrame(pBuffer, nWidth, nHeight, nBodyCount, ppBodies);

		if (data.persons.size() > 0)
		{
			StartRecording();

			dataWriterBufferSize = m_pDataWriter->bufferSize();

			m_pDataWriter->appendFrame(data);
		}
		else
		{
			if (m_pDataWriter)
			{
				if (m_pDataWriter->frameCount() < 10)
				{
					m_pDataWriter->eraseVideo();
				}

				StopRecording();
			}
		}
	}
	else
	{
		if (m_pDataWriter)
		{
			dataWriterBufferSize = m_pDataWriter->bufferSize();

			FrameData data = CreateAndValidateFrame(pBuffer, nWidth, nHeight, nBodyCount, ppBodies);

			if (data.persons.size() > 0)
			{
				m_pDataWriter->appendFrame(data);
			}
		}
	}

	if (pBuffer && (nWidth == cColorWidth) && (nHeight == cColorHeight))
	{
		m_pSceneRenderer->Draw(reinterpret_cast<BYTE*>(pBuffer), cColorWidth * cColorHeight * sizeof(RGBQUAD), nBodyCount, ppBodies, m_pCoordinateMapper);
	}

	if (m_hWnd)
	{
		if (!m_nStartTime)
		{
			m_nStartTime = nTime;
		}

		double fps = 0.0;

		LARGE_INTEGER qpcNow = { 0 };
		if (m_fFreq)
		{
			if (QueryPerformanceCounter(&qpcNow))
			{
				if (m_nLastCounter)
				{
					m_nFramesSinceUpdate++;
					fps = m_fFreq * m_nFramesSinceUpdate / double(qpcNow.QuadPart - m_nLastCounter);
				}
			}
		}
		
		WCHAR szStatusMessage[128];
		StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" FPS: %0.2f    Record buffer size: %d    %s", fps, dataWriterBufferSize, m_pDataWriter ? L"=== RECORDING ===" : L"");

		if (SetStatusMessage(szStatusMessage, 1000, false))
		{
			m_nLastCounter = qpcNow.QuadPart;
			m_nFramesSinceUpdate = 0;
		}
	}
}

bool PersonTracker::SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce)
{
    INT64 now = GetTickCount64();

    if (m_hWnd && (bForce || (m_nNextStatusTime <= now)))
    {
        SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);
        m_nNextStatusTime = now + nShowTimeMsec;

        return true;
    }

    return false;
}

void PersonTracker::StartRecording()
{
	if (!m_pDataWriter)
	{
		WCHAR szBuffer[MAX_PATH];

		BOOL ret = GetDlgItemText(m_hWnd, IDC_STATIC_RECORD_PATH, szBuffer, MAX_PATH);

		std::string currentPath = ret ? ws2s(szBuffer) : "c:\\";

		m_pDataWriter = new DataWriter(currentPath, IMAGE_SEQUENCE_WRITER);
	}
}

void PersonTracker::StopRecording()
{
	if (m_pDataWriter)
	{
		delete m_pDataWriter;
		m_pDataWriter = NULL;
	}
}

FrameData PersonTracker::CreateAndValidateFrame(RGBQUAD* pBuffer, int nWidth, int nHeight, int nBodyCount, IBody** ppBodies)
{
	FrameData data;

	if (!pBuffer || nWidth != cColorWidth || nHeight != cColorHeight || !ppBodies)
	{
		return data;
	}

	for (int i = 0; i < nBodyCount; ++i)
	{
		IBody* pBody = ppBodies[i];
		if (pBody)
		{
			BOOLEAN bTracked = false;
			HRESULT hr = pBody->get_IsTracked(&bTracked);

			if (SUCCEEDED(hr) && bTracked)
			{
				Joint joints[JointType_Count];

				hr = pBody->GetJoints(_countof(joints), joints);
				if (SUCCEEDED(hr))
				{
					PersonData person;

					for (int j = 0; j < _countof(joints); ++j)
					{
						D2D1_POINT_2F point = BodyToScreen(joints[j].Position, m_pCoordinateMapper);
						
						person.appendJoint(JointData(
							point.x * m_jointScaleX, 
							point.y * m_jointScaleY, 
							joints[j].TrackingState));
					}
					
					if (_countof(joints) == person.joints.size())
					{
						data.appendPerson(person);
					}
				}
			}
		}
	}

	if (data.persons.size() > 0)
	{
		data.frame = cv::Mat(nHeight, nWidth, CV_8UC4, reinterpret_cast<void*>(pBuffer)).clone();
	}

	return data;
}