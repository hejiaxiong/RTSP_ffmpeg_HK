#pragma once
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include<libavfilter/avfilter.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include "libavutil/time.h" 
}

class MyEncoder
{
public:
	MyEncoder();
public:
	AVOutputFormat *fmt;
	const char  *out_filename = "rtsp://10.138.197.133:554/123362.sdp";
	AVFormatContext *fmtctx;
	AVStream *video_st;
	AVFrame *m_pRGBFrame;   //帧对象  
	AVFrame *m_pYUVFrame;   //帧对象  
	AVCodec *pCodecH264;    //编码器  
	AVCodecContext *c;      //编码器数据结构对象  
	uint8_t *yuv_buff;      //yuv图像数据区  
	uint8_t *rgb_buff;      //rgb图像数据区  
	SwsContext *scxt;       //图像格式转换对象  
	uint8_t *outbuf;        //编码出来视频数据缓存  
	int outbuf_size;        //编码输出数据去大小  
	int nDataLen;           //rgb图像数据区长度  
	int width;              //输出视频宽度  
	int height;             //输出视频高度  
	AVPacket pkt;            //数据包结构体
public:
	void Ffmpeg_Encoder_Init();//初始化  
	void Ffmpeg_Encoder_Setpara(AVCodecID mycodeid);//设置参数,第一个参数为编码器,第二个参数为压缩出来的视频的宽度，第三个视频则为其高度  
	void Ffmpeg_Encoder_Encode(FILE *file, uint8_t *data);//编码并写入数据到文件  
	void Ffmpeg_Encoder_Close();//关闭 
};
