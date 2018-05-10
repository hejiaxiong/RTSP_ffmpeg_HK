// ffmpeg_RTSP_HK.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "Windows.h"
#include "HCNetSDK.h"
#include "plaympeg4.h"
#include <opencv2\opencv.hpp>
#include <time.h>
#include "MyEncoder.h"

using namespace std;
using namespace cv;

LONG nPort = -1;
double time1 = 0;
volatile int gbHandling = 3;
int cnt = 0;

MyEncoder myencoder;
FILE *f = NULL;


//解码回调 视频为YUV数据(YV12)，音频为PCM数据
void CALLBACK DecCBFun(long nPort, char * pBuf, long nSize, FRAME_INFO * pFrameInfo, long nReserved1, long nReserved2)
{
	if (gbHandling)
	{
		gbHandling--;
		return;
	}

	long lFrameType = pFrameInfo->nType;
	if (lFrameType == T_YV12)
	{
		Mat pImg(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3);
		Mat src(pFrameInfo->nHeight + pFrameInfo->nHeight / 2, pFrameInfo->nWidth, CV_8UC1, pBuf);
		cvtColor(src, pImg, CV_YUV2BGR_YV12);
		//Sleep(50);
		/*imshow("IPCamera", pImg);
		waitKey(10);*/

		for (int i = 0; i <10; i++)
		{
			putText(pImg, "Total using time:" + to_string((unsigned int)time1 * 1000) + "ms", Point(20, 80), FONT_HERSHEY_COMPLEX,
				1, Scalar(0, 0, 255), 2, CV_AA);
			myencoder.Ffmpeg_Encoder_Encode(f, (uchar*)pImg.data);//编码  
			imshow("IPCamera", pImg);
			waitKey(10);
		}
	}
	gbHandling = 3;
}

///实时流回调
void CALLBACK fRealDataCallBack(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void *pUser)
{
	switch (dwDataType)
	{
	case NET_DVR_SYSHEAD: //系统头
		if (!PlayM4_GetPort(&nPort))  //获取播放库未使用的通道号
		{
			break;
		}
		//m_iPort = lPort; //第一次回调的是系统头，将获取的播放库port号赋值给全局port，下次回调数据时即使用此port号播放
		if (dwBufSize > 0)
		{
			if (!PlayM4_SetStreamOpenMode(nPort, STREAME_REALTIME))  //设置实时流播放模式
			{
				break;
			}
			if (!PlayM4_OpenStream(nPort, pBuffer, dwBufSize, 10 * 1024 * 1024)) //打开流接口
			{
				break;
			}
			if (!PlayM4_SetDecCallBack(nPort, DecCBFun))
			{
				break;
			}
			if (!PlayM4_Play(nPort, NULL)) //播放开始
			{
				break;
			}
		}
		break;
	case NET_DVR_STREAMDATA:   //码流数据
		while (dwBufSize > 0 && nPort != -1)
		{
			while (!PlayM4_InputData(nPort, pBuffer, dwBufSize))
			{
				if (PlayM4_GetLastError(nPort) == PLAYM4_BUF_OVER)
				{
					Sleep(5);
					//cout << "a";
					continue;
				}
				cout << "error" << PlayM4_GetLastError(nPort) << endl;
				break;
			}
			break;

		}
		break;
	default: //其他数据
		if (dwBufSize > 0 && nPort != -1)
		{
			if (!PlayM4_InputData(nPort, pBuffer, dwBufSize))
			{
				break;
			}
		}
		break;
	}
}
void CALLBACK g_ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
{
	char tempbuf[256] = { 0 };
	switch (dwType)
	{
	case EXCEPTION_RECONNECT:    //预览时重连
		printf("----------reconnect--------%d\n", time(NULL));
		break;
	default:
		break;
	}
}

void main()
{

	//---------------------------------------
	// 初始化
	NET_DVR_Init();
	//设置连接时间与重连时间
	NET_DVR_SetConnectTime(2000, 1);
	NET_DVR_SetReconnect(10000, true);


	//---------------------------------------
	// 注册设备
	LONG lUserID;
	NET_DVR_DEVICEINFO_V30 struDeviceInfo;
	lUserID = NET_DVR_Login_V30("169.254.83.244", 8000, "admin", "admin12345", &struDeviceInfo);
	if (lUserID < 0)
	{
		printf("Login error, %d\n", NET_DVR_GetLastError());
		NET_DVR_Cleanup();
		return;
	}
	//---------------------------------------
	//设置异常消息回调函数
	NET_DVR_SetExceptionCallBack_V30(0, NULL, g_ExceptionCallBack, NULL);

	char * filename = "myData.h264";
	fopen_s(&f, filename, "ab");//打开文件存储编码完成数据  
	myencoder.Ffmpeg_Encoder_Init();//初始化编码器  
	myencoder.Ffmpeg_Encoder_Setpara(AV_CODEC_ID_H264);//设置编码器参数  
																			 //图象编码  
	//---------------------------------------
	//启动预览并设置回调数据流
	LONG lRealPlayHandle;
	//cvNamedWindow("Mywindow", 0);
	cvNamedWindow("IPCamera", 1);
	HWND  h = (HWND)cvGetWindowHandle("IPCamera");
	if (h == 0)
	{
		cout << "窗口创建失败" << endl;
	}

	NET_DVR_PREVIEWINFO struPlayInfo = { 0 };
	struPlayInfo.hPlayWnd = h;         //需要SDK解码时句柄设为有效值，仅取流不解码时可设为空
	struPlayInfo.lChannel = 1;           //预览通道号
	struPlayInfo.dwStreamType = 0;       //0-主码流，1-子码流，2-码流3，3-码流4，以此类推
	struPlayInfo.dwLinkMode = 0;         //0- TCP方式，1- UDP方式，2- 多播方式，3- RTP方式，4-RTP/RTSP，5-RSTP/HTTP

	lRealPlayHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, fRealDataCallBack, NULL);
	if (lRealPlayHandle < 0)
	{
		printf("NET_DVR_RealPlay_V40 error\n");
		printf("%d\n", NET_DVR_GetLastError());
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}
	waitKey();
	Sleep(-1);

	myencoder.Ffmpeg_Encoder_Close();
	fclose(f);

	//---------------------------------------
	//关闭预览
	NET_DVR_StopRealPlay(lRealPlayHandle);
	//注销用户
	NET_DVR_Logout(lUserID);
	//释放SDK资源
	NET_DVR_Cleanup();

	return;
}

