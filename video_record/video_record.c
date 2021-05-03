//
// Created by ly on 2021/4/24.
//
#include "video_record.h"

/**
 * 播放命令
 * ffplay -s 640x480 -pix_fmt yuyv422 local.yuv
 */
void record_video() {
    av_log_set_level(AV_LOG_DEBUG);
    // 1. 注册设备
    avdevice_register_all();
    // 2. 获取输入格式
    AVInputFormat *iformat = av_find_input_format("video4linux2");
    // 3. 打开设备
    int ret = 0;
    char errors[1024];
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    av_dict_set(&options, "video_size", "640x480", 0);
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "pixel_format", "yuyv422",0); // ubuntu的yuv存储格式为yuyv422
    char *devicename = "/dev/video0";   // 摄像头设备名称
    ret = avformat_open_input(&fmt_ctx, devicename, iformat, &options);
    if (ret < 0) {
        av_strerror(ret, errors, 1024);
        av_log(NULL, AV_LOG_ERROR, "failed to open audio device,[%d],[%s]", ret, errors);
        return;
    }
    // 4. read data from device and write to a file
    FILE *outfile = fopen("/home/ly/videos/local.yuv", "wb+");

    AVPacket pkt;
    av_init_packet(&pkt);
    int count = 0;
    while ((ret = av_read_frame(fmt_ctx, &pkt)) == 0 && count++ < 500) {
        av_log(NULL, AV_LOG_INFO,"read frame size[%d],data[%p],count[%d]\n",
               pkt.size, pkt.data, count);
        // 注意这里，写入的大小应该等于分辨率乘以2（yuyv422对应的大小为2），这里刚好跟pkt.size相等而已，但是他们并不一定总是相等
        fwrite(pkt.data, pkt.size, 1, outfile);
        av_packet_unref(&pkt);
    }
    avformat_close_input(&fmt_ctx);
    fclose(outfile);

    av_log(NULL, AV_LOG_DEBUG, "hello world");
}