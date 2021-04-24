cmake_minimum_required(VERSION 3.17)
project(ffstudio C)

include_directories(/home/ly/devtools/ffmpeg/include)
link_directories(/home/ly/devtools/ffmpeg/lib)

set(CMAKE_C_STANDARD 99)

add_executable(ffstudio main.c one/one.c record_audio/record_audio.c)

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