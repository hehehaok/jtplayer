#ifndef JTOUTPUT_H
#define JTOUTPUT_H

#include "ffmpeginclude.h"
#include "avclock.h"

class JTDecoder;
class YUV420Frame;

class JTOutput : public QObject
{
    Q_OBJECT
public:
    explicit JTOutput(QObject *parent = nullptr);
    ~JTOutput();
    bool outputInit();
    void exit();

    bool initVideo();
    void videoCallBack(std::shared_ptr<void> param);
    void displayImage(AVFrame* frame);
    void initAVClock();
    double vpDuration(MyFrame* curFrame, MyFrame* lastFrame);
    double computeTargetDelay(double delay);

    bool initAudio();
    static void audioCallBack(void *userData, uint8_t *stream, int len);

signals:
    void frameChanged(std::shared_ptr<YUV420Frame> frame);
    void ptsChanged(unsigned int pts);

public:
    // 视频相关
    int m_videoIndex;                    // 视频流对应序号
    AVRational m_videoFrameTimeBase;     // 视频帧时间基
    AVCodecParameters *m_videoCodecPar;  // 视频编码参数
    SwsContext *m_swsCtx;                // 图像格式转换上下文
    int m_dstPixWidth;                   // 目标图像宽度
    int m_dstPixHeight;                  // 目标图像高度
    enum AVPixelFormat m_dstPixFmt;      // 目标图像格式
    int m_swsFlags;                      // 图像格式转换算法
    uint8_t *m_videoBuffer;              // 目标图像缓冲区
    uint8_t *m_pixels[4];                // 每种像素分量对应缓冲区入口指针
    int m_pitch[4];                      // 每种像素分量对应缓冲区大小

    // 音频相关
    int m_audioIndex;                    // 音频流对应序号
    AVRational m_audioFrameTimeBase;     // 音频帧时间基
    AVCodecParameters* m_audioCodecPar;  // 音频编码参数
    SwrContext *m_swrCtx;                // 音频重采样上下文
    uint8_t *m_audioBuffer;              // 音频缓冲区
    uint32_t m_audioBufferSize;          // 音频缓冲区大小/字节
    uint32_t m_audioBufferIndex;         // 音频缓存读索引/字节
    int64_t m_lastAudioPts;              // 上一帧的音频pts，主要是用于更新当前的视频时间
    enum AVSampleFormat m_dstSampleFmt;  // 目标音频格式
    int m_dstChannels;                   // 目标通道数
    int m_dstFreq;                       // 目标采样率
    int m_dstChannelLayout;              // 目标通道布局

    AVFrame* m_audioFrame;               // 音频帧实例，用于接收来自帧队列的帧



    // 相关模块
    std::shared_ptr<JTDecoder> m_jtDecoder;  // 解码模块

    // 时间相关
    bool m_clockInitFlag;                // 时钟是否初始化
    AVClock m_videoClock;                // 视频流对应时钟
    AVClock m_audioClock;                // 音频流对应时钟
    double m_frameTimer;                 // 当前帧刚开始播放时的时间戳

    // 操作相关
    bool m_exit;                         // 退出
    bool m_pause;                        // 暂停
    bool m_step;                         // 逐帧
    float m_speed;                       // 播放速度
    int m_volume;                        // 音量


    // 其他
    char m_errorBuffer[1024];            // 错误信息
    const int m_sleepTime;               // 工作线程最多睡眠时间

};

#endif // JTOUTPUT_H
