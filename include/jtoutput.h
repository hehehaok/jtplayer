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
    void videoCallBack(std::shared_ptr<void> par);
    void displayImage(AVFrame* frame);
    void initAVClock();
    double vpDuration(MyFrame* curFrame, MyFrame* lastFrame);
    double computeTargetDelay(double delay);

    bool initAudio();

signals:
    void frameChanged(std::shared_ptr<YUV420Frame> frame);

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
    uint8_t *m_buffer;                   // 目标图像缓冲区
    uint8_t *m_pixels[4];                // 每种像素分量对应缓冲区入口指针
    int m_pitch[4];                      // 每种像素分量对应缓冲区大小

    // 相关模块
    std::shared_ptr<JTDecoder> m_jtdecoder;  // 解码

    // 时间相关
    bool m_clockInitFlag;                // 时钟是否初始化
    AVClock m_videoClock;                // 视频流对应时钟
    AVClock m_audioClock;                // 音频流对应时钟
    double m_frameTimer;                    // 记录音视频最新的播放帧的时间戳，用于时间同步

    // 操作相关
    bool m_exit;                         // 退出
    bool m_pause;                        // 暂停

};

#endif // JTOUTPUT_H
