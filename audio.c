/*=========================================================================\
* Copyright(C)2016 Chudai.
*
* File name    : audio.c
* Version      : v1.0.0
* Author       : 初代
* Date         : 2016/10/06
* Description  :
* Function list: 1.
*                2.
*                3.
* History      :
\*=========================================================================*/

/*-----------------------------------------------------------*
 * 头文件                                                    *
 *-----------------------------------------------------------*/
#define __STDC_CONSTANT_MACROS      //ffmpeg要求

#include "player.h"
#include "audio.h"

/*======================================================================\
* Author     (作者): i.sshe
* Date       (日期): 2016/10/06
* Others     (其他): 准备audio
\*=======================================================================*/
int prepare_audio(PlayerState *ps)
{
	ps->paudio_stream = ps->pformat_ctx->streams[ps->audio_stream_index];
	ps->paudio_codec_ctx = 	ps->paudio_stream->codec;
	ps->paudio_codec = avcodec_find_decoder(ps->paudio_codec_ctx->codec_id);
	if (ps->paudio_codec == NULL)
    {
         fprintf(ERR_STREAM, "Couldn't find audio decoder!!!!!!!\n");
         return -1;
    }
	//初始化AVCondecContext，以及进行一些处理工作。
	avcodec_open2(ps->paudio_codec_ctx, ps->paudio_codec, NULL);

    return 0;
}


/*======================================================================\
* Author     (作者): i.sshe
* Date       (日期): 2016/10/06
* Others     (其他): 播放声音
\*=======================================================================*/
int play_audio(PlayerState *ps)
{
    SDL_AudioSpec      wanted_spec;
    wanted_spec.freq      = ps->paudio_codec_ctx->sample_rate;
    wanted_spec.format    = AUDIO_S16SYS;
    wanted_spec.channels  = ps->paudio_codec_ctx->channels;
    wanted_spec.silence   = 0;
    wanted_spec.samples   = 1024;     //
    wanted_spec.callback  = audio_callback;
    wanted_spec.userdata  = ps; // ps->paudio_codec_ctx;

    if (SDL_OpenAudio(&wanted_spec, NULL) < 0)
    {
        fprintf(ERR_STREAM, "Couldn't open audio device\n");
        return -1;
    }

    SDL_PauseAudio(0);

    return 0;
}


/*======================================================================\
* Author     (作者): i.sshe
* Date       (日期): 2016/10/06
* Others     (其他): 音频回调函数，打开设备的时候会开线程调用
\*=======================================================================*/
uint8_t DataBuffer[10000];
int		Datalen = 0;

void audio_callback(void *userdata, uint8_t *stream, int len)
{
	PlayerState *ps = (PlayerState *)userdata;
	int 		send_data_size = 0;
	int 		audio_size = 0;

	SDL_memset(stream, 0, len);

	if (Datalen >= len)
	{
		SDL_MixAudio(stream, DataBuffer, len, SDL_MIX_MAXVOLUME);
		Datalen -= len;
		memcpy(DataBuffer, DataBuffer + len, Datalen);
		return;
	}
	else
	{
		audio_size = audio_decode_frame(ps);
		if (audio_size < 0)
		{
			printf("audio decode error,just return\n");
			return;
		}
		memcpy(DataBuffer + Datalen, ps->audio_buf, audio_size);
		Datalen += audio_size;
		if (Datalen >= len)
		{
			SDL_MixAudio(stream, DataBuffer, len, SDL_MIX_MAXVOLUME);
			Datalen -= len;
			memcpy(DataBuffer, DataBuffer + len, Datalen);
			return;
		}
	}
}

int audio_decode_frame(PlayerState *ps)
{
	uint8_t 			*audio_buf = ps->audio_buf;
	AVPacket           	packet;
	AVFrame            	*pframe;
	AVSampleFormat     	dst_sample_fmt;
	uint64_t           	dst_channel_layout;
	uint64_t           	dst_nb_samples;
	int                	convert_len;
	SwrContext 			*swr_ctx = NULL;
	int                	data_size;
	int 				ret = 0;

	pframe = av_frame_alloc();

	if (packet_queue_get(&ps->audio_packet_queue, &packet, 1) < 0)
	{
		av_frame_free(&pframe);
		ps->audioerrnum++;
		printf("audio get packet error,maybe queue empty\n");
		return -1;
	}

	if (packet.pts != AV_NOPTS_VALUE)
	{
		ps->audio_clock = packet.pts * av_q2d(ps->paudio_stream->time_base);
	}

	ret = avcodec_send_packet(ps->paudio_codec_ctx, &packet);
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
	{
		printf("send decode packet error\n");
		av_frame_free(&pframe);
		return -1;
	}

	ret = avcodec_receive_frame(ps->paudio_codec_ctx, pframe);
	if (ret < 0 && ret != AVERROR_EOF)
	{
		printf("receive decode frame error\n");
		av_frame_free(&pframe);
		return -1;
	}
	ps->audioerrnum = 0;

	if (packet.pts != AV_NOPTS_VALUE)
	{
		ps->audio_clock = packet.pts
		*av_q2d(ps->paudio_stream->time_base);
	}

	if (pframe->channels > 0 && pframe->channel_layout == 0)
	{
		pframe->channel_layout = av_get_default_channel_layout(pframe->channels);
	}
	else if (pframe->channels == 0 && pframe->channel_layout > 0)
	{
		pframe->channels = av_get_channel_layout_nb_channels(pframe->channel_layout);
	}

	dst_sample_fmt     = AV_SAMPLE_FMT_S16;
	dst_channel_layout = av_get_default_channel_layout(pframe->channels);
	swr_ctx = swr_alloc_set_opts(NULL, dst_channel_layout, dst_sample_fmt,
	pframe->sample_rate, pframe->channel_layout,
	(AVSampleFormat)pframe->format, pframe->sample_rate, 0, NULL);
	if (swr_ctx == NULL || swr_init(swr_ctx) < 0)
	{
		printf("swr set open or swr init error\n");
		av_frame_free(&pframe);
		return -1;
	}

	dst_nb_samples = av_rescale_rnd(
	swr_get_delay(swr_ctx, pframe->sample_rate) + pframe->nb_samples,
	pframe->sample_rate, pframe->sample_rate, AVRounding(1));

	convert_len = swr_convert(swr_ctx, &audio_buf, dst_nb_samples,
	(const uint8_t **)pframe->data, pframe->nb_samples);

	data_size = convert_len * pframe->channels * av_get_bytes_per_sample(dst_sample_fmt);

	av_frame_free(&pframe);
	swr_free(&swr_ctx);            //

	return data_size;
}


/*======================================================================\
* Author     (作者): i.sshe
* Date       (日期): 2016/10/08
* Others     (其他): 获取音频的当前时间
\*=======================================================================*/
double get_audio_clock(PlayerState *ps)
{
	long long bytes_per_sec = 0;
	double cur_audio_clock = 0.0;
	double cur_buf_pos = ps->audio_buf_index;

	//每个样本占2bytes。16bit
	bytes_per_sec = ps->paudio_stream->codec->sample_rate
	 			* ps->paudio_codec_ctx->channels * 2;

	if (bytes_per_sec != 0)
	{
		cur_audio_clock = ps->audio_clock +
	 				cur_buf_pos / (double)bytes_per_sec;
	}
	return cur_audio_clock;
}

