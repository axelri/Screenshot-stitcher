#include "video.h"
#include "actualizer.h"

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>

#define CRF "23" /* real quality setting for H264 */

/*
 * interval_to_frames returns the number of
 * frames in the interval in millisecs,
 * based on the fps
 */
int interval_to_frames(long interval, int fps) {
    double frame_per_millisec;
    int    frames;

    frame_per_millisec = (double)fps / 1000.0;
    /* round to neareast integer */
    frames = (int)(interval * frame_per_millisec + 0.5);
    return frames;
}

/*
 * pts_to_timestamp returns the current timestamp
 * based on the current framecount and the fps
 */
long pts_to_timestamp(long base, int pts, int fps) {
    double millisec_per_frame;
    long timestamp;

    millisec_per_frame = (double)1000.0 / fps;
    /* round to nearest timestamp in millisec */
    timestamp = base + (long)(pts * millisec_per_frame + 0.5);
    return timestamp;
}

#ifdef DEBUG_WRITE
/*
 * log_packet logs the packet attributes to stdout
 */
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt) {
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}
#endif

/*
 * write_packet writes the packet to the output format context,
 * and also converts the packet time to the correct container time
 */
int write_packet(AVFormatContext *fmt_ctx, const AVRational *time_base,
                AVStream *st, AVPacket *pkt) {

    /* rescale output packet timestamp values from codec to stream timebase */
    pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base,
                                AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base,
                                AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
    pkt->stream_index = st->index;

    #ifdef DEBUG_WRITE
    log_packet(fmt_ctx, pkt);
    #endif

    /* Write the compressed frame to the media file */
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/*
 * get_video_stream finds the video stream number
 * from the specified format context
 *
 * it returns -1 if a video stream can't be found,
 * and returns the video stream index otherwise
 */
int get_video_stream(AVFormatContext *fctx) {
    int i;

    for(i = 0; i < (int)fctx->nb_streams; i++) {
        if(fctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
            return i;
        }
    }

    /* found no video stream */
    return -1;
}

/*
 * get_fcontext determines and allocates
 * the format context that was automatically
 * detected for the input file
 *
 * returns NULL if the format context could not
 * be loaded, a valid pointer otherwise
 *
 * side effects: allocs AVFormatContext if successful
 * must free with avformat_close_input(&a)
 */
AVFormatContext * get_fcontext(const char *filename) {
    AVFormatContext *fctx = NULL;
    int ret;

    ret = avformat_open_input(&fctx, filename, NULL, NULL);
    if (ret != 0) {
        /* could not associate file with context */
        return NULL;
    }

    /* TODO: This seems to open codecs, must be closed */
    ret = avformat_find_stream_info(fctx, NULL);
    if (ret < 0) {
        /* could not find stream info, invalid stream */
        avformat_close_input(&fctx);
        return NULL;
    }

    return fctx;
}

/*
 * get_ccontext returns a reference to the
 * codec context of the specified format context
 * and stream number
 */
AVCodecContext * get_ccontext(AVFormatContext *fctx, int stream_no) {
    return fctx->streams[stream_no]->codec;
}

/*
 * get_codec finds and opens the the
 * codec for the codec context
 *
 * returns NULL if a codec could not be found
 * or could not be initialized
 * returns a valid pointer otherwise
 *
 * side effects: allocs and AVCodec if successful
 * parameter AVCodecContext a must be
 * freed with avcodec_close(a)
 */
AVCodec * get_codec(AVCodecContext *cctx) {
    AVCodec *c = NULL;
    int ret = 0;

    /* find the codec associated with the data stream */
    c = avcodec_find_decoder(cctx->codec_id);
    if (c == NULL) {
        return NULL;
    }

    ret = avcodec_open2(cctx, c, NULL);
    if (ret < 0) {
        return NULL;
    }

    return c;
}

/*
 * decode_picture returns a decoded picture in
 * the form of an AVFrame pointer
 * It should work for all common formats
 *
 * returns NULL if the picture could not be decoded
 *
 * side effects: allocs AVFrame a if successful
 * must be freed with av_free
 */
AVFrame * decode_picture(AVFormatContext * fctx, int stream_no,
                     AVCodecContext *cctx) {

    int got_frame = 0;
    int ret1 = 1;
    int ret2 = 0;

    AVFrame *frame = NULL;
    AVPacket pkt;

    /* allocate memory for the image frame */
    frame = av_frame_alloc();

    /* read frames (bits of the file) and try to decode the image */
    while(ret1 >= 0) {
        /* read one frame from the input file */
        ret1 = av_read_frame(fctx, &pkt);

        /* frame must be in desired file stream */
        if (pkt.stream_index != stream_no) {
            av_free_packet(&pkt);
            continue;
        }

        ret2 = avcodec_decode_video2(cctx, frame, &got_frame, &pkt);
        if (ret2 < 0) {
            /* can not decode frame, possible image corruption */
            return NULL;
        }

        /* end looping */
        if (got_frame) {
            ret1 = -1;
        }

        av_free_packet(&pkt);
    }

    if (got_frame) {
        return frame;
    }

    /* check for cached frames,
     * for some reason ffmpeg
     * sometimes saves frames internally */
    pkt.data = NULL;
    pkt.size = 0;

    ret2 = avcodec_decode_video2(cctx, frame, &got_frame, &pkt);
    if (ret2 < 0) {
        /* can not decode frame, possible image corruption */
        return NULL;
    }

    if (got_frame) {
        return frame;
    }

    /* could not decode whole image */
    av_frame_free(&frame);
    return NULL;
}

/*
 * get_scale_ctx allocates and returns a scaling
 * context to convert between two different kinds
 * of frames
 *
 * side effects: allocates a struct SwsContext
 * must be freed with sws_freeContext
 */
struct SwsContext * get_scale_ctx(int in_w, int in_h,
                                  int in_f, int out_w,
                                  int out_h, int out_f,
                                  int filter) {

    struct SwsContext *sws_ctx;

    sws_ctx = sws_getContext(in_w, in_h, in_f,
                             out_w, out_h, out_f,
                             filter, NULL, NULL, NULL);

    if (!sws_ctx) {
        fprintf(stderr, "Fatal: Could not allocate scaling context\n");
        exit(1);
    }

    return sws_ctx;
}

/*
 * add_video_stream allocates and returns a new video stream,
 * associated with the output context and the video codec
 *
 * side effects: "User is required to call avcodec_close() [on *oc]
 * and avformat_free_context() [on *codec] to clean up the allocation by
 * avformat_new_stream()" <-- from FFMPEG documentation
 */
AVStream * add_video_stream(AVFormatContext *oc, AVCodec **codec,
                            enum AVCodecID codec_id,
                            int bit_rate, int width, int height,
                            int fps, int pix_fmt) {
    AVCodecContext *c;
    AVStream *st;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }

    st = avformat_new_stream(oc, *codec);
    if (!st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    st->id = oc->nb_streams-1;
    c = st->codec;

    /* assumes that the stream is a video stream */

    /* TODO: explore multithread */
    c->codec_id = codec_id;

    c->bit_rate = bit_rate;
    /* Resolution must be a multiple of two. */
    c->width    = width;
    c->height   = height;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    c->time_base.den = fps;
    c->time_base.num = 1;
    /* emit one intra frame every twelve frames at most
     * TODO: explore gop_size */
    c->gop_size      = 20; /* Tune this for every changing screenshot */
    c->pix_fmt       = pix_fmt;

    /* H264 specific settings
     * TODO: tune this, can result in a major performance boost*/
    if (codec_id == AV_CODEC_ID_H264) {
        /* TODO: correct way to set several options? */
        av_opt_set(c->priv_data, "preset", "ultrafast", 0);
        av_opt_set(c->priv_data, "tune", "animation", 0);
        av_opt_set(c->priv_data, "log-level", "none", 0);
        /* set this instead of bit rate for H264
         * the codec will figure out a good bitrate
         * itself */
        c->bit_rate = 0;
        av_opt_set(c->priv_data, "crf", CRF, 0);
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

/*
 * alloc_frame allocates a frame with the specified
 * width, height and pixel format
 *
 * side effects: allocates an AVFrame, must be freed with
 * av_frame_free
 */
AVFrame * alloc_frame(int width, int height, int pix_fmt) {
    AVFrame *out_frame;
    int ret;

    out_frame = av_frame_alloc();
    if (!out_frame) {
        fprintf(stderr, "Fatal: Could not allocate output video frame\n");
        exit(1);
    }

    out_frame->width = width;
    out_frame->height = height;
    out_frame->format = pix_fmt;

    /* allocate the frame with 32 bit alignment */
    if ((ret = av_image_alloc(out_frame->data, out_frame->linesize,
                              width, height, pix_fmt, 32)) < 0) {
        fprintf(stderr, "Could not allocate destination image\n");
        exit(1);
    }

    return out_frame;
}

/*
 * write_frame appends the supplied frame to the destination video
 * file at the specified interval
 */
int write_frame(AVFormatContext *oc, AVStream *st,
                struct SwsContext *sc, AVFrame *in_frame, TouchActualizer *ta,
                int fps, int pts, long interval, long base) {

    AVCodecContext *c_ctx = st->codec;

    int      i, ret, got_output;
    int      frames, n_pts;
    long     time_end;
    /*long     time_start;*/
    AVPacket pkt;
    AVFrame *out_frame;
    Frame   *frame_data;

    frames = interval_to_frames(interval, fps);
    frame_data = Frame_new(in_frame->data[0], in_frame->linesize[0],
                in_frame->width, in_frame->height, 0);

    /* i = current total frame count */
    for (i = pts; i < (pts+frames); i++) {
        av_init_packet(&pkt);
        /* packet data will be allocated by the encoder */
        pkt.data = NULL;
        pkt.size = 0;
        fflush(stdout);

        /*time_start = pts_to_timestamp(base, i, fps);*/
        time_end = pts_to_timestamp(base, i+1, fps);

        frame_data->timestamp = time_end;
        /* draw touch data */
        actualize(ta, frame_data);

        out_frame = alloc_frame(c_ctx->width, c_ctx->height, c_ctx->pix_fmt);

        /* convert to destination format, ie YUV */
        sws_scale(sc, (const unsigned char *const *)in_frame->data,
                  (const int *)in_frame->linesize, 0, out_frame->height,
                  out_frame->data, out_frame->linesize);

        out_frame->pts = i;

        /* encode the image */
        ret = avcodec_encode_video2(c_ctx, &pkt, out_frame, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame %d\n", i);
            exit(1);
        }

        if (got_output) {

            #ifdef DEBUG_WRITE
            printf("Write frame %3d (size=%5d)\n", i, pkt.size);
            #endif

            ret = write_packet(oc, &c_ctx->time_base, st, &pkt);
            if (ret < 0) {
                fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
                exit(1);
            }
            av_free_packet(&pkt);
        }

        /* free allocated image */
        av_freep(&out_frame->data[0]);
        /* free temporary yuv frame */
        av_frame_free(&out_frame);
        /* revert back to original frame data */
        revert_actualize(ta, frame_data);
    }
    n_pts = i; /* set new frame count */
    Frame_destroy(frame_data);
    return n_pts;
}

/*
 * flush_video writes all the delayed frames to the output video file
 */
void flush_video(AVFormatContext *oc, AVStream *st) {
    AVCodecContext *c_ctx = st->codec;
    AVPacket pkt;
    int i, ret, got_output;

    i = 0;
    printf("Flush it yeah\n");
    for (got_output = 1; got_output; i++) {
        av_init_packet(&pkt);
        pkt.data = NULL;    /* packet data will be allocated by the encoder */
        pkt.size = 0;
        fflush(stdout); /* TODO: why is this needed? */

        ret = avcodec_encode_video2(c_ctx, &pkt, NULL, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }

        if (got_output) {

            #ifdef DEBUG_WRITE
            printf("Write frame %3d (size=%5d)\n", i, pkt.size);
            #endif

            ret = write_packet(oc, &c_ctx->time_base, st, &pkt);
            if (ret < 0) {
                fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
                exit(1);
            }
            av_free_packet(&pkt);
        }
    }
}

/*
 * write_end_code writes an MPEG encode to the file stream
 * NOTE: this is not needed when working with
 * AVFormatContext instead of manually writing to a file stream
 */
void write_end_code(FILE *f) {
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    /* add sequence end code to have a real mpeg file */
    fwrite(endcode, 1, sizeof(endcode), f);
}

