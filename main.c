#include <math.h>
#include <stdio.h>

#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <jansson.h>

#include "video.h"
#include "json.h"
#include "utils.h"
#include "actualizer.h"

#define FPS 25
#define OUT_CODEC AV_CODEC_ID_H264
#define SCALE_METHOD SWS_BILINEAR
#define BIT_RATE 400000 /* does not apply to H264 */
#define PIX_FMT_OUT AV_PIX_FMT_YUV420P /* TODO: use another faster pix format? */

int main(int argc, char *argv[]) {
    /* file attributes */
    char              *basedir;
    char              *dst_filename;

    /* video format/container attributes */
    AVFormatContext   *oc;

    /* video stream attributes */
    AVStream          *video_st;
    AVCodecContext    *codec_ctx;
    struct SwsContext *sc;
    AVCodec           *video_codec;

    /* json parsing variables */
    char              *video_folder;
    char              *video_json_filename;
    json_t            *root_json;
    json_t            *timestamps;

    char              *touch_folder;
    char              *touch_json_filename;
    TouchActualizer   *ta;

    /* config variables */
    char              *first_pic_full;
    const char        *first_pic;
    AVFrame           *first_frame;
    int                width, height, pix_fmt;
    int                out_width, out_height;
    long               base_time;

    /* temporary state variables */
    int                ret, i, frame_count;
    FFMPEG_tmp         *tmp;

    if (argc < 3) {
        printf("Please provide an input folder and output file\n");
        return -1;
    }

    /* Read json data */
    basedir = argv[1];
    dst_filename = argv[2];

    video_folder = get_video_folder(basedir);
    video_json_filename = get_video_json_filename(video_folder);
    root_json = read_json(video_json_filename);
    timestamps = json_object_get(root_json, "timestamps");

    if(!json_is_array(timestamps)) {
        fprintf(stderr, "error: timestamps is not an array\n");
        json_decref(root_json);
        exit(1);
    }

    /* set base time for sync */
    base_time = get_base_time(timestamps);

    /* Register codecs and open output files */
    av_register_all();

    /* get the config information from the first picture,
     * assume all other pictures follow the same format */
    first_pic = get_first_picture(timestamps);
    if (!asprintf(&first_pic_full, "%s/%s", video_folder, first_pic)) {
            fprintf(stderr, "Fatal: asprintf failure\n");
            exit(1);
    }
    tmp = picture_to_frame(first_pic_full);
    first_frame = tmp->frame;

    width = first_frame->width;
    height = first_frame->height;
    pix_fmt = first_frame->format;

    /* free temp codecs and frames */
    tmp_free(tmp);

    /* FFMPEG requires dimensions to be
     * multiple of 2 */
    out_width = width;
    if (out_width % 2 != 0) out_width += 1;
    out_height = height;
    if (out_height % 2 != 0) out_height += 1;

    /* allocate touch drawing context */
    touch_folder = get_touch_folder(basedir);
    touch_json_filename = get_touch_json_file(touch_folder);
    ta = TouchActualizer_new(touch_json_filename, width, height);

    /* allocate output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, dst_filename);
    if (!oc) {
        fprintf(stderr, "Could not deduce output format from file extension\n");
        exit(1);
    }

    /* Add the audio and video streams using the defined codecs */
    video_st = NULL;

    /* Fill codec and associate it with the output context */
    video_st = add_video_stream(oc, &video_codec, OUT_CODEC,
                                BIT_RATE, out_width, out_height,
                                FPS, PIX_FMT_OUT);

    codec_ctx = video_st->codec;
    ret = avcodec_open2(codec_ctx, video_codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

    #ifdef DEBUG_FMT
    av_dump_format(oc, 0, dst_filename, 1);
    #endif

    /* Open and prepare the output file */
    ret = avio_open(&oc->pb, dst_filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
        fprintf(stderr, "Could not open '%s': %s\n", dst_filename,
                av_err2str(ret));
        exit(1);
    }

    ret = avformat_write_header(oc, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
        exit(1);
    }

    /* Set up context for converting between
     * the picture and video format */
    sc = get_scale_ctx(width, height, pix_fmt,
                       out_width, out_height, PIX_FMT_OUT,
                       SCALE_METHOD);

    /* Read and write each screenshot to the video file */
    frame_count = 0;
    for(i = 0; i < (int)json_array_size(timestamps) - 1; i++) {

        json_t     *data, *next;
        json_t     *t1, *t2, *name;
        long        time_interval;
        const char *filename;
        char       *filepath;

        /* Interval is timestamp difference */
        data = json_array_get(timestamps, i);
        next = json_array_get(timestamps, i+1);

        t1 = json_object_get(data, "time");
        t2 = json_object_get(next, "time");
        time_interval = json_integer_value(t2) - json_integer_value(t1);
        name = json_object_get(data, "name");
        filename = json_string_value(name);
        if (!asprintf(&filepath, "%s/%s", video_folder, filename)) {
            fprintf(stderr, "Fatal: asprintf failure\n");
            exit(1);
        }

        /* handle each screenshot and update frame count */
        frame_count = handle_screenshot(oc, video_st, sc, frame_count,
                                        time_interval, filepath, ta,
                                        FPS, base_time);
        free(filepath);
    }

    /* Depending on the video codec, the actual
     * writing of frames can be delayed for optimization.
     * This forces all the delayed frames to be
     * written */
    flush_video(oc, video_st);

    /* Write file trailer, if any */
    av_write_trailer(oc);

    /* free temporary data */
    free(touch_folder);
    free(touch_json_filename);
    free(video_folder);
    free(video_json_filename);
    free(first_pic_full);

    /* free objects */
    TouchActualizer_destroy(ta);
    json_decref(root_json);

    if (video_st) avcodec_close(video_st->codec);
    sws_freeContext(sc);

    avio_close(oc->pb);
    avformat_free_context(oc);

    return 0;
}
