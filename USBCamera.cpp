/*
1.创建一个 ISampleGrabber 的实例
CComPtr< ISampleGrabber > m_pGrabber;
m_pGrabber.CoCreateInstance( CLSID_SampleGrabber );

2.设置媒体类型
m_pGrabber->SetMediaType(...);

3.把ISampleGrabber添加到Graph中
CComQIPtr< IBaseFilter, &IID_IBaseFilter > pGrabBase(m_pGrabber );
m_pGraph->AddFilter( pGrabBase, L"Grabber" );

4.设置回调函数
m_pGrabber->SetCallback( &mCB, 1 );

当Graph开始Run的时候，Graph会在每收到一帧数据时调用一次BufferCB，因此，在BufferCB中需要设置一个开关，开始抓拍 图片的时候开启开关，保存完图片以后关闭开关。
*/

#include "USBCamera.h"

int g_nCurWidth;
int g_nCurHeight;
cv::Mat g_Image;
std::mutex g_Mut;
class CSampleGrabberCB : public ISampleGrabberCB
{
public:
	long lWidth;
	long lHeight;
	int m_nInterval;
	TCHAR m_szFileName[MAX_PATH];
	CSampleGrabberCB()
	{   
		m_nInterval = 0;
	}
	STDMETHODIMP_(ULONG) AddRef() { return 2; }
	STDMETHODIMP_(ULONG) Release() { return 1; }
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		if (riid == IID_ISampleGrabberCB || riid == IID_IUnknown)
		{
			*ppv = (void*) static_cast<ISampleGrabberCB*> (this);
			return NOERROR;
		}
		return E_NOINTERFACE;
	}

	STDMETHODIMP SampleCB(double SampleTime, IMediaSample* pSample)
	{
		return 0;
	}

	STDMETHODIMP BufferCB(double dblSampleTime, BYTE* pBuffer, long lBufferSize)
	{
		//保存 pBuffer中的数据，此数据即为抓拍的图片  
		if (!pBuffer)
			return E_POINTER;
		std::unique_lock<std::mutex>lck(g_Mut);
		g_Image = cv::Mat(g_nCurHeight, g_nCurWidth, CV_8UC3, pBuffer).clone();
		cv::flip(g_Image, g_Image, 0);
		return 0;
	}
};
//创建一个 CSampleGrabberCB的实例 
CSampleGrabberCB g_sampleGrabberCB;


bool USBCamera::GetVideoCapbility(std::map<std::string, int>& VideoCapbility)
{
	return true;
}

bool USBCamera::SetVideoCapbility(std::map<std::string, int> VideoCapbility)
{
	return true;
}

int USBCamera::EnumCameraName(std::vector<std::string>& CameraName)
{
	CameraName.clear();
    HRESULT hr = CoInitialize(nullptr);
    if (hr != S_OK)
        return false;

    //1.申明一个列表指针,通过 CoCreateInstance 建立
    ICreateDevEnum* pCreateDevEnum = nullptr;
    hr = CoCreateInstance(  
        CLSID_SystemDeviceEnum,             //Com对象的类标识符(CLSID/GUID)，全局唯一标示符
        nullptr,                            //如果为 nullptr，表明此对象不是聚合式对象一部分。如果不是 nullptr，则指针指向一个聚合式对象的IUnknown接口          
        CLSCTX_INPROC_SERVER,               //组件类别，可使用CLSCTX枚举器中预定义的值。
        IID_ICreateDevEnum,                 //引用接口标识符，用来与对象通信。
        (void**)&pCreateDevEnum);           //用来接收指向Com对象接口地址的指针变量
    if (hr != S_OK)
        return false;

    //2.建立视频捕捉的硬件设备列表,申明一个IEnumMoniker接口，
    IEnumMoniker* pEnumMon = nullptr;
    hr = pCreateDevEnum->CreateClassEnumerator( 
        CLSID_VideoInputDeviceCategory,     //Com对象的类标识符(CLSID/GUID)，全局唯一标示符
        &pEnumMon,                          //接收指向 IEnumMoniker 接口地址的指针变量
        0);                                 //位标识符，若为0，则列举所有的过滤器目录
    if (hr != S_OK)
        return false;

    //3.使用 IEnumMoniker::Next 接口枚举所有的设备标识,调用IMoniker::BindToObject建立一个和选择的device联合的filter，并且装载filter的属性(CLSID,FriendlyName,DevicePath)
    ULONG cFetched;
    IMoniker* pM = nullptr;
    while (hr = pEnumMon->Next(1, &pM, &cFetched), hr == S_OK)
    {
        IPropertyBag* pBag = nullptr;
        hr = pM->BindToStorage( 
            nullptr, 
            nullptr, 
            IID_IPropertyBag, 
            (void**)&pBag);// BindToStorage后可以访问设备标识的属性集。
        if (S_OK == hr)
        {
            VARIANT varName;
            varName.vt = VT_BSTR;
            hr = pBag->Read(L"FriendlyName", &varName, NULL);
            if (hr == S_OK)
            {
                char name[2048] = { "" };
                WideCharToMultiByte(CP_ACP, 0, varName.bstrVal, -1, name, 2048, nullptr, nullptr);
				CameraName.push_back(name);
                SysFreeString(varName.bstrVal);
            }
            pBag->Release();
        }
        pM->Release();
    }
	CoUninitialize();
    pCreateDevEnum = nullptr;
    return CameraName.size();
}

bool USBCamera::EnumCameraSize(std::vector<Size>& CameraSize)
{
	CameraSize.clear();
	if (!m_bConnectFlag)
		return false;                                                              
	IAMStreamConfig* pSC = nullptr;
	HRESULT hr = NOERROR;
	hr = m_pCaptureGraphBuilder2->FindInterface(&PIN_CATEGORY_CAPTURE,
		&MEDIATYPE_Interleaved, m_pBF, IID_IAMStreamConfig, (void**)&pSC);

	if (hr != NOERROR)
		hr = m_pCaptureGraphBuilder2->FindInterface(&PIN_CATEGORY_CAPTURE,
			&MEDIATYPE_Video, m_pBF, IID_IAMStreamConfig, (void**)&pSC);

	int iCount, iSize;
	hr = pSC->GetNumberOfCapabilities(&iCount, &iSize);
	VIDEO_STREAM_CONFIG_CAPS scc;
	if (sizeof(scc) != iSize)
	{
		return false;
	}

	AM_MEDIA_TYPE* pMediaType = nullptr;
	for (int i = 0; i < iCount; i++)
	{
		hr = pSC->GetStreamCaps(i, &pMediaType, reinterpret_cast<BYTE*>(&scc));
		if (hr != S_OK)
			continue;

		if (!(pMediaType->formattype == FORMAT_VideoInfo))
			continue;

		Size size;
		size.nWidth = scc.InputSize.cx;
		size.nHeight = scc.InputSize.cy;

		//会枚举到相同的分辨率 
		bool bRes = true;
		for (size_t j = 0; j < CameraSize.size(); j++)
		{
			if (size.nWidth == CameraSize[j].nWidth &&
				size.nHeight == CameraSize[j].nHeight)
			{
				bRes = false;
			}
	
		}
		if(bRes)
			CameraSize.push_back(size);	
	}

	//对分辨率进行排序
	for (size_t i = 0; i < CameraSize.size(); i++)
	{
		for (size_t j = i + 1; j < CameraSize.size(); j++)
		{
			if (CameraSize[i].nWidth > CameraSize[j].nWidth)
			{
				Size temp = CameraSize[i];
				CameraSize[i] = CameraSize[j];
				CameraSize[j] = temp;
			}
			else if (CameraSize[i].nWidth == CameraSize[j].nWidth)
			{
				if (CameraSize[i].nHeight > CameraSize[j].nHeight)
				{
					Size temp = CameraSize[i];
					CameraSize[i] = CameraSize[j];
					CameraSize[j] = temp;
				}
			}
		}
	}
	return true;
}

bool USBCamera::SetVideoCapbility(ICaptureGraphBuilder2* pBuilder, IBaseFilter* pVCap, bool bInit, int width, int height)
{
	IAMStreamConfig* pSC = nullptr;
	HRESULT hr = pBuilder->FindInterface(
		&PIN_CATEGORY_CAPTURE,
		&MEDIATYPE_Interleaved, 
		pVCap,
		IID_IAMStreamConfig, 
		(void**)&pSC);

	if (hr != NOERROR)
		hr = pBuilder->FindInterface(
			&PIN_CATEGORY_CAPTURE,
			&MEDIATYPE_Video, 
			pVCap,
			IID_IAMStreamConfig, 
			(void**)&pSC);

	VIDEO_STREAM_CONFIG_CAPS scc;
	AM_MEDIA_TYPE* pmt = nullptr;
	int iCount = 0, iSize = 0;
	hr = pSC->GetNumberOfCapabilities(&iCount, &iSize);
	if (sizeof(scc) != iSize)
	{
		return false;
	}
	for (int i = 0; i < iCount; i++)
	{
		hr = pSC->GetStreamCaps(i, &pmt, reinterpret_cast<BYTE*>(&scc));
		if (hr != S_OK)continue;
		if (!(pmt->formattype == FORMAT_VideoInfo))continue;


		int nWidth;
		int nHeight;
		if (bInit)
		{
			int nSize = (int)CameraSize.size();
			nWidth = CameraSize[nSize - 1].nWidth;
			nHeight = CameraSize[nSize - 1].nHeight;
		}
		else
		{
			nWidth = width;
			nHeight = height;
		}
		VIDEOINFOHEADER* pVih = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
		pVih->bmiHeader.biWidth = nWidth;
		pVih->bmiHeader.biHeight = nHeight;
		int cbPixel = 2;
		pmt->lSampleSize = pVih->bmiHeader.biSizeImage =
			((width + 3) & ~3) * height * cbPixel;
		hr = pSC->SetFormat(pmt);

		FreeMediaType(*pmt);
		if (FAILED(hr))return true;

		return TRUE;
	}
	return false;
}

bool USBCamera::GetVideoCapbility(ICaptureGraphBuilder2* pBuilder, IBaseFilter* pVCap,int nIndex, int& nWidth, int& nHeight)
{
	int              iCount, iSize;
	IAMStreamConfig* pSC = nullptr;
	HRESULT hr = pBuilder->FindInterface(
		&PIN_CATEGORY_CAPTURE,
		&MEDIATYPE_Interleaved, pVCap,
		IID_IAMStreamConfig, 
		(void**)&pSC);

	if (hr != NOERROR)
		hr = pBuilder->FindInterface(
			&PIN_CATEGORY_CAPTURE,
			&MEDIATYPE_Video,
			pVCap,
			IID_IAMStreamConfig, 
			(void**)&pSC);

	VIDEO_STREAM_CONFIG_CAPS scc;
	AM_MEDIA_TYPE* pmt = nullptr;
	hr = pSC->GetNumberOfCapabilities(&iCount, &iSize);
	if (sizeof(scc) != iSize)
	{
		return false;
	}
	for (int i = 0; i < iCount; i++)
	{
		hr = pSC->GetStreamCaps(i, &pmt, reinterpret_cast<BYTE*>(&scc));
		if (hr != S_OK)continue;
		if (!(pmt->formattype == FORMAT_VideoInfo))
			continue;

		if (i == nIndex)
		{
			nWidth = scc.OutputGranularityX;
			nHeight = scc.OutputGranularityY;
			return true;
		}
	}
	return false;
}

bool USBCamera::Init()
{
	HRESULT hr = CoInitialize(nullptr);
	if (hr != S_OK)
		return false;
	// 创建IGraphBuilder接口
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_pGraphBuilder);
	if (FAILED(hr))
		return false;

	// 创建ICaptureGraphBuilder2接口
	hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC, IID_ICaptureGraphBuilder2, (void**)&m_pCaptureGraphBuilder2);
	if (FAILED(hr))
		return false;

	m_pCaptureGraphBuilder2->SetFiltergraph(m_pGraphBuilder);

	hr = m_pGraphBuilder->QueryInterface(IID_IMediaControl, (void**)&m_pMediaControl);
	if (FAILED(hr))
		return false;

	hr = m_pGraphBuilder->QueryInterface(IID_IVideoWindow, (LPVOID*)&m_pVideoWindow);
	if (FAILED(hr))
		return false;
	m_bInitFlag = true;
	return true;
}

bool USBCamera::BindFilter(int deviceId, IBaseFilter** pFilter)
{
	if (deviceId < 0) 
		return false;

	CComPtr<ICreateDevEnum> pCreateDevEnum;
	HRESULT hr = CoCreateInstance(
		CLSID_SystemDeviceEnum, 
		nullptr, 
		CLSCTX_INPROC_SERVER,
		IID_ICreateDevEnum, 
		(void**)&pCreateDevEnum);
	if (hr != NOERROR)
		return false;
	CComPtr<IEnumMoniker> pEm;
	hr = pCreateDevEnum->CreateClassEnumerator(
		CLSID_VideoInputDeviceCategory, 
		&pEm, 
		0);
	if (hr != NOERROR) 
		return false;
	pEm->Reset();
	ULONG cFetched;
	IMoniker* pM;
	int index = 0;
	while (hr = pEm->Next(1, &pM, &cFetched), hr == S_OK, index <= deviceId)
	{
		IPropertyBag* pBag;
		hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pBag);
		if (SUCCEEDED(hr))
		{
			VARIANT var;
			var.vt = VT_BSTR;
			hr = pBag->Read(L"FriendlyName", &var, NULL);
			if (hr == NOERROR)
			{
				if (index == deviceId)
				{
					pM->BindToObject(0, 0, IID_IBaseFilter, (void**)pFilter);
					m_bConnectFlag = true;
				}
				SysFreeString(var.bstrVal);
			}
			pBag->Release();
		}
		pM->Release();
		index++;
	}
	return true;
}

void USBCamera::FreeMediaType(AM_MEDIA_TYPE& mt)
{
	if (mt.cbFormat != 0)
	{
		CoTaskMemFree((PVOID)mt.pbFormat);
		// Strictly unnecessary but tidier
		mt.cbFormat = 0;
		mt.pbFormat = nullptr;
	}
	if (mt.pUnk != nullptr)
	{
		mt.pUnk->Release();
		mt.pUnk = nullptr;
	}
}

bool USBCamera::Start(int nIndex)
{
	if (!m_bInitFlag)
	{
		Init();
		if (!m_bInitFlag) 
		{
			return false;
		}
	}

	BindFilter(nIndex, &m_pBF);
	EnumCameraSize(CameraSize);
	SetVideoCapbility(m_pCaptureGraphBuilder2, m_pBF, 0, CameraSize[CameraSize.size()-1].nWidth, CameraSize[CameraSize.size() - 1].nHeight);
	HRESULT hr = m_pGraphBuilder->AddFilter(m_pBF, L"Capture Filter");
	hr = m_pBF->QueryInterface(IID_IAMVideoProcAmp, (void**)&m_pProcAmp);
	hr = m_pGrabber.CoCreateInstance(CLSID_SampleGrabber);
	if (!m_pGrabber)
		return false;

	CComQIPtr< IBaseFilter, &IID_IBaseFilter > pGrabBase(m_pGrabber);

	//设置视频格式
	AM_MEDIA_TYPE mt;
	ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
	mt.majortype = MEDIATYPE_Video;
	mt.subtype = MEDIASUBTYPE_RGB24;
	mt.subtype = MEDIASUBTYPE_RGB24;
	//mt.subtype = MEDIASUBTYPE_YUY2;
	//mt.subtype = MEDIASUBTYPE_MJPG;
	hr = m_pGrabber->SetMediaType(&mt);
	if (FAILED(hr))
		return false;

	hr = m_pGraphBuilder->AddFilter(pGrabBase, L"Grabber");
	if (FAILED(hr))
		return false;

	//尝试渲染预览/捕获pin
	hr = m_pCaptureGraphBuilder2->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pBF, pGrabBase, NULL);
	if (FAILED(hr))
	{
		hr = m_pCaptureGraphBuilder2->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pBF, pGrabBase, NULL);
		if (FAILED(hr))
			return false;
	}

	//获取视频属性
	hr = m_pGrabber->GetConnectedMediaType(&mt);
	if (FAILED(hr))
		return false;

	VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt.pbFormat;
	g_sampleGrabberCB.lWidth = vih->bmiHeader.biWidth;
	g_sampleGrabberCB.lHeight = vih->bmiHeader.biHeight;

	g_nCurWidth = vih->bmiHeader.biWidth;
	g_nCurHeight = vih->bmiHeader.biHeight;
	FreeMediaType(mt);
	hr = m_pGrabber->SetBufferSamples(FALSE);
	hr = m_pGrabber->SetOneShot(FALSE);
	hr = m_pGrabber->SetCallback(&g_sampleGrabberCB, 1);

	//隐藏 ActiveMove Window 窗口，代码位置不可更改，否则不会隐藏
	if (m_pVideoWindow != nullptr)
	{
		if (FAILED(m_pVideoWindow->put_AutoShow(OAFALSE)))
			std::cout << "Couldn't hide ActiveMove Window!" << std::endl;
	}

	//开始视频捕捉
	hr = m_pMediaControl->Run();
	if (FAILED(hr))
		std::cout << "Couldn't run the graph!" << std::endl;
	m_bOpenFlag = true;
	return true;
}

bool USBCamera::Close()
{
	m_bConnectFlag = false;
	if (m_bInitFlag)
	{
		m_bOpenFlag = false;
		if (m_pMediaControl)
		{
			m_pMediaControl->Stop();
		}
		if (m_pVideoWindow)
		{
			m_pVideoWindow->get_Visible(FALSE);
			m_pVideoWindow->put_Owner(NULL);
		}
		
		IEnumFilters* pEnum = nullptr;
		HRESULT hr = m_pGraphBuilder->EnumFilters(&pEnum);
		if (SUCCEEDED(hr))
		{
			IBaseFilter* pFilter = NULL;
			while (S_OK == pEnum->Next(1, &pFilter, NULL))
			{	
				m_pGraphBuilder->RemoveFilter(pFilter);
				pEnum->Reset();
				pFilter->Release();
			}
			pEnum->Release();
		}
		ReleaseInterface(m_pMediaControl);
		ReleaseInterface(m_pMediaEvent);
		ReleaseInterface(m_pSampleGrabber);
		ReleaseInterface(m_pVideoWindow);
		ReleaseInterface(m_pCaptureGraphBuilder2);
		ReleaseInterface(m_pGraphBuilder);
	}
	CoUninitialize();
	m_bConnectFlag = false;
	m_bOpenFlag = false;
	return true;
}

void USBCamera::getImage(cv::Mat& img)
{
	std::unique_lock<std::mutex>lck(g_Mut);
	img = g_Image.clone();
}