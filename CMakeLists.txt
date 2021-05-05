cmake_minimum_required(VERSION 3.17)
project(ffstudio C)

include_directories(/home/ly/devtools/ffmpeg/include)
link_directories(/home/ly/devtools/ffmpeg/lib)

set(CMAKE_C_STANDARD 99)

add_executable(ffstudio
        main.c
        one/one.c
        record_audio/record_audio.c
        audio_swr/audio_swr.c
        create_aac/create_aac.c
        video_record/video_record.c
        h264_encoder/h264_encoder.c)

target_link_libraries(
        ffstudio
        avcodec
        avdevice
        avfilter
        avformat
        avutil
        swresample
        swscale
)