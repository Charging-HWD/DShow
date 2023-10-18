#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <dshow.h>
#include <comdef.h>
#include <atlbase.h>
#include "opencv2/opencv.hpp"
#include "Common/qedit.h"

#define ReleaseInterface(x) \
	if ( nullptr != x ) \
{ \
	x->Release( ); \
	x = nullptr; \
}

struct Size
{
    int nWidth = 640;
    int nHeight = 480;
};

class USBCamera
{
public:
    bool Start(int nIndex);
    bool Close();
    void getImage(cv::Mat& img);
    bool GetVideoCapbility(std::map<std::string,int>& VideoCapbility);
    bool SetVideoCapbility(std::map<std::string, int> VideoCapbility);
    bool EnumCameraSize(std::vector<Size>& CameraSize);
    int EnumCameraName(std::vector<std::string>& CameraName);
private:
    bool Init();
    bool BindFilter(int nCameraIndex, IBaseFilter** pFilter);
    void FreeMediaType(AM_MEDIA_TYPE& mt);
    bool GetVideoCapbility(ICaptureGraphBuilder2* pBuilder, IBaseFilter* pVCap,
        int nIndex, int& nWidth, int& nHeight);
    bool SetVideoCapbility(ICaptureGraphBuilder2* pBuilder, IBaseFilter* pVCap, bool bInit, int width, int height);
    HRESULT GrabByteFrame();
    BYTE* GetByteBuffer() { return m_BYTEbuffer; }
private:
    IGraphBuilder* m_pGraphBuilder = nullptr;
    ICaptureGraphBuilder2* m_pCaptureGraphBuilder2 = nullptr;
    IMediaEventEx* m_pMediaEvent = nullptr;
    ISampleGrabber* m_pSampleGrabber = nullptr;
    IMediaControl* m_pMediaControl = nullptr;				//采集媒体使用接口
    IVideoWindow* m_pVideoWindow = nullptr;					//采集视屏使用接口
    IAMVideoProcAmp* m_pProcAmp = nullptr;			//用于调整视频对比度、亮度饱和度等
    CComPtr <ISampleGrabber> m_pGrabber;
    IBaseFilter* m_pBF = nullptr;

    AM_MEDIA_TYPE   mt;
    bool    m_bInitFlag = false;
    bool    m_bConnectFlag = false;
    bool    m_bOpenFlag = false;
    int     m_nCameraCount = 0;
    int     m_width = 640;
    int     m_height = 480;
    long    m_BufferSize;
    long*   m_Buffer = nullptr;
    BYTE*   m_BYTEbuffer = nullptr;
    std::vector<std::string> CameraName;
    std::vector<Size> CameraSize;
};


