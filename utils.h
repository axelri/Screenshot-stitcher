#ifndef _UTILS_H_
#define _UTILS_H_

#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#include <jansson.h>

typedef struct FFMPEG_tmp {
    AVFrame *frame;
    AVFormatContext *fctx;
    AVCodecContext  *cctx;
    AVCodec         *c;
} FFMPEG_tmp;

char * get_video_json_filename(char *base);
char * get_video_folder(char *base);
char * get_touch_folder(char *base);
char * get_touch_json_file(char *base);

FFMPEG_tmp * picture_to_frame(char *filepath);

AVFrame * copy_frame(AVFrame *frame);

void tmp_free(FFMPEG_tmp *tmp);

const char * get_first_picture(json_t *timestamps);

long get_base_time(json_t *timestamps);

int handle_screenshot(AVFormatContext *oc, AVStream *st,
                      struct SwsContext *s_ctx, int frame_count,
                      long time_interval, char* filepath, TouchActualizer *ta,
                      int fps, long base_time);

#endif
