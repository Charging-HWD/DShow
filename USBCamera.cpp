/*
1.����һ�� ISampleGrabber ��ʵ��
CComPtr< ISampleGrabber > m_pGrabber;
m_pGrabber.CoCreateInstance( CLSID_SampleGrabber );

2.����ý������
m_pGrabber->SetMediaType(...);

3.��ISampleGrabber��ӵ�Graph��
CComQIPtr< IBaseFilter, &IID_IBaseFilter > pGrabBase(m_pGrabber );
m_pGraph->AddFilter( pGrabBase, L"Grabber" );

4.���ûص�����
m_pGrabber->SetCallback( &mCB, 1 );

��Graph��ʼRun��ʱ��Graph����ÿ�յ�һ֡����ʱ����һ��BufferCB����ˣ���BufferCB����Ҫ����һ�����أ���ʼץ�� ͼƬ��ʱ�������أ�������ͼƬ�Ժ�رտ��ء�
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
		//���� pBuffer�е����ݣ������ݼ�Ϊץ�ĵ�ͼƬ  
		if (!pBuffer)
			return E_POINTER;
		std::unique_lock<std::mutex>lck(g_Mut);
		g_Image = cv::Mat(g_nCurHeight, g_nCurWidth, CV_8UC3, pBuffer).clone();
		cv::flip(g_Image, g_Image, 0);
		return 0;
	}
};
//����һ�� CSampleGrabberCB��ʵ�� 
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

    //1.����һ���б�ָ��,ͨ�� CoCreateInstance ����
    ICreateDevEnum* pCreateDevEnum = nullptr;
    hr = CoCreateInstance(  
        CLSID_SystemDeviceEnum,             //Com��������ʶ��(CLSID/GUID)��ȫ��Ψһ��ʾ��
        nullptr,                            //���Ϊ nullptr�������˶����Ǿۺ�ʽ����һ���֡�������� nullptr����ָ��ָ��һ���ۺ�ʽ�����IUnknown�ӿ�          
        CLSCTX_INPROC_SERVER,               //�����𣬿�ʹ��CLSCTXö������Ԥ�����ֵ��
        IID_ICreateDevEnum,                 //���ýӿڱ�ʶ�������������ͨ�š�
        (void**)&pCreateDevEnum);           //��������ָ��Com����ӿڵ�ַ��ָ�����
    if (hr != S_OK)
        return false;

    //2.������Ƶ��׽��Ӳ���豸�б�,����һ��IEnumMoniker�ӿڣ�
    IEnumMoniker* pEnumMon = nullptr;
    hr = pCreateDevEnum->CreateClassEnumerator( 
        CLSID_VideoInputDeviceCategory,     //Com��������ʶ��(CLSID/GUID)��ȫ��Ψһ��ʾ��
        &pEnumMon,                          //����ָ�� IEnumMoniker �ӿڵ�ַ��ָ�����
        0);                                 //λ��ʶ������Ϊ0�����о����еĹ�����Ŀ¼
    if (hr != S_OK)
        return false;

    //3.ʹ�� IEnumMoniker::Next �ӿ�ö�����е��豸��ʶ,����IMoniker::BindToObject����һ����ѡ���device���ϵ�filter������װ��filter������(CLSID,FriendlyName,DevicePath)
    ULONG cFetched;
    IMoniker* pM = nullptr;
    while (hr = pEnumMon->Next(1, &pM, &cFetched), hr == S_OK)
    {
        IPropertyBag* pBag = nullptr;
        hr = pM->BindToStorage( 
            nullptr, 
            nullptr, 
            IID_IPropertyBag, 
            (void**)&pBag);// BindToStorage����Է����豸��ʶ�����Լ���
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

		//��ö�ٵ���ͬ�ķֱ��� 
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

	//�Էֱ��ʽ�������
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
	// ����IGraphBuilder�ӿ�
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_pGraphBuilder);
	if (FAILED(hr))
		return false;

	// ����ICaptureGraphBuilder2�ӿ�
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

	//������Ƶ��ʽ
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

	//������ȾԤ��/����pin
	hr = m_pCaptureGraphBuilder2->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pBF, pGrabBase, NULL);
	if (FAILED(hr))
	{
		hr = m_pCaptureGraphBuilder2->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pBF, pGrabBase, NULL);
		if (FAILED(hr))
			return false;
	}

	//��ȡ��Ƶ����
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

	//���� ActiveMove Window ���ڣ�����λ�ò��ɸ��ģ����򲻻�����
	if (m_pVideoWindow != nullptr)
	{
		if (FAILED(m_pVideoWindow->put_AutoShow(OAFALSE)))
			std::cout << "Couldn't hide ActiveMove Window!" << std::endl;
	}

	//��ʼ��Ƶ��׽
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