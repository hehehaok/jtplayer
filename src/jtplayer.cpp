#include "jtplayer.h"
#include "jtdemux.h"
#include "jtdecoder.h"
#include "jtoutput.h"
#include "threadpool.h"

jtPlayer::jtPlayer(QObject *parent) : QObject(parent)
{

}

jtPlayer* jtPlayer::get()
{
    static jtPlayer videoFFmpeg;
    return &videoFFmpeg;
}

bool jtPlayer::processInput(const QString url)
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

void jtPlayer::play()
{
    ThreadPool::addTask(std::bind(&JTDemux::demux, m_jtdemux, std::placeholders::_1), std::make_shared<int>(1));
    ThreadPool::addTask(std::bind(&JTDecoder::audioDecoder, m_jtdecoder, std::placeholders::_1), std::make_shared<int>(2));
    ThreadPool::addTask(std::bind(&JTDecoder::videoDecoder, m_jtdecoder, std::placeholders::_1), std::make_shared<int>(3));
    ThreadPool::addTask(std::bind(&JTOutput::videoCallBack, m_jtoutput, std::placeholders::_1), std::make_shared<int>(4));
}

bool jtPlayer::playerInit()
{
    errorBuffer[1023] = '\0';
    videoBuffer[1023] = '\0';
    avformtctx = NULL;          // 输入文件格式上下文

    // 视频相关
    Vcodecctx = NULL;           // 视频解码上下文
    Vcodec = NULL;              // 视频解码器
    Vswsctx = NULL;             // 视频帧格式转换上下文
    Vstreamindex = -1;          // 视频流索引

    // 音频相关
    Acodecctx = NULL;           // 音频解码上下文
    Acodec = NULL;              // 视频解码器
    Aswrctx = NULL;             // 音频采样转换上下文
    Astreamindex = -1;          // 视频流索引

    avformat_network_init(); // 初始化网络
    return true;
}

bool jtPlayer::setInput(const QString url)
{
    // 打开视频文件
    int res = avformat_open_input(&avformtctx, url.toStdString().c_str(), NULL, NULL);
    if (res < 0) {
        av_strerror(res, errorBuffer, sizeof(errorBuffer));
        qDebug() << "open " << url << " failed:" << errorBuffer << "\n";
        return false;
    }

    // 获取流信息
    res = avformat_find_stream_info(avformtctx, NULL);
    if (res < 0) {
        av_strerror(res, errorBuffer, sizeof(errorBuffer));
        qDebug() << "find steam info failed:" << errorBuffer << "\n";
        return false;
    }
    qDebug() << "open " << url << " success!\nTotalMs : "
             << avformtctx->duration / AV_TIME_BASE << "\nStreamNums : "
             << avformtctx->nb_streams << "\nStartTime : "
             << avformtctx->start_time << "\nBitRate : "
             << avformtctx->bit_rate << "\n";
    return true;
}

bool jtPlayer::setVideo()
{
    // 寻找解码器
    for (size_t i = 0; i < avformtctx->nb_streams; i++) {
        AVCodecParameters* avcodecpar = avformtctx->streams[i]->codecpar;
        if (avcodecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            // 记录视频流对应序号
            Vstreamindex = i;

            // 找到对应解码器
            Vcodec = avcodec_find_decoder(avcodecpar->codec_id);
            if (!Vcodec) {
                qDebug() << "video codec not found!\n";
                return false;
            }

            // 构建解码器上下文
            Vcodecctx = avcodec_alloc_context3(Vcodec);

            // 用解码器参数初始化对应解码器上下文
            int err = avcodec_parameters_to_context(Vcodecctx, avcodecpar);
            if (err < 0) {
                av_strerror(err, errorBuffer, sizeof(errorBuffer));
                qDebug() << "video avcodec_parameters_to_context() failed:" << errorBuffer << "\n";
                return false;
            }
            // 打开解码器
            err = avcodec_open2(Vcodecctx, Vcodec, NULL);
            if (err != 0) {
                av_strerror(err, errorBuffer, sizeof(errorBuffer));
                qDebug() << "open video avcodec failed:" << errorBuffer << "\n";
                return false;
            }
            break; // 找到视频流后直接跳出循环
        }
    }
    if (Vstreamindex == -1) {
        qDebug() << "not found video stream!\n";
        return false;
    }
    return true;
}

bool jtPlayer::setAudio()
{
    // 寻找解码器
    for (size_t i = 0; i < avformtctx->nb_streams; i++) {
        AVCodecParameters* avcodecpar = avformtctx->streams[i]->codecpar;
        if (avcodecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            // 记录音频流对应序号
            Astreamindex = i;

            // 找到对应解码器
            Acodec = avcodec_find_decoder(avcodecpar->codec_id);
            if (!Acodec) {
                qDebug() << "audio codec not found!\n";
                return false;
            }

            // 构建解码器上下文
            Acodecctx = avcodec_alloc_context3(Acodec);

            // 用解码器参数初始化对应解码器上下文
            int err = avcodec_parameters_to_context(Acodecctx, avcodecpar);
            if (err < 0) {
                av_strerror(err, errorBuffer, sizeof(errorBuffer));
                qDebug() << "audio avcodec_parameters_to_context() failed:" << errorBuffer << "\n";
                return false;
            }
            // 打开解码器
            err = avcodec_open2(Acodecctx, Acodec, NULL);
            if (err != 0) {
                av_strerror(err, errorBuffer, sizeof(errorBuffer));
                qDebug() << "open audio avcodec failed:" << errorBuffer << "\n";
                return false;
            }
            break; // 找到音频流后直接跳出循环
        }
    }
    if (Astreamindex == -1) {
        qDebug() << "not found audio stream!\n";
        return false;
    }
    return true;
}

bool jtPlayer::setModule()
{
    if (!ThreadPool::init())
    {
        qDebug() << "threadpool init failed!\n";
        return false;
    }
    m_jtdemux = std::make_shared<JTDemux>();
    if (m_jtdemux == nullptr) {
        qDebug() << "demux module init failed!\n";
        return false;
    }
    m_jtdecoder = std::make_shared<JTDecoder>();
    if (m_jtdecoder == nullptr) {
        qDebug() << "decoder module init failed!\n";
        return false;
    }
    m_jtoutput = std::make_shared<JTOutput>();
    if (m_jtoutput == nullptr) {
        qDebug() << "output module init failed!\n";
        return false;
    }
    return true;
}

void jtPlayer::close()
{
    if (Vswsctx != NULL) {
        sws_freeContext(Vswsctx);
        Vswsctx = NULL;
    }

    if (Aswrctx != NULL) {
        sws_freeContext(Vswsctx);
        Aswrctx = NULL;
    }
    if (Vcodecctx != NULL) {
        avcodec_free_context(&Vcodecctx);
        Vcodecctx = NULL;
        Vcodec = NULL;
    }
    if (Acodecctx != NULL) {
        avcodec_free_context(&Acodecctx);
        Acodecctx = NULL;
        Acodec = NULL;
    }
    if (avformtctx != NULL) {
        avformat_close_input(&avformtctx);
        avformtctx = NULL;
    }
    qDebug() << "file close!\n";
}
