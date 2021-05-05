//
// Created by ly on 2021/4/24.
//
#include "h264_encoder.h"

#define V_WIDTH 640
#define V_HEIGHT 480

static void open_encoder(int width, int height, AVCodecContext **enc_ctx) {
    AVCodec *codec = NULL;
    int ret = 0;

    codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "find 264 encoder failed \n");
        exit(-1);
    }
    *enc_ctx = avcodec_alloc_context3(codec);
    if (!enc_ctx) {
        av_log(NULL, AV_LOG_ERROR, "alloc codec context failed \n");
        exit(-1);
    }
    // SPS PPS
    (*enc_ctx)->profile = FF_PROFILE_H264_HIGH_444;
    (*enc_ctx)->level = 50;

    // 设置视频的宽度和高度 
    (*enc_ctx)->width = width;
    (*enc_ctx)->height = height;

    // 每个gop包含图片的最大数值
    (*enc_ctx)->gop_size = 250;
    // 最少隔25帧就可以插入一个I帧
    (*enc_ctx)->keyint_min = 25;

    // 使用B帧
    (*enc_ctx)->has_b_frames = 1;
    // 每个gop最大B帧数量
    (*enc_ctx)->max_b_frames = 3;

    // 参考帧数为3，参考帧数越多，编码效果越好，但是越耗时
    (*enc_ctx)->refs = 3;

    // 指定yuv数据格式为YUV420P，libx264只支持这个格式
    (*enc_ctx)->pix_fmt = AV_PIX_FMT_YUV420P;

    /**
     * 码流为600kbps
     * YUV原始码流大小为 640 x 480 x 2 x 25 x 8 = 122880000bps (ubuntu摄像头采集出来的yuv为yuyv422)
     * H264的压缩率约为1/100
     * 所以压缩后的码流约为600kps（其实准确来说应该是 1228kbps，但是我用600kbps，编出来也能看）
     */ 
    (*enc_ctx)->bit_rate = 600000;

    // 帧与帧之间的间隔是1/25秒
    (*enc_ctx)->time_base = (AVRational){1, 25};
    // 帧率是美秒25帧
    (*enc_ctx)->framerate = (AVRational){25, 1};

    ret = avcodec_open2(*enc_ctx, codec, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "open codec failed %s\n", av_err2str(ret));
        exit(-1);
    }
}

static AVFrame* create_frame(int width, int height) {
    int ret;
    AVFrame *frame = NULL;

    frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "failed to alloc frame\n");
        goto __ERROR;
    }

    frame->width = width;
    frame->height = height;

    frame->format = AV_PIX_FMT_YUV420P;

    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "failed to get frame buffer \n");
        goto __ERROR;
    }

    return frame;


__ERROR:
    if (frame) {
        av_frame_free(&frame);
    }
    return NULL;
}

static void yuyv422_to_yuv420p(AVFrame *frame, AVPacket pkt, int width, int height) {
    int i = 0;
    int j = 0;
    int y_i = 0;
    int u_i = 0;
    int v_i = 0;

    for (j = 0; j < height; j++) {
        // yuyv422格式，一个像素占2个字节，所以宽度要乘以2
        for (i = 0; i < width * 2; i+=4) {
            // 把y分量全部提取出来
            frame->data[0][y_i++] = pkt.data[j * width * 2 + i];
            frame->data[0][y_i++] = pkt.data[j * width * 2 + i + 2];
            // u和v分量进行隔行采样
            if (j % 2 == 0) {
                frame->data[1][u_i++] = pkt.data[j * width * 2 + i + 1];
            } else {
                frame->data[2][v_i++] = pkt.data[j * width * 2 + i + 3];
            }
        }
    }
}

static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *newpkt, FILE *outfile) {
    int ret = 0;

    if (frame) {
        av_log(NULL, AV_LOG_INFO, "send frame to encoder, pts=%lld\n", frame->pts);
    } else {
        av_log(NULL, AV_LOG_INFO, "send eof to encoder\n");
    }

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send frame to encoder failed %s", av_err2str(ret));
        exit(-1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, newpkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "failed to encoder %s", av_err2str(ret));
            exit(-1);
        }
        fwrite(newpkt->data, 1, newpkt->size, outfile);
        fflush(outfile);
        av_packet_unref(newpkt);
    }
}

/**
 * 本地文件播放命令
 * ffplay -s 640x480 local.yuv
 * ffplay local.h264
 */
void h264_encoder() {
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    AVInputFormat *iformat = NULL;
    int ret = 0;
    char errors[1024];
    FILE *yuvfile = NULL;
    FILE *h264file = NULL;
    AVCodecContext *enc_ctx = NULL;
    AVPacket pkt;
    AVFrame *frame = NULL;
    AVPacket *newpkt = NULL;
    int count = 0;
    int base = 0;


    av_log_set_level(AV_LOG_DEBUG);
    // 1. 注册设备
    avdevice_register_all();
    // 2. 获取输入格式
    iformat = av_find_input_format("video4linux2");
    // 3. 打开设备
    av_dict_set(&options, "video_size", "640x480", 0);
    av_dict_set(&options, "framerate", "25", 0);
    av_dict_set(&options, "pixel_format", "yuyv422",0); // ubuntu的yuv存储格式为yuyv422
    char *devicename = "/dev/video0";   // 摄像头设备名称
    ret = avformat_open_input(&fmt_ctx, devicename, iformat, &options);
    if (ret < 0) {
        av_strerror(ret, errors, 1024);
        av_log(NULL, AV_LOG_ERROR, "failed to open audio device,[%d],[%s]", ret, errors);
        return;
    }

    // 4. read data from device and write to a file
    yuvfile = fopen("/home/ly/videos/local.yuv", "wb+");
    h264file = fopen("/home/ly/videos/local.h264", "wb+");


    open_encoder(V_WIDTH, V_HEIGHT, &enc_ctx);
    frame = create_frame(V_WIDTH, V_HEIGHT);
    newpkt = av_packet_alloc();
    if (!newpkt) {
        av_log(NULL, AV_LOG_ERROR, "alloc newpkt failed \n");
        goto __ERROR;
    }


    av_init_packet(&pkt);
    while ((ret = av_read_frame(fmt_ctx, &pkt)) == 0 && count++ < 500) {
        av_log(NULL, AV_LOG_INFO,"read frame size[%d],data[%p],count[%d]\n",
               pkt.size, pkt.data, count);

        yuyv422_to_yuv420p(frame, pkt, V_WIDTH, V_HEIGHT);

        fwrite(frame->data[0], 1, 307200, yuvfile);
        fwrite(frame->data[1], 1, 307200/4, yuvfile);
        fwrite(frame->data[2], 1, 307200/4, yuvfile);

        // libx264要求每个frame的pts递增，否则编码出来的视频可能存在花屏等问题
        frame->pts = base++;

        encode(enc_ctx, frame, newpkt, h264file);

        av_packet_unref(&pkt);
    }

    // 最后输入一个空的frame，让编码器把所有缓存的数据吐出来
    encode(enc_ctx, NULL, newpkt, h264file);

    av_log(NULL, AV_LOG_DEBUG, "hello world");

__ERROR:
    if (yuvfile) {
        fclose(yuvfile);
    }
    if (h264file) {
        fclose(h264file);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (newpkt) {
        av_packet_free(&newpkt);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }

    av_log(NULL, AV_LOG_ERROR, "get a error end\n");

    return;

}