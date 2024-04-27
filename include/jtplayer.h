#ifndef JTPLAYER_H
#define JTPLAYER_H

#include <QObject>
#include <QString>
#include <QDebug>
#include "ffmpeginclude.h"


class JTDemux;
class JTDecoder;

// 播放器类：所有的播放器行为
class jtPlayer : public QObject
{
    Q_OBJECT

public:
    char errorBuffer[1024];          // 报错缓存
    char videoBuffer[1024];          // 视频信息缓存
    AVFormatContext* avformtctx;     // 输入文件格式上下文

    // 视频相关
    AVCodecContext* Vcodecctx;       // 视频解码上下文
    AVCodec* Vcodec;                 // 视频解码器
    SwsContext* Vswsctx;             // 视频帧格式转换上下文
    int Vstreamindex;                // 视频流索引

    // 音频相关
    AVCodecContext *Acodecctx;       // 音频解码上下文
    AVCodec *Acodec;                 // 视频解码器
    SwrContext *Aswrctx;             // 音频采样转换上下文
    int Astreamindex;                // 视频流索引

    // 解复用和解码模块
    std::shared_ptr<JTDemux> m_jtdemux;
    std::shared_ptr<JTDecoder> m_jtdecoder;

public:
    static jtPlayer* get();
    bool processInput(const QString url);
    void play();
    void close();

private:
    explicit jtPlayer(QObject *parent = nullptr);
    bool playerInit();
    bool setInput(const QString url);
    bool setVideo();
    bool setAudio();
    bool setModule();
signals:

};

#endif // JTPLAYER_H
