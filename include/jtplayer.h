#ifndef JTPLAYER_H
#define JTPLAYER_H

#include <QObject>
#include <QString>
#include <QDebug>
#include "ffmpeginclude.h"

class JTDemux;
class JTDecoder;
class JTOutput;
class YUV420Frame;

enum PlayerState {
    STOPED = 0,
    PLAYING,
    PAUSED
};

// 播放器类（单例）：播放器所有的行为以及对外接口
class JTPlayer : public QObject
{
    Q_OBJECT

public:
    char m_errorBuffer[1024];        // 报错缓存
    AVFormatContext* m_avFmtCtx;     // 输入文件格式上下文

    // 视频相关
    AVCodecContext *m_videoCodecCtx;       // 视频解码上下文
    AVCodec *m_videoCodec;                 // 视频解码器
    int m_videoStreamIndex;                // 视频流索引

    // 音频相关
    AVCodecContext *m_audioCodecCtx;       // 音频解码上下文
    AVCodec *m_audioCodec;                 // 音频解码器
    int m_audioStreamIndex;                // 音频流索引

    // 组成播放器的各个模块
    std::shared_ptr<JTDemux> m_jtDemux;      // 解复用模块
    std::shared_ptr<JTDecoder> m_jtDecoder;  // 解码模块
    std::shared_ptr<JTOutput> m_jtOutput;    // 视频音频输出模块

    // 播放器相关
    PlayerState m_playerState;               // 播放器当前状态-播放/暂停/停止
    bool m_exit;                             // 退出播放
    bool m_pause;                            // 暂停播放
    bool m_step;                             // 逐帧播放
    bool m_end;                              // 播放到文件尾
    int64_t m_duration;                      // 文件总时间/秒
    double m_curTime;                        // 当前播放时间
    float m_speed;                           // 当前播放速度

public:
    static JTPlayer* get();
    bool processInput(const QString url);
    void play();
    void close();
    void pause(bool isPause);
    void step(bool isStep);
    void seekTo(double seekTarget);
    void endPause();
    PlayerState getPlayerState();

private:
    explicit JTPlayer(QObject *parent = nullptr);
    JTPlayer(const JTPlayer& other) = delete;
    JTPlayer& operator=(const JTPlayer& other) = delete;
    bool playerInit();
    bool setInput(const QString url);
    bool setVideo();
    bool setAudio();
    bool setModule();

signals:
    void frameChanged(std::shared_ptr<YUV420Frame> frame);
    void durationChanged(int64_t duration);
    void playerStateChanged(PlayerState playerState);
    void playerAtFileEnd();

public slots:
    void setPlayerState(PlayerState playerState);
    void setVolume(int volume);

};

#endif // JTPLAYER_H
