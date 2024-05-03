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
    bool m_exit;                         // 退出解码
    bool m_pause;                        // 暂停解码
    char m_errorBuffer[1024];            // 错误信息
    const int m_maxFrameQueueSize;       // 帧队列最大长度
    const int m_sleepTime;               // 线程工作最多睡眠时间

    // 视频相关
    FrameQueue m_videoFrameQueue;        // 视频帧队列
    PktDecoder m_videoPktDecoder;        // 视频解码器
    AVRational m_videoFrameTimeBase;     // 视频帧时间基
    double m_videoFrameDuration;         // 视频帧持续时间

    // 音频相关
    FrameQueue m_audioFrameQueue;        // 音频帧队列
    PktDecoder m_audioPktDecoder;        // 音频解码器
    AVRational m_audioFrameTimeBase;     // 音频帧时间基

    std::shared_ptr<JTDemux> m_jtDemux;  // 解复用模块，需要借助这个模块拿取包数据用于解码

public:
    void decoderInit();
    void audioDecoder(std::shared_ptr<void> param);  // 音频解码线程工作函数
    void videoDecoder(std::shared_ptr<void> param);  // 视频解码线程工作函数
    void exit();


    void pushAudioFrame(AVFrame *frame);     // 写入音频帧
    void pushVideoFrame(AVFrame *frame);     // 写入视频帧
    void frameQueueFlush(FrameQueue *queue); // 清空帧队列
    int getRemainingVideoFrame();            // 查看剩余视频帧
    MyFrame* peekLastVideoFrame();           // 查看上一视频帧
    MyFrame* peekCurVideoFrame();            // 查看当前视频帧
    MyFrame* peekNextVideoFrame();           // 查看下一视频帧
    void setNextVideoFrame();                // 将视频帧队列读索引后移一位
    bool getAudioFrame(AVFrame *frame);      // 读取音频帧
};

#endif // JTDECODER_H
