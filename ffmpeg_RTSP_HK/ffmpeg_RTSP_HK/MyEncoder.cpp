#include "MyEncoder.h"

MyEncoder::MyEncoder()
{
}

void MyEncoder::Ffmpeg_Encoder_Init()
{
	av_register_all();
	avcodec_register_all();
	avformat_network_init();

	m_pRGBFrame = new AVFrame[1];//RGB帧数据赋值    
	m_pYUVFrame = new AVFrame[1];//YUV帧数据赋值    

	c = NULL;//解码器指针对象赋初值  
}

void MyEncoder::Ffmpeg_Encoder_Setpara(AVCodecID mycodeid)
{
	fmtctx = avformat_alloc_context();
	fmt = av_guess_format(NULL, out_filename, NULL);
	fmtctx->oformat = fmt;
	avformat_alloc_output_context2(&fmtctx, NULL, "rtsp", out_filename);

	pCodecH264 = avcodec_find_encoder(mycodeid);//查找h264编码器  
	if (!pCodecH264)
	{
		fprintf(stderr, "h264 codec not found\n");
		exit(1);
	}

	avio_open(&fmtctx->pb, out_filename, AVIO_FLAG_WRITE);//打开流

	width = 1920;
	height = 1080;

	video_st = avformat_new_stream(fmtctx, pCodecH264);
	video_st->time_base.num = 1;
	video_st->time_base.den = 25;//帧率

	c = video_st->codec;
	avcodec_get_context_defaults3(c, pCodecH264);

	//c = avcodec_alloc_context3(pCodecH264);//函数用于分配一个AVCodecContext并设置默认值，如果失败返回NULL，并可用av_free()进行释放  
	c->codec_id = mycodeid;
	c->codec_tag = 0;
	c->codec_type = AVMEDIA_TYPE_VIDEO;
	c->bit_rate = 400000; //设置采样参数，即比特率  
	c->width = width;//设置编码视频宽度   
	c->height = height; //设置编码视频高度  
	c->time_base.den = 25;//设置帧率,num为分子和den为分母，如果是1/25则表示25帧/s  
	c->time_base.num = 1;
	c->gop_size = 10; //设置GOP大小,该值表示每10帧会插入一个I帧  
	c->max_b_frames = 1;//设置B帧最大数,该值表示在两个非B帧之间，所允许插入的B帧的最大帧数  
	c->pix_fmt = AV_PIX_FMT_YUV420P;//设置像素格式  

	av_opt_set(c->priv_data, "tune", "zerolatency", 0);//设置编码器的延时，解决前面的几十帧不出数据的情况  
	av_opt_set(c->priv_data, "preset", "slow", 0);
	av_opt_set(c->priv_data, "tune", "zerolatency", 0);
	av_opt_set(c->priv_data, "x264opts", "crf=26:vbv-maxrate=728:vbv-bufsize=3640:keyint=25", 0);

	if (avcodec_open2(video_st->codec, pCodecH264, NULL) < 0)return;//打开编码器  
	avformat_write_header(fmtctx, NULL);

	nDataLen = width*height * 3;//计算图像rgb数据区长度  

	yuv_buff = new uint8_t[nDataLen / 2];//初始化数据区，为yuv图像帧准备填充缓存  
	rgb_buff = new uint8_t[nDataLen];//初始化数据区，为rgb图像帧准备填充缓存  
	outbuf_size = 100000;////初始化编码输出数据区  
	outbuf = new uint8_t[outbuf_size];

	scxt = sws_getContext(c->width, c->height, AV_PIX_FMT_BGR24, c->width, c->height, AV_PIX_FMT_YUV420P, SWS_POINT, NULL, NULL, NULL);//初始化格式转换函数  
}

void MyEncoder::Ffmpeg_Encoder_Encode(FILE *file, uint8_t *data)
{
	memcpy(rgb_buff, data, nDataLen);//拷贝图像数据到rgb图像帧缓存中准备处理  
	avpicture_fill((AVPicture*)m_pRGBFrame, (uint8_t*)rgb_buff, AV_PIX_FMT_RGB24, width, height);//将rgb_buff填充到m_pRGBFrame  
																								 //av_image_fill_arrays((AVPicture*)m_pRGBFrame, (uint8_t*)rgb_buff, AV_PIX_FMT_RGB24, width, height);
	avpicture_fill((AVPicture*)m_pYUVFrame, (uint8_t*)yuv_buff, AV_PIX_FMT_YUV420P, width, height);//将yuv_buff填充到m_pYUVFrame  
	sws_scale(scxt, m_pRGBFrame->data, m_pRGBFrame->linesize, 0, c->height, m_pYUVFrame->data, m_pYUVFrame->linesize);// 将RGB转化为YUV  
	
	int myoutputlen = 0;
	av_init_packet(&pkt);
	pkt.data = NULL;    // packet data will be allocated by the encoder
	pkt.size = 0;
	pkt.pts = AV_NOPTS_VALUE;
	pkt.dts = AV_NOPTS_VALUE;
	m_pYUVFrame->pts = video_st->codec->frame_number;
	int returnvalue = avcodec_encode_video2(c, &pkt, m_pYUVFrame, &myoutputlen);


	if (returnvalue < 0)
	{
		fprintf(stderr, "Error encoding video frame:\n");
		exit(1);
	}
	if (returnvalue == 0)
	{
		fwrite(pkt.data, 1, pkt.size, file);
	}
	if (myoutputlen)
	{
		if (c->coded_frame->key_frame)
			pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = video_st->index;

		//计算PTS
		if (pkt.pts != AV_NOPTS_VALUE)
		{
			pkt.pts = av_rescale_q(pkt.pts, video_st->codec->time_base, video_st->time_base);
		}
		if (pkt.dts != AV_NOPTS_VALUE)
		{
			pkt.dts = av_rescale_q(pkt.dts, video_st->codec->time_base, video_st->time_base);
		}
		//写入一个AVPacket到输出文件, 这里是一个输出流
		av_interleaved_write_frame(fmtctx, &pkt);
	}
	av_free_packet(&pkt);
}

void MyEncoder::Ffmpeg_Encoder_Close()
{
	av_write_trailer(fmtctx);
	delete[]m_pRGBFrame;
	delete[]m_pYUVFrame;
	delete[]rgb_buff;
	delete[]yuv_buff;
	delete[]outbuf;
	sws_freeContext(scxt);
	avcodec_close(c);//关闭编码器  
	av_free(c);
	avio_close(fmtctx->pb);
	avformat_free_context(fmtctx);
}