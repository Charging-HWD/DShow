#include "VideoPlayer.h"

VideoPalyer::VideoPalyer()
{
	Init();
}

VideoPalyer::~VideoPalyer()
{
	UnInit();
}

bool VideoPalyer::Init()
{
	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr))
	{
		return false;
	}

	hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_pGraph);
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_pGraph->QueryInterface(IID_IMediaControl, (void**)&m_pControl);
	hr = m_pGraph->QueryInterface(IID_IMediaEvent, (void**)&m_pEvent);
	if (FAILED(hr))
	{
		return false;
	}
	m_bInitFlag = true;
	return true;
}

void VideoPalyer::UnInit()
{
	m_pControl->Release();
	m_pEvent->Release();
	m_pGraph->Release();
	CoUninitialize();
}

int VideoPalyer::Show_Filter_in_FilterGpragh(IGraphBuilder* pGraph)
{
	USES_CONVERSION;
	IEnumFilters* pFilterEnum = nullptr;
	if (FAILED(pGraph->EnumFilters(&pFilterEnum)))
	{
		pFilterEnum->Release();
		return -1;
	}
	pFilterEnum->Reset();
	IBaseFilter* filter = nullptr;
	ULONG fetchCount = 0;
	while (SUCCEEDED(pFilterEnum->Next(1, &filter, &fetchCount)) && fetchCount)
	{
		if (!filter)
		{
			continue;
		}
		FILTER_INFO FilterInfo;
		if (FAILED(filter->QueryFilterInfo(&FilterInfo)))
		{
			continue;
		}
		std::cout << "Info:" << W2A(FilterInfo.achName) <<std::endl;
		filter->Release();
	}
	pFilterEnum->Release();
	return 0;
}

bool VideoPalyer::GetInfo(const std::string& VideoName)
{
	if (!m_bInitFlag)
	{
		Init();
		if (!m_bInitFlag)
		{
			return false;
		}
	}
	HRESULT hr = m_pGraph->RenderFile(ConvertCharToLPWSTR(VideoName.c_str()), nullptr);
	if (FAILED(hr))
	{
		return false;
	}

	Show_Filter_in_FilterGpragh(m_pGraph);

	long video_w = 0, video_h = 0, video_bitrate = 0, audio_volume = 0;
	long long duration_1 = 0, position_1 = 0;
	REFTIME avgtimeperframe = 0;
	float framerate = 0, duration_sec = 0, progress = 0, position_sec = 0;

	//Video
	hr = m_pGraph->QueryInterface(IID_IBasicVideo, (void**)&m_pVideo);
	m_pVideo->get_VideoWidth(&video_w);
	m_pVideo->get_VideoHeight(&video_h);
	m_pVideo->get_AvgTimePerFrame(&avgtimeperframe);
	framerate = 1 / avgtimeperframe;
	m_pVideo->get_BitRate(&video_bitrate);

	std::cout << "Video Resolution:" << video_w << " x " << video_h << std::endl;
	std::cout << "Video Framerate:" << framerate << std::endl;
	std::cout << "Duration:" << duration_sec << std::endl;
}

bool VideoPalyer::ShowVideo(const std::string& VideoName,bool IsInfo)
{
	if (!m_bInitFlag)
	{
		Init();
		if (!m_bInitFlag)
		{
			return false;
		}
	}
	HRESULT hr = m_pGraph->RenderFile(ConvertCharToLPWSTR(VideoName.c_str()), nullptr);
	if (FAILED(hr)) 
	{
		return false;
	}

	long video_w = 0, video_h = 0, video_bitrate = 0, audio_volume = 0;
	long long duration_1 = 0, position_1 = 0;
	REFTIME avgtimeperframe = 0;
	float framerate = 0, duration_sec = 0, progress = 0, position_sec = 0;

	//Video
	hr = m_pGraph->QueryInterface(IID_IBasicVideo, (void**)&m_pVideo);
	m_pVideo->get_VideoWidth(&video_w);
	m_pVideo->get_VideoHeight(&video_h);
	m_pVideo->get_AvgTimePerFrame(&avgtimeperframe);
	framerate = 1 / avgtimeperframe;
	m_pVideo->get_BitRate(&video_bitrate);

	//Audio
	hr = m_pGraph->QueryInterface(IID_IBasicAudio, (void**)&m_pAudio);

	//Window
	hr = m_pGraph->QueryInterface(IID_IVideoWindow, (void**)&m_pWindow);
	const wchar_t* str = L"Simplest DirectShow Player"; // 待转换的字符串
	BSTR bstr = SysAllocString(str);					// 转换为BSTR类型
	m_pWindow->put_Caption(bstr);
	//m_pWindow->put_Width(480);
	//m_pWindow->put_Height(272);

	//Seek
	hr = m_pGraph->QueryInterface(IID_IMediaSeeking, (void**)&m_pSeeking);
	m_pSeeking->GetDuration(&duration_1);
	duration_sec = (float)duration_1 / 10000000.0;
	
	if (IsInfo)
	{
		std::cout << "Video Resolution:" << video_w << " x " << video_h << std::endl;
		std::cout << "Video Framerate:" << framerate << std::endl;
		std::cout << "Duration:" << duration_sec << std::endl;
	}
	if (SUCCEEDED(hr)) {
		hr = m_pControl->Run();
		if (SUCCEEDED(hr)) {
			long evCode = 0;
			while (evCode != EC_COMPLETE)
			{

				if (IsInfo)
				{
					m_pSeeking->GetCurrentPosition(&position_1);
					position_sec = (float)position_1 / 10000000.0;
					progress = position_sec * 100 / duration_sec;
					printf("%7.2fs\t%5.2f%%\n", position_sec, progress);
				}
				m_pEvent->WaitForCompletion(1000, &evCode);
			}
		}
	}
	SysFreeString(bstr);
}
