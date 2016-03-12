#ifndef _VIDEO_H_
#define _VIDEO_H_

#include "actualizer.h"

#include <libavformat/avformat.h>

int interval_to_frames(long interval, int fps);

long pts_to_timestamp(long base, int pts, int fps);

int get_video_stream(AVFormatContext *fctx);

AVFormatContext * get_fcontext(const char *filename);

AVCodecContext * get_ccontext(AVFormatContext *fctx, int stream_no);

AVCodec * get_codec(AVCodecContext *cctx);

AVFrame * decode_picture(AVFormatContext * fctx, int stream_no,
                     AVCodecContext *cctx);

int get_frames(long interval, int fps);

struct SwsContext * get_scale_ctx(int in_w, int in_h, int in_f, int out_w,
                                  int out_h, int out_f, int filer);

AVFrame * alloc_frame(int width, int height, int pix_fmt);

int write_frame(AVFormatContext *oc, AVStream *st,
                struct SwsContext *sc, AVFrame *in_frame, TouchActualizer *ta,
                int fps, int pts, long interval, long base);

AVStream *add_video_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id,
                     int bit_rate, int width, int height,
                     int fps, int pix_fmt);

void flush_video(AVFormatContext *oc, AVStream *st);

void write_end_code(FILE *f);

#endif
