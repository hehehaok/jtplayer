#ifndef JTDEMUX_H
#define JTDEMUX_H

#include "ffmpeginclude.h"
#include <memory>

class JTDemux
{
public:
    JTDemux();
    ~JTDemux();
    void demux(std::shared_ptr<void> param);  // 解复用线程工作函数

public:
    bool m_exit;                     // 退出解复用
    bool m_pause;                    // 暂停解复用
    bool m_step;                     // 逐帧
    bool m_seek;                     // 跳转播放
    double m_seekTarget;             // 跳转位置
    char m_errorBuffer[1024];        // 错误信息
    AVFormatContext* m_avFmtCtx;     // 输入上下文
    const int m_maxPacketQueueSize;  // 包队列最大长度
    const int m_sleepTime;           // 工作线程中的线程睡眠时间

    // 视频相关
    PacketQueue m_audioPacketQueue;  // 音频包队列
    int m_videoStreamIndex;          // 视频流对应序号

    // 音频相关
    PacketQueue m_videoPacketQueue;  // 视频包队列
    int m_audioStreamIndex;          // 视频流对应序号

public:
    void demuxInit();
    void exit();
    int getPacketQueueSize(PacketQueue* queue);
    bool getPacket(PacketQueue* queue, AVPacket* pkt, PktDecoder* decoder);
    void pushPacket(PacketQueue* queue, AVPacket* pkt);
    void packetQueueFlush(PacketQueue* queue);
};

#endif // JTDEMUX_H
