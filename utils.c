#include <stdio.h>
#include <stdlib.h>

#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#include <jansson.h>

#include "video.h"
#include "json.h"
#include "utils.h"
#include "actualizer.h"

#define VIDEO_DATA_FILE "videodata.json"
#define TOUCH_DATA_FILE "touch.json"
#define VIDEO_FOLDER "Screen"
#define TOUCH_FOLDER "Touch"

/*
 * get_video_json_filename returns the filename
 * of the json file containing all the
 * screenshot information of one
 * testing session, according to the
 * project conventions
 */
char * get_video_json_filename(char *base) {
    char *filename;

    asprintf(&filename, "%s/%s", base, VIDEO_DATA_FILE);
    if (!filename) {
        fprintf(stderr, "Fatal: error in asprintf\n");
        exit(1);
    }

    return filename;
}

char * get_video_folder(char *base) {
    char *filename;

    asprintf(&filename, "%s/%s", base, VIDEO_FOLDER);
    if (!filename) {
        fprintf(stderr, "Fatal: error in asprintf\n");
        exit(1);
    }

    return filename;
}

char * get_touch_folder(char *base) {
    char *filename;

    asprintf(&filename, "%s/%s", base, TOUCH_FOLDER);
    if (!filename) {
        fprintf(stderr, "Fatal: error in asprintf\n");
        exit(1);
    }

    return filename;
}


char * get_touch_json_file(char *base) {
    char *filename;

    asprintf(&filename, "%s/%s", base, TOUCH_DATA_FILE);
    if (!filename) {
        fprintf(stderr, "Fatal: error in asprintf\n");
        exit(1);
    }

    return filename;
}

/*
 * picture_to_frame reads and decodes
 * the picture file
 *
 * side effects: allocates an AVFrame which
 * must be freed with av_free_frame
 */
FFMPEG_tmp * picture_to_frame(char *filepath) {

    AVFormatContext *fctx = NULL;
    int              stream_no;
    AVCodecContext  *cctx = NULL;
    AVCodec         *c = NULL;
    AVFrame         *frame = NULL;

    FFMPEG_tmp      *tmp = malloc(sizeof(FFMPEG_tmp));

    fctx = get_fcontext(filepath);
    if (fctx == NULL) {
        fprintf(stderr, "Fatal: could not open %s\n", filepath);
        avformat_close_input(&fctx);
        exit(1);
    }

    stream_no = get_video_stream(fctx);
    if (stream_no == -1) {
        fprintf(stderr, "Fatal: could not find video stream\n");
        avformat_close_input(&fctx);
        exit(1);
    }

    cctx = get_ccontext(fctx, stream_no);
    if (cctx == NULL) {
        fprintf(stderr, "Fatal: no codec context initialized\n");
        avcodec_close(cctx);
        avformat_close_input(&fctx);
        exit(1);
    }

    c = get_codec(cctx);
    if (c == NULL) {
        fprintf(stderr, "Fatal: could not open codec\n");
        avcodec_close(cctx);
        avformat_close_input(&fctx);
        exit(1);
    }

    frame = decode_picture(fctx, stream_no, cctx);
    if (frame == NULL) {
        avcodec_close(cctx);
        avformat_close_input(&fctx);
        fprintf(stderr, "Fatal: could not decode image\n");
        exit(1);
    }

    /* clean up successful run */
    tmp->frame = frame;
    tmp->fctx = fctx;
    tmp->cctx = cctx;
    tmp->c = c;

    return tmp;
}

void tmp_free(FFMPEG_tmp *tmp) {
    /*av_freep(&tmp->frame->data[0]);*/
    /* TODO: for some reason av_freep
     * screws up av_codec_close */
    avcodec_close(tmp->cctx);
    av_frame_free(&tmp->frame);
    avformat_close_input(&tmp->fctx);
    free(tmp);
}

const char * get_first_picture(json_t *timestamps) {
    json_t *data;
    json_t *name;

    data = json_array_get(timestamps, 0);
    name = json_object_get(data, "name");

    return json_string_value(name);
}

long get_base_time(json_t *timestamps) {
    json_t *base_data;
    json_t *base;

    base_data = json_array_get(timestamps, 0);
    base = json_object_get(base_data, "time");
    return json_integer_value(base);
}

/* handle_screenshots appends the screenshot to the video buffer */
int handle_screenshot(AVFormatContext *oc, AVStream *st,
                      struct SwsContext *s_ctx, int frame_count,
                      long time_interval, char* filepath, TouchActualizer *ta,
                      int fps, long base_time) {

    FFMPEG_tmp *tmp;
    AVFrame *in_frame;

    tmp = picture_to_frame(filepath);
    in_frame = tmp->frame;

    #ifdef DEBUG_FRAME
    printf("Begin writing picture\nCurrent frame: %d\n", frame_count);
    #endif

    frame_count = write_frame(oc, st, s_ctx, in_frame, ta,
            fps, frame_count, time_interval, base_time);
    tmp_free(tmp);

    #ifdef DEBUG_FRAME
    printf("End writing picture\nCurrent frame: %d\n", frame_count);
    #endif

    #ifdef DEBUG_FRAME
    printf("Read file %s and wrote interval %ld\n", filepath, time_interval);
    printf("Written %d frames\n", interval_to_frames(time_interval, 25));
    #endif

    return frame_count;
}
