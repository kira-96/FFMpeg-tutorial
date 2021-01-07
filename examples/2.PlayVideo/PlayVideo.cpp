/**
 * Copyright(c) 2021 kira-96
 * FFMpeg-tutorial is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at :
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON - INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <iostream>
#include <string>
#include <sys\stat.h>

using namespace std;

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavcodec\avcodec.h"
#include "libavformat\avformat.h"
#include "libavutil\imgutils.h"
#include "libswscale\swscale.h"
#include "libsdl\SDL.h"

#ifdef __cplusplus
}
#endif

#pragma comment(lib, "avcodec")
#pragma comment(lib, "avformat")
#pragma comment(lib, "avutil")
#pragma comment(lib, "swscale")
#pragma comment(lib, "SDL2")
#pragma comment(lib, "SDL2main")

#define err2str(errnum) \
    char errstr[AV_ERROR_MAX_STRING_SIZE] = { 0 }; \
    av_strerror(errnum, errstr, AV_ERROR_MAX_STRING_SIZE);

#define PLAY_VIDEO_EVENT 101
#define PLAY_AUDIO_EVENT 102
#define QUIT_VIDEO_EVENT 103
#define QUIT_AUDIO_EVENT 104

/**
 * 判断文件是否存在
 */
bool file_exist(const string& filename);

typedef struct VideoThreadArgs
{
    string name;
    int width;
    int height;
    int delay;
};

int play_audio(void* args);

int play_video(void* args);

int main(int argc, char* argv[])
{
    string filename = "../input.mp4";

    // 判断文件是否存在
    if (!file_exist(filename))
    {
        cout << filename << " not exist." << endl;
        return -1;
    }

    // 1. 注册所有文件格式的编解码器的库（新版已废弃）
    // av_register_all();

    // 2. 打开文件
    AVFormatContext* pFormatContext = avformat_alloc_context();
    int errnum = avformat_open_input(&pFormatContext, filename.c_str(), nullptr, nullptr);

    if (errnum != 0)
    {
        err2str(errnum);
        cout << "avformat_open_input() error! " << errstr << endl;

        return errnum;
    }

    // 3. 检查文件流信息
    errnum = avformat_find_stream_info(pFormatContext, nullptr);

    if (errnum < 0)
    {
        err2str(errnum);
        cout << "avformat_find_stream_info() error! " << errstr << endl;

        // 最后：关闭文件
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    // 输出文件流信息
    av_dump_format(pFormatContext, 0, filename.c_str(), 0);

    // 获取视频/音频流
    auto get_stream_by_type = [pFormatContext](enum AVMediaType type) -> AVStream* {
        for (size_t i = 0; i < pFormatContext->nb_streams; i++)
        {
            if (pFormatContext->streams[i]->codecpar->codec_type == type)
            {
                return pFormatContext->streams[i];
            }
        }

        return nullptr;
    };

    AVStream* video_stream = get_stream_by_type(AVMEDIA_TYPE_VIDEO);

    if (video_stream == nullptr)
    {
        cout << filename.c_str() << " not contains video stream! " << endl;

        avformat_close_input(&pFormatContext);

        return -1;
    }

    // 4. 得到编解码器上下文(AVCodecContext)
    AVCodecContext* pVideoCodecContext = avcodec_alloc_context3(nullptr);

    if (pVideoCodecContext == nullptr)
    {
        cout << "avcodec_alloc_context3() error! " << endl;

        avformat_close_input(&pFormatContext);

        return -1;
    }

    errnum = avcodec_parameters_to_context(pVideoCodecContext, video_stream->codecpar);

    if (errnum < 0)
    {
        err2str(errnum);
        cout << "avcodec_parameters_to_context() error! " << errstr << endl;

        avcodec_free_context(&pVideoCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    // 5. 查找编解码器实例(AVCodec)
    AVCodec* pVideoCodec = avcodec_find_decoder(pVideoCodecContext->codec_id);

    if (pVideoCodec == nullptr)
    {
        cout << "Unsupported Codec! " << endl;

        avcodec_free_context(&pVideoCodecContext);
        avformat_close_input(&pFormatContext);

        return -1;
    }

    // 6. 初始化编解码器上下文(AVCodecContext)
    errnum = avcodec_open2(pVideoCodecContext, pVideoCodec, nullptr);

    if (errnum != 0)
    {
        err2str(errnum);
        cout << "avcodec_open2() error! " << errstr << endl;

        avcodec_free_context(&pVideoCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    // 7. 获取每帧图像大小
    errnum = av_image_get_buffer_size(
        AV_PIX_FMT_YUV420P,
        pVideoCodecContext->width,
        pVideoCodecContext->height, 1);

    if (errnum < 0)
    {
        err2str(errnum);
        cout << "av_image_get_buffer_size() error! " << errstr << endl;

        avcodec_free_context(&pVideoCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    // 分配图像帧所需内存
    uint8_t* frame_buffer = (uint8_t*)av_malloc(errnum);
    AVFrame* pframeYUV = av_frame_alloc();

    errnum = av_image_fill_arrays(
        pframeYUV->data, pframeYUV->linesize, frame_buffer,
        AV_PIX_FMT_YUV420P, pVideoCodecContext->width, pVideoCodecContext->height, 1);

    if (errnum < 0)
    {
        err2str(errnum);
        cout << "av_image_fill_arrays() error! " << errstr << endl;

        av_free(frame_buffer);
        av_frame_free(&pframeYUV);

        avcodec_free_context(&pVideoCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    // 8.
    // 初始化 SDL
    errnum = SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    if (errnum != 0)
    {
        cout << "SDL_Init error! " << SDL_GetError() << endl;

        av_free(frame_buffer);
        av_frame_free(&pframeYUV);

        avcodec_free_context(&pVideoCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    AVFrame* pframe = av_frame_alloc();

    // 9. 读取每一帧图像，刷新显示
    // 分配视频包所需资源
    AVPacket* packet = av_packet_alloc();

    // 转换参数 -> YUV
    SwsContext* ctx = sws_getContext(
        pVideoCodecContext->width,
        pVideoCodecContext->height,
        pVideoCodecContext->pix_fmt,
        pVideoCodecContext->width,
        pVideoCodecContext->height,
        AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);

    float fps = (float)video_stream->avg_frame_rate.num / video_stream->avg_frame_rate.den;
    int delay = (int)(1000 /* ms */ / fps);  // 每帧图像的显示间隔 ms

    SDL_Thread* audio_thread = SDL_CreateThread(play_audio, "audio_thread", NULL);

    VideoThreadArgs video_args{};
    video_args.name = filename;
    video_args.width = pVideoCodecContext->width;
    video_args.height = pVideoCodecContext->height;
    video_args.delay = delay;

    SDL_Thread* video_thread = SDL_CreateThread(play_video, "video_thread", &video_args);

    while (av_read_frame(pFormatContext, packet) >= 0)
    {
        if (packet->stream_index != video_stream->index)
        {
            goto next_packet;
        }

        errnum = avcodec_send_packet(pVideoCodecContext, packet);

        if (errnum < 0)
        {
            err2str(errnum);
            cout << "avcodec_send_packet() error! " << errstr << endl;

            goto next_packet;
        }

        while (avcodec_receive_frame(pVideoCodecContext, pframe) == 0)
        {
            sws_scale(ctx, pframe->data, pframe->linesize, 0, pVideoCodecContext->height, pframeYUV->data, pframeYUV->linesize);

            SDL_UserEvent user_event{};
            user_event.type = SDL_USEREVENT;
            user_event.code = PLAY_VIDEO_EVENT;
            user_event.data1 = pframeYUV;

            SDL_PushEvent((SDL_Event*)&user_event);

            // 延时
            SDL_Delay(delay);
        }

    next_packet:
        av_packet_unref(packet);
    }

    // 正常结束，返回 0
    errnum = 0;

    // 10. 释放资源
    // SDL
    SDL_UserEvent quit{};
    quit.type = SDL_USEREVENT;
    quit.code = QUIT_VIDEO_EVENT;
    SDL_PushEvent((SDL_Event*)&quit);
    int thread_status = 0;
    SDL_WaitThread(video_thread, &thread_status);
    cout << "Video thread exit: " << thread_status << endl;
    quit.code = QUIT_AUDIO_EVENT;
    SDL_PushEvent((SDL_Event*)&quit);
    SDL_WaitThread(audio_thread, &thread_status);
    cout << "Audio thread exit: " << thread_status << endl;
    SDL_Quit();
    // FFmpeg
    sws_freeContext(ctx);
    av_packet_free(&packet);
    av_frame_free(&pframe);
    av_frame_free(&pframeYUV);
    av_free(frame_buffer);
    avcodec_free_context(&pVideoCodecContext);
    avformat_close_input(&pFormatContext);

    return errnum;
}

bool file_exist(const string& filename)
{
    struct stat temp;
    return stat(filename.c_str(), &temp) == 0;
}

int play_audio(void* args)
{
    SDL_Event event{};

    while (true)
    {
        if (SDL_WaitEvent(&event) == 0)
            break;

        if (event.type == SDL_QUIT)
        {
            break;
        }

        if (event.type == SDL_USEREVENT)
        {
            SDL_UserEvent* user_event = (SDL_UserEvent*)&event;

            if (user_event->code == QUIT_AUDIO_EVENT)
            {
                cout << "Quit audio thread." << endl;
                break;
            }
            else if (user_event->code == PLAY_AUDIO_EVENT)
            {
                cout << "play audio." << endl;
            }
            else
            {
            }
        }
    }

    return 0;
}

int play_video(void* args)
{
    VideoThreadArgs* init_args = (VideoThreadArgs*)args;

    int width = init_args->width;
    int height = init_args->height;
    int delay = init_args->delay;

    // 创建 SDL 窗口
    SDL_Window* sdl_window = SDL_CreateWindow(
        init_args->name.c_str(),
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_OPENGL);
    SDL_Renderer* sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);
    SDL_Texture* sdl_texture = SDL_CreateTexture(
        sdl_renderer,
        SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        width, height);
    SDL_Rect sdl_rect{};

    SDL_Event event;

    while (true)
    {
        if (SDL_WaitEvent(&event) == 0)
            break;

        if (event.type == SDL_QUIT)
        {
            break;
        }

        if (event.type == SDL_USEREVENT)
        {
            SDL_UserEvent* user_event = (SDL_UserEvent*)&event;

            if (user_event->code == QUIT_VIDEO_EVENT)
            {
                cout << "Quit video thread." << endl;
                break;
            }
            else if (user_event->code == PLAY_VIDEO_EVENT)
            {
                AVFrame* pframeYUV = (AVFrame*)user_event->data1;

                SDL_UpdateYUVTexture(
                    sdl_texture, &sdl_rect,
                    pframeYUV->data[0], pframeYUV->linesize[0],
                    pframeYUV->data[1], pframeYUV->linesize[1],
                    pframeYUV->data[2], pframeYUV->linesize[2]);

                // SDL_UpdateTexture(sdl_texture, &sdl_rect, pframeYUV->data[0], pframeYUV->linesize[0]);

                // 需要更新显示的区域
                sdl_rect.x = 0;
                sdl_rect.y = 0;
                sdl_rect.w = width;
                sdl_rect.h = height;

                // 刷新显示
                SDL_RenderClear(sdl_renderer);
                SDL_RenderCopy(sdl_renderer, sdl_texture, nullptr, &sdl_rect);
                SDL_RenderPresent(sdl_renderer);
            }
            else
            {
            }
        }
    }

    SDL_DestroyTexture(sdl_texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(sdl_window);

    return 0;
}
