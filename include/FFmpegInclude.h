#ifndef FFMPEGINCLUDE_H
#define FFMPEGINCLUDE_H

#ifndef INT64_C
#define INT64_C
#define UINT64_C
#endif
// 引入ffmpeg头文件
extern "C"
{
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/frame.h"
#include "libavutil/display.h"
#include "libavutil/pixdesc.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "libavutil/ffversion.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavdevice/avdevice.h"
#if (LIBAVCODEC_VERSION_MAJOR > 56)
#include "libavutil/hwcontext.h"
#endif
}

#ifdef Q_CC_MSVC
#pragma execution_character_set("utf-8")
#endif

#ifndef TIMEMS
#define TIMES qPrintable(QTime::currentTime().toString("HH:mm:ss zzz"))
#endif

// 通过avcode版本定义对应主版本
#if (LIBAVCODEC_VERSION_MAJOR == 56)
#define FFMPEG_VERSION_MAJOR 2
#elif (LIBAVCODEC_VERSION_MAJOR == 57)
#define FFMPEG_VERSION_MAJOR 3
#elif (LIBAVCODEC_VERSION_MAJOR == 58)
#define FFMPEG_VERSION_MAJOR 4
#elif (LIBAVCODEC_VERSION_MAJOR == 59)
#define FFMPEG_VERSION_MAJOR 5
#elif (LIBAVCODEC_VERSION_MAJOR == 60)
#define FFMPEG_VERSION_MAJOR 6
#endif

#if (FFMPEG_VERSION_MAJOR > 4)
#define AVCodecx const AVCodec
#define AVInputFormatx const AVInputFormat
#define AVOutputFormatx const AVOutputFormat
#else
#define AVCodecx AVCodec
#define AVInputFormatx AVInputFormat
#define AVOutputFormatx AVOutputFormat
#endif

#if (FFMPEG_VERSION_MAJOR < 3)
enum AVHWDeviceType
{
    AV_HWDEVICE_TYPE_VDPAU,
    AV_HWDEVICE_TYPE_CUDA,
    AV_HWDEVICE_TYPE_VAAPI,
    AV_HWDEVICE_TYPE_DXVA2,
    AV_HWDEVICE_TYPE_QSV,
    AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
    AV_HWDEVICE_TYPE_NONE,
    AV_HWDEVICE_TYPE_D3D11VA,
    AV_HWDEVICE_TYPE_DRM,
};
#endif

#include <vector>
#include <mutex>
#include <condition_variable>
#include <QDebug>

// 常用数据结构

struct MyPacket {
    AVPacket packet;              // 解复用后的包
    int serial;                   // 包对应的序列号
};

struct PacketQueue {
    std::vector<MyPacket> pktQue; // 包队列
    int readIndex;                // 读索引
    int pushIndex;                // 写索引
    int size;                     // 包队列中包的数量
    int serial;                   // 包队列对应序列号，当包清空后序列号会随之加1，用于辨识当前包队列版本
    std::mutex mutex;             // 互斥锁
    std::condition_variable cond; // 条件变量
};

struct MyFrame {
    AVFrame frame;       // 对应帧
    int serial;          // 帧对应序列号
    double duration;     // 帧时长？
    double pts;          // 帧pts
};

struct FrameQueue {
    std::vector<MyFrame> frameQue; // 帧队列
    int readIndex;                 // 读索引
    int pushIndex;                 // 写索引
    int shown;                     // 显示计数？
    int size;                      // 帧队列中帧的数量
    std::mutex mutex;              // 互斥锁
    std::condition_variable cond;  // 条件变量
};

struct PktDecoder {
    AVCodecContext* codecCtx;     // 解码器上下文
    int serial;                   // 解码器对应序列号
};


#endif // FFMPEGINCLUDE_H
