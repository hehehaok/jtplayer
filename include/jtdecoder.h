#ifndef JTDECODER_H
#define JTDECODER_H

#include "ffmpeginclude.h"
#include <memory>

class JTDemux;
class JTDecoder
{
public:
    JTDecoder();
    ~JTDecoder();

public:
    const int m_maxFrameQueueSize;       // 帧队列最大长度
    std::shared_ptr<JTDemux> m_jtdemux;  // 解复用
    FrameQueue m_audioFrameQueue;        // 音频帧队列
    FrameQueue m_videoFrameQueue;        // 视频帧队列
    PktDecoder m_audioPktDecoder;        // 音频解码器
    PktDecoder m_videoPktDecoder;        // 视频解码器
    bool m_exit;                         // 退出解码
    char m_errorBuffer[1024];            // 错误信息
    AVRational m_audioFrameTimeBase;     // 音频帧时间基
    AVRational m_videoFrameTimeBase;     // 视频帧时间基
    AVRational m_videoFrameRate;         // 视频帧率

public:
    void decoderInit();
    void audioDecoder(std::shared_ptr<void> param);
    void videoDecoder(std::shared_ptr<void> param);
    void exit();


    void pushAFrame(AVFrame *frame);
    void pushVFrame(AVFrame *frame);
    void frameQueueFlush(FrameQueue *queue);
    int getRemainingVFrame();    // 查看剩余视频帧
    MyFrame* peekLastVFrame();   // 查看上一视频帧
    MyFrame* peekCurVFrame();    // 查看当前视频帧
    MyFrame* peekNextVFrame();   // 查看下一视频帧
    void setNextVFrame();        // 将读索引后移一位
};

#endif // JTDECODER_H
