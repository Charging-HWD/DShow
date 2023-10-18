#pragma once
#include "Common.h"

class VideoPalyer
{
public:
	VideoPalyer();
	~VideoPalyer();
	bool GetInfo(const std::string& VideoName);
	bool ShowVideo(const std::string& VideoName, bool IsInfo);
private:
	bool Init();
	void UnInit();
	int Show_Filter_in_FilterGpragh(IGraphBuilder* pGraph);
private:
	bool m_bInitFlag = false;
	IGraphBuilder*	m_pGraph = nullptr;
	IMediaControl*  m_pControl = nullptr;
	IMediaEvent*	m_pEvent = nullptr;

	//Get some param--------------
	IBasicVideo*	m_pVideo = nullptr;
	IBasicAudio*	m_pAudio = nullptr;
	IVideoWindow*	m_pWindow = nullptr;
	IMediaSeeking*	m_pSeeking = nullptr;
};

