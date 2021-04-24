//
// Created by ly on 2021/4/24.
//
#include "audio_swr.h"

/**
 * 使用以下命令播放文件
 * ffplay -ar 44100 -ac 2 -f f32le audio2.pcm
 */
void audio_swr() {
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
    FILE *outfile = fopen("/home/ly/codes/ffstudio/audio2.pcm", "wb+");

    SwrContext *swr_ctx = NULL;
    swr_ctx = swr_alloc_set_opts(NULL,
                       AV_CH_LAYOUT_STEREO,   // 双声道，左右扬声器
                       AV_SAMPLE_FMT_FLT,     // 重采样后采样大小为32位
                       44100,                 // 采样率为44100Hz
                       AV_CH_LAYOUT_STEREO,   // 双声道，左右扬声器
                       AV_SAMPLE_FMT_S16,     // 重采样前采样大小为16位
                       44100,                 // 采样率为44100Hz
                       0,
                       NULL);
    if (!swr_ctx) {
        av_log(NULL, AV_LOG_ERROR, "swr context is null\n");
        return;
    }
    if (swr_init(swr_ctx) < 0) {
        av_log(NULL, AV_LOG_ERROR, "init swr context failed\n");
        return;
    }

    // 创建输入缓冲区
    uint8_t **src_data = NULL;
    int src_linesize = 0;
    /**
     * 从设备中读取的每个avpacket大小为16384字节（实测结果）
     * 每个包包含的采样次数 = 16384 / 2 (通道数) / 2 (采样大小为16位，也就是2字节) = 4096
     */
    av_samples_alloc_array_and_samples(&src_data,
                                       &src_linesize,
                                       2,
                                       4096,
                                       AV_SAMPLE_FMT_S16,   // 原始数据的采样大小为16位
                                       0);
    // 创建输出缓冲区
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;
    /**
     * 从设备中读取的每个avpacket大小为16384字节（实测结果）
     * 每个包包含的采样次数 = 16384 / 2 (通道数) / 2 (采样大小为16位，也就是2字节) = 4096
     */
    av_samples_alloc_array_and_samples(&dst_data,
                                       &dst_linesize,
                                       2,
                                       4096,
                                       AV_SAMPLE_FMT_FLT,  // 重采样后的采样大小为32位
                                       0);
    AVPacket pkt;
    av_init_packet(&pkt);
    int count = 0;
    while ((ret = av_read_frame(fmt_ctx, &pkt)) == 0 && count++ < 500) {
        av_log(NULL, AV_LOG_INFO,"read frame size[%d],data[%p],count[%d]\n",
               pkt.size, pkt.data, count);
        // 把读到的包的数据拷贝到输入缓冲区
        memcpy((void *)src_data[0], (void *)pkt.data, pkt.size);
        // 进行音频重采样
        swr_convert(swr_ctx,
                    dst_data,
                    4096,
                    (const uint8_t **)src_data,
                    4096);
        fwrite(dst_data[0], dst_linesize, 1, outfile);
        av_packet_unref(&pkt);
    }
    avformat_close_input(&fmt_ctx);
    fclose(outfile);
    if (src_data) {
        av_freep(&src_data[0]);
    }
    av_freep(&src_data);
    if (dst_data) {
        av_freep(&dst_data[0]);
    }
    av_freep(&dst_data);
    swr_free(&swr_ctx);



    av_log(NULL, AV_LOG_DEBUG, "hello world");
}