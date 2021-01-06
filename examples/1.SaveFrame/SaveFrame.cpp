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
#include <fstream>
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

#ifdef __cplusplus
}
#endif

#pragma comment(lib, "avcodec")
#pragma comment(lib, "avformat")
#pragma comment(lib, "avutil")
#pragma comment(lib, "swscale")

#define err2str(errnum) \
    char errstr[AV_ERROR_MAX_STRING_SIZE] = { 0 }; \
    av_strerror(errnum, errstr, AV_ERROR_MAX_STRING_SIZE);

/**
 * 判断文件是否存在
 */
bool file_exist(const string& filename);

/**
 * 保存视频帧数据
 */
int save_frame(AVFrame* pframe, int width, int height, const char* filename);

int main()
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

    // 获取视频流
    auto get_video_stream = [pFormatContext]() -> AVStream* {
        for (size_t i = 0; i < pFormatContext->nb_streams; i++)
        {
            if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                return pFormatContext->streams[i];
            }
        }

        return nullptr;
    };

    AVStream* video_stream = get_video_stream();

    if (video_stream == nullptr)
    {
        cout << filename.c_str() << " not contains video stream! " << endl;

        avformat_close_input(&pFormatContext);

        return -1;
    }

    // 4. 得到编解码器上下文(AVCodecContext)
    AVCodecContext* pCodecContext = avcodec_alloc_context3(nullptr);

    if (pCodecContext == nullptr)
    {
        cout << "avcodec_alloc_context3() error! " << endl;

        avformat_close_input(&pFormatContext);

        return -1;
    }

    errnum = avcodec_parameters_to_context(pCodecContext, video_stream->codecpar);

    if (errnum < 0)
    {
        err2str(errnum);
        cout << "avcodec_parameters_to_context() error! " << errstr << endl;

        avcodec_free_context(&pCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    // 5. 查找编解码器实例(AVCodec)
    AVCodec* pCodec = avcodec_find_decoder(pCodecContext->codec_id);

    if (pCodec == nullptr)
    {
        cout << "Unsupported Codec! " << endl;

        avcodec_free_context(&pCodecContext);
        avformat_close_input(&pFormatContext);

        return -1;
    }

    // 6. 初始化编解码器上下文(AVCodecContext)
    errnum = avcodec_open2(pCodecContext, pCodec, nullptr);

    if (errnum != 0)
    {
        err2str(errnum);
        cout << "avcodec_open2() error! " << errstr << endl;

        avcodec_free_context(&pCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    // 7. 获取每帧图像大小 width × height × 3(RGB)
    errnum = av_image_get_buffer_size(
        AV_PIX_FMT_RGB24,
        pCodecContext->width,
        pCodecContext->height, 1);

    if (errnum < 0)
    {
        err2str(errnum);
        cout << "av_image_get_buffer_size() error! " << errstr << endl;

        avcodec_free_context(&pCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    // 分配图像帧所需内存
    uint8_t* frame_buffer = (uint8_t*)av_malloc(errnum);
    AVFrame* pframeRGB = av_frame_alloc();

    errnum = av_image_fill_arrays(
        pframeRGB->data, pframeRGB->linesize, frame_buffer,
        AV_PIX_FMT_RGB24, pCodecContext->width, pCodecContext->height, 1);

    if (errnum < 0)
    {
        err2str(errnum);
        cout << "av_image_fill_arrays() error! " << errstr << endl;

        av_free(frame_buffer);
        av_frame_free(&pframeRGB);

        avcodec_free_context(&pCodecContext);
        avformat_close_input(&pFormatContext);

        return errnum;
    }

    AVFrame* pframe = av_frame_alloc();

    // 分配视频包所需资源
    AVPacket* packet = av_packet_alloc();

    // 转换参数 -> RGB
    SwsContext* ctx = sws_getContext(
        pCodecContext->width,
        pCodecContext->height,
        pCodecContext->pix_fmt,
        pCodecContext->width,
        pCodecContext->height,
        AV_PIX_FMT_RGB24, SWS_BICUBIC, nullptr, nullptr, nullptr);

    // 8. 读取每一帧图像，保存图片
    while (av_read_frame(pFormatContext, packet) >= 0)
    {
        if (packet->stream_index != video_stream->index)
        {
            goto next_packet;
        }

        errnum = avcodec_send_packet(pCodecContext, packet);

        if (errnum < 0)
        {
            err2str(errnum);
            cout << "avcodec_send_packet() error! " << errstr << endl;

            goto next_packet;
        }

        while (avcodec_receive_frame(pCodecContext, pframe) == 0)
        {
            // 每 100 帧保存一次
            if (pframe->coded_picture_number % 100 == 0)
            {
                cout << "Save frame: " << pframe->coded_picture_number << endl;
                // 转换为 RGB 格式
                sws_scale(ctx, pframe->data, pframe->linesize, 0, pCodecContext->height, pframeRGB->data, pframeRGB->linesize);
                save_frame(pframeRGB, pCodecContext->width, pCodecContext->height, (to_string(pframe->coded_picture_number) + ".ppm").c_str());
            }
        }

        next_packet:
        av_packet_unref(packet);
    }

    // 正常结束，返回 0
    errnum = 0;

    // 9. 释放资源
    sws_freeContext(ctx);
    av_packet_free(&packet);
    av_frame_free(&pframe);
    av_frame_free(&pframeRGB);
    av_free(frame_buffer);

    // 释放上下文资源
    avcodec_free_context(&pCodecContext);
    // 最后：关闭文件
    avformat_close_input(&pFormatContext);

    return errnum;
}

bool file_exist(const string& filename)
{
    struct stat temp;
    return stat(filename.c_str(), &temp) == 0;
}

int save_frame(AVFrame* pframe, int width, int height, const char* filename)
{
    ofstream fs(filename, ios::out | ios::binary);

    if (!fs.is_open())
    {
        return -1;
    }

    fs << "P6\n" << to_string(width) << " " << to_string(height) << "\n" << "255\n";

    for (size_t i = 0; i < height; i++)
    {
        fs.write((char*)(pframe->data[0] + i * pframe->linesize[0]), static_cast<std::streamsize>(width) * 3);
    }

    fs.close();

    return 0;
}
