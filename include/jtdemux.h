#ifndef JTDEMUX_H
#define JTDEMUX_H

#include "ffmpeginclude.h"
#include <memory>

class JTDemux
{
public:
    JTDemux();
    ~JTDemux();
    void demux(std::shared_ptr<void> param);

public:
    PacketQueue m_audioPacketQueue;  // 音频包队列
    PacketQueue m_videoPacketQueue;  // 视频包队列
    const int m_maxPacketQueueSize;  // 包队列最大长度
    bool m_exit;                     // 退出解复用
    char m_errorBuffer[1024];        // 错误信息
    AVFormatContext* m_fmtCtx;       // 输入上下文
    int m_videoIndex;                // 视频流对应序号
    int m_audioIndex;                // 视频流对应序号

public:
    void demuxInit();
    void exit();
    int getPacketQueueSize(PacketQueue* queue);
    bool getPacket(PacketQueue* queue, AVPacket* pkt, PktDecoder* decoder);
    void pushPacket(PacketQueue* queue, AVPacket* pkt);
    void packetQueueFlush(PacketQueue* queue);
};

#endif // JTDEMUX_H
