#include "jtplayer.h"
#include "jtdemux.h"
#include "jtdecoder.h"
#include "jtoutput.h"
#include "threadpool.h"
#include "SDL2/SDL.h"

JTPlayer::JTPlayer(QObject *parent) : QObject(parent),
    m_jtDemux(std::make_shared<JTDemux>()),
    m_jtDecoder(std::make_shared<JTDecoder>()),
    m_jtOutput(std::make_shared<JTOutput>()),
    m_playerState(PlayerState::STOPED),
    m_exit(false), m_pause(false), m_step(false), m_end(false),
    m_duration(0), m_curTime(0.00), m_speed(0.00)
{

}

JTPlayer* JTPlayer::get()
{
    static JTPlayer jtPlayer;
    return &jtPlayer;
}

bool JTPlayer::processInput(const QString url)
{
    // 初始化成员变量
    if (!playerInit())
    {
        return false;
    }

    // 初始化输入
    if (!setInput(url))
    {
        return false;
    }

    // 初始化视频
    if (!setVideo())
    {
        return false;
    }

    // 初始化音频
    if (!setAudio())
    {
        return false;
    }

    // 初始化模块
    if (!setModule())
    {
        return false;
    }
    return true;
}

void JTPlayer::play()
{
    ThreadPool::addTask(std::bind(&JTDemux::demux, m_jtDemux, std::placeholders::_1), std::make_shared<int>(1));
    ThreadPool::addTask(std::bind(&JTDecoder::audioDecoder, m_jtDecoder, std::placeholders::_1), std::make_shared<int>(2));
    ThreadPool::addTask(std::bind(&JTDecoder::videoDecoder, m_jtDecoder, std::placeholders::_1), std::make_shared<int>(3));
    ThreadPool::addTask(std::bind(&JTOutput::videoCallBack, m_jtOutput, std::placeholders::_1), std::make_shared<int>(4));
    SDL_PauseAudio(0);
    emit playerStateChanged(PlayerState::PLAYING);
}

bool JTPlayer::playerInit()
{
    m_exit = false;
    m_pause = false;
    m_step = false;
    m_end = false;

    m_errorBuffer[1023] = '\0';
    m_avFmtCtx = NULL;          // 输入文件格式上下文

    // 视频相关
    m_videoCodecCtx = NULL;           // 视频解码上下文
    m_videoCodec = NULL;              // 视频解码器
    m_videoStreamIndex = -1;          // 视频流索引

    // 音频相关
    m_audioCodecCtx = NULL;           // 音频解码上下文
    m_audioCodec = NULL;              // 视频解码器
    m_audioStreamIndex = -1;          // 视频流索引

    return true;
}

bool JTPlayer::setInput(const QString url)
{
    // 打开视频文件
    int res = avformat_open_input(&m_avFmtCtx, url.toStdString().c_str(), NULL, NULL);
    if (res < 0) {
        av_strerror(res, m_errorBuffer, sizeof(m_errorBuffer));
        qDebug() << "open " << url << " failed:" << m_errorBuffer << "\n";
        return false;
    }

    // 获取流信息
    res = avformat_find_stream_info(m_avFmtCtx, NULL);
    if (res < 0) {
        av_strerror(res, m_errorBuffer, sizeof(m_errorBuffer));
        qDebug() << "find steam info failed:" << m_errorBuffer << "\n";
        return false;
    }

    m_duration = m_avFmtCtx->duration / AV_TIME_BASE;
    emit durationChanged(m_duration);

    qDebug() << "open " << url << " success!\nTotalSec : "
             << m_duration << "\nStreamNums : "
             << m_avFmtCtx->nb_streams << "\nStartTime : "
             << m_avFmtCtx->start_time << "\nBitRate : "
             << m_avFmtCtx->bit_rate << "\n";
    return true;
}

bool JTPlayer::setVideo()
{
    // 寻找解码器
    for (size_t i = 0; i < m_avFmtCtx->nb_streams; i++) {
        AVCodecParameters* avcodecpar = m_avFmtCtx->streams[i]->codecpar;
        if (avcodecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            // 记录视频流对应序号
            m_videoStreamIndex = i;

            // 找到对应解码器
            m_videoCodec = avcodec_find_decoder(avcodecpar->codec_id);
            if (!m_videoCodec) {
                qDebug() << "video codec not found!\n";
                return false;
            }

            // 构建解码器上下文
            m_videoCodecCtx = avcodec_alloc_context3(m_videoCodec);

            // 用解码器参数初始化对应解码器上下文
            int err = avcodec_parameters_to_context(m_videoCodecCtx, avcodecpar);
            if (err < 0) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "video avcodec_parameters_to_context() failed:" << m_errorBuffer << "\n";
                return false;
            }
            // 打开解码器
            err = avcodec_open2(m_videoCodecCtx, m_videoCodec, NULL);
            if (err != 0) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "open video avcodec failed:" << m_errorBuffer << "\n";
                return false;
            }
            break; // 找到视频流后直接跳出循环
        }
    }
    if (m_videoStreamIndex == -1) {
        qDebug() << "not found video stream!\n";
        return false;
    }
    return true;
}

bool JTPlayer::setAudio()
{
    // 寻找解码器
    for (size_t i = 0; i < m_avFmtCtx->nb_streams; i++) {
        AVCodecParameters* avCodecParam = m_avFmtCtx->streams[i]->codecpar;
        if (avCodecParam->codec_type == AVMEDIA_TYPE_AUDIO) {
            // 记录音频流对应序号
            m_audioStreamIndex = i;

            // 找到对应解码器
            m_audioCodec = avcodec_find_decoder(avCodecParam->codec_id);
            if (!m_audioCodec) {
                qDebug() << "audio codec not found!\n";
                return false;
            }

            // 构建解码器上下文
            m_audioCodecCtx = avcodec_alloc_context3(m_audioCodec);

            // 用解码器参数初始化对应解码器上下文
            int err = avcodec_parameters_to_context(m_audioCodecCtx, avCodecParam);
            if (err < 0) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "audio avcodec_parameters_to_context() failed:" << m_errorBuffer << "\n";
                return false;
            }
            // 打开解码器
            err = avcodec_open2(m_audioCodecCtx, m_audioCodec, NULL);
            if (err != 0) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "open audio avcodec failed:" << m_errorBuffer << "\n";
                return false;
            }
            break; // 找到音频流后直接跳出循环
        }
    }
    if (m_audioStreamIndex == -1) {
        qDebug() << "not found audio stream!\n";
        return false;
    }
    return true;
}

bool JTPlayer::setModule()
{
    if (!ThreadPool::init())
    {
        qDebug() << "threadpool init failed!\n";
        return false;
    }
    if (m_jtDemux == nullptr) {
        qDebug() << "demux module init failed!\n";
        return false;
    }
    m_jtDemux->demuxInit();

    if (m_jtDecoder == nullptr) {
        qDebug() << "decoder module init failed!\n";
        return false;
    }
    m_jtDecoder->decoderInit();

    if (m_jtOutput == nullptr) {
        qDebug() << "output module init failed!\n";
        return false;
    }
    m_jtOutput->outputInit();
    return true;
}

void JTPlayer::close()
{
    m_jtOutput->exit();  // 释放输出模块对应资源
    m_jtDecoder->exit(); // 释放解码模块对应资源
    m_jtDemux->exit();   // 释放解复用模块对应资源

    if (m_videoCodecCtx != NULL) {
        avcodec_free_context(&m_videoCodecCtx);
        m_videoCodec = NULL;
    }
    if (m_audioCodecCtx != NULL) {
        avcodec_free_context(&m_audioCodecCtx);
        m_audioCodec = NULL;
    }
    if (m_avFmtCtx != NULL) {
        avformat_close_input(&m_avFmtCtx);
    }

    SDL_CloseAudio(); // 停止音频线程
    ThreadPool::releasePool(); // 停止线程池中的线程
    emit playerStateChanged(PlayerState::STOPED);
    qDebug() << "file close!\n";
}

void JTPlayer::pause(bool isPause)
{
    m_pause = isPause;
    m_jtDemux->m_pause = isPause;
    m_jtDecoder->m_pause = isPause;
    m_jtOutput->m_pause = isPause;
    m_jtOutput->m_audioClock.pauseClock(isPause);
    if (m_pause) {
        emit playerStateChanged(PlayerState::PAUSED);
    }
    else {
        emit playerStateChanged(PlayerState::PLAYING);
    }
}

void JTPlayer::step(bool isStep)
{
    m_step = isStep;
    m_jtOutput->m_audioClock.pauseClock(false);
}

void JTPlayer::seekTo(double seekTarget)
{
    m_jtDemux->m_seekTarget = seekTarget;
    m_jtDemux->m_seek = true;
}

void JTPlayer::endPause()
{
    emit playerAtFileEnd();
}

void JTPlayer::setSpeed(float speed)
{
    if (m_jtOutput->m_speed != speed) {
        m_jtOutput->m_lastSpeed = m_jtOutput->m_speed;
        m_jtOutput->m_speed = speed;
        m_jtOutput->m_speedChanged = true;
    }
}

PlayerState JTPlayer::getPlayerState()
{
    return m_playerState;
}

void JTPlayer::setPlayerState(PlayerState playerState) {
    m_playerState = playerState;
}

void JTPlayer::setVolume(int volume)
{
    m_jtOutput->m_volume = volume;
}
