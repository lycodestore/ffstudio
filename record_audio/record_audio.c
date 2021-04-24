//
// Created by ly on 2021/4/24.
//
#include "record_audio.h"

// record audio from micro phone in linux system
/**
 * use this common line to play the pcm file
 * ffplay -ar 44100 -ac 2 -f f32le audio.pcm
 */
void record_audio() {
    av_log_set_level(AV_LOG_DEBUG);
    // 1. register all devices
    avdevice_register_all();
    // 2. get format
    AVInputFormat *iformat = av_find_input_format("alsa");
    // 3. open device
    int ret = 0;
    char errors[1024];
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    char *devicename = "hw:0";
    ret = avformat_open_input(&fmt_ctx, devicename, iformat, &options);
    if (ret < 0) {
        av_strerror(ret, errors, 1024);
        av_log(NULL, AV_LOG_ERROR, "failed to open audio device,[%d],[%s]", ret, errors);
        return;
    }
    // 4. read data from device and write to a file
    FILE *outfile = fopen("/home/ly/codes/ffstudio/audio.pcm", "wb+");

    AVPacket pkt;
    av_init_packet(&pkt);
    int count = 0;
    while ((ret = av_read_frame(fmt_ctx, &pkt)) == 0 && count++ < 500) {
        av_log(NULL, AV_LOG_INFO,"read frame size[%d],data[%p],count[%d]\n",
               pkt.size, pkt.data, count);
        fwrite(pkt.data, pkt.size, 1, outfile);
        av_packet_unref(&pkt);
    }
    avformat_close_input(&fmt_ctx);
    fclose(outfile);

    av_log(NULL, AV_LOG_DEBUG, "hello world");
}