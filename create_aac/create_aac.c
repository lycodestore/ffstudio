//
// Created by ly on 2021/4/24.
//
#include "create_aac.h"


static AVCodecContext* open_encoder() {
    AVCodec *codec = avcodec_find_encoder_by_name("libfdk_aac");
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
    codec_ctx->sample_rate = 44100;
    codec_ctx->channels = 2;
    codec_ctx->bit_rate = 0;
    codec_ctx->profile = FF_PROFILE_AAC_HE_V2;

    int ret = avcodec_open2(codec_ctx, codec, NULL);

    av_log(NULL, AV_LOG_INFO, "frame size is %d\n", codec_ctx->frame_size);

    if (ret < 0) {
        return NULL;
    }

    return codec_ctx;
}

static void encoder(AVCodecContext *ctx,
                    AVFrame *frame,
                    AVPacket *pkt,
                    FILE *output) {
    int ret = 0;
    /**
     * [libfdk_aac @ 0x55f8a6b5b6c0] more samples than frame size
     * 由于frame的nb_samples也就是每一个音频帧包含的采样个数高于ctx->frame_size导致无法编码成功
     * 后续考虑使用audio fifo解决，每次读到的音频包传入fifo中，fifo中每凑足一个frame_size就送入编码器编码
     */

    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send frame failed %d \n", ret);
        return;
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return;
            } else {
                av_log(NULL, AV_LOG_ERROR, "error, encoding audio frame \n");
                exit(-1);
            }
        }
        fwrite(pkt->data, 1, pkt->size, output);
        fflush(output);
    }
}
/**
 * 使用以下命令播放文件
 * ffplay -ar 44100 -ac 2 -f f32le audio2.pcm
 */
void create_aac() {
    av_log_set_level(AV_LOG_DEBUG);
    // 1. 注册设备
    avdevice_register_all();
    // 2. 设置采集方式，从ubuntu麦克风采集
    AVInputFormat *iformat = av_find_input_format("alsa");
    // 3. 打开设备
    int ret = 0;
    char errors[1024];
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    char *devicename = "hw:0";    // 设备名 麦克风
    ret = avformat_open_input(&fmt_ctx, devicename, iformat, &options);
    if (ret < 0) {
        av_strerror(ret, errors, 1024);
        av_log(NULL, AV_LOG_ERROR, "failed to open audio device,[%d],[%s]", ret, errors);
        return;
    }
    // 4. 从设备中循环读取数据包，对采集的原始数据包进行音频重采样，然后写入文件中
    FILE *outfile = fopen("/home/ly/videos/audio.aac", "wb+");

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "alloc frame failed\n");
        return;
    }
    frame->nb_samples = 4096;
    frame->format = AV_SAMPLE_FMT_S16;
    frame->channel_layout = AV_CH_LAYOUT_STEREO;
    av_frame_get_buffer(frame, 0);
    if (!frame->data[0]) {
        av_log(NULL, AV_LOG_ERROR, "get frame buffer failed \n");
        return;
    }

    AVPacket *newpkt = av_packet_alloc();
    if (!newpkt) {
        av_log(NULL, AV_LOG_ERROR, "new pkt failed \n");
        return;
    }

    AVCodecContext *codec_ctx = open_encoder();

    AVPacket pkt;
    av_init_packet(&pkt);
    int count = 0;
    while ((ret = av_read_frame(fmt_ctx, &pkt)) == 0 && count++ < 2) {
        av_log(NULL, AV_LOG_INFO,"read frame size[%d],data[%p],count[%d]\n",
               pkt.size, pkt.data, count);
        memcpy((void *)frame->data[0], (void *)pkt.data, pkt.size);
        encoder(codec_ctx, frame, newpkt, outfile);
        av_packet_unref(&pkt);
    }
    encoder(codec_ctx, NULL, newpkt, outfile);
    av_frame_free(&frame);
    av_packet_free(&newpkt);
    avformat_close_input(&fmt_ctx);
    fclose(outfile);

    av_log(NULL, AV_LOG_DEBUG, "hello world");
}