#include "SDL2/SDL.h"
#include "jtoutput.h"
#include "jtplayer.h"
#include "jtdecoder.h"
#include "jtdemux.h"
#include "vframe.h"
#include <thread>
#include <chrono>


// 音视频同步相关宏(单位秒)

#define AV_SYNC_THRESHOLD_MIN 0.04  // 同步阈值下限

#define AV_SYNC_THRESHOLD_MAX 0.1   // 同步阈值上限

// 单帧视频时长阈值上限，用于适配低帧时同步，
// 帧率过低视频帧超前不适合翻倍延迟，应特殊
// 处理，这里设置上限一秒10帧
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1

#define AV_NOSYNC_THRESHOLD 10.0    // 同步操作摆烂阈值上限，此时同步已无意义

#define AV_SYNC_REJUDGESHOLD 0.01



JTOutput::JTOutput(QObject *parent) : QObject(parent),
    m_videoCodecPar(NULL),
    m_swsCtx(NULL),
    m_videoBuffer(NULL),
    m_swrCtx(NULL),
    m_audioBuffer(NULL),
    m_audioFrame(NULL),
    m_speedConverter(NULL),
    m_sonicBuffer(NULL),
    m_volume(30),
    m_sleepTime(10)
{

}

JTOutput::~JTOutput()
{
    if (!m_exit) {
        exit();
    }
}

bool JTOutput::outputInit()
{
    m_exit = false;
    m_pause = false;
    m_step = false;
    m_speed = 1.0;
    m_lastSpeed = 1.0;
    m_speedChanged = false;
    m_frameTimer = 0.00;
    m_clockInitFlag = false;
    m_jtDecoder = JTPlayer::get()->m_jtDecoder;
    m_errorBuffer[1023] = '\0';

    if(!initVideo()) {
        qDebug() << "init video failed!\n";
        return false;
    }
    if(!initAudio()) {
        qDebug() << "init audio failed!\n";
        return false;
    }
    return true;
}

void JTOutput::exit()
{
    m_exit = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (m_audioFrame != NULL) {
        av_frame_free(&m_audioFrame);
    }
    if (m_swsCtx != NULL) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = NULL;
    }
    if (m_swrCtx != NULL) {
        swr_free(&m_swrCtx);
    }
    if (m_videoBuffer != NULL) {
        av_freep(&m_videoBuffer);
    }
    if (m_audioBuffer != NULL) {
        av_freep(&m_audioBuffer);
    }
    if (m_sonicBuffer != NULL) {
        av_freep(&m_sonicBuffer);
    }
    if (m_speedConverter != NULL) {
        sonicDestroyStream(m_speedConverter);
        m_speedConverter = NULL;
    }
    qDebug() << "output exit!\n";
}

bool JTOutput::initVideo()
{
    m_videoIndex = JTPlayer::get()->m_videoStreamIndex;
    m_videoFrameTimeBase = JTPlayer::get()->m_avFmtCtx->streams[m_videoIndex]->time_base;
    m_videoCodecPar = JTPlayer::get()->m_avFmtCtx->streams[m_videoIndex]->codecpar;
    m_dstPixWidth = m_videoCodecPar->width;         // 目标图像宽度，初始值设置与源相同
    m_dstPixHeight = m_videoCodecPar->height;       // 目标图像高度，初始值设置与源相同
    m_dstPixFmt = AV_PIX_FMT_YUV420P;
    m_swsFlags = SWS_BICUBIC;
    m_swsCtx = sws_getContext(m_videoCodecPar->width, m_videoCodecPar->height, (enum AVPixelFormat)m_videoCodecPar->format,
                              m_dstPixWidth, m_dstPixHeight, m_dstPixFmt,
                              SWS_BICUBIC, NULL, NULL, NULL);
    if (m_swsCtx == NULL) {
        qDebug() << "allocate the SwsContext failed\n";
        return false;
    }

    int bufferSize = av_image_get_buffer_size(m_dstPixFmt, m_dstPixWidth, m_dstPixHeight, 1);  // 计算对应格式、对应宽高下一帧图像所需要的缓冲区大小
    m_videoBuffer = (uint8_t*)av_realloc(NULL, bufferSize * sizeof(uint8_t));                   // 按照大小分配对应内存
    av_image_fill_arrays(m_pixels, m_pitch, m_videoBuffer, m_dstPixFmt, m_dstPixWidth, m_dstPixHeight, 1); // 提供缓冲区中各分量入口以及对应大小
    return true;
}

void JTOutput::videoCallBack(std::shared_ptr<void> param)
{
    Q_UNUSED(param);
    double time = 0.00;
    double duration = 0.00;
    double delay = 0.00;
    if (!m_clockInitFlag) {
        initAVClock();
    }
    while (true) {
        if (m_exit) {
            break;
        }
        if (m_pause) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleepTime));
            if(!JTPlayer::get()->m_step) {
                continue;  // 只有当m_pause为真且m_step为假时才是真正的暂停
            }
        }
        if (m_jtDecoder->getRemainingVideoFrame()) {
            MyFrame* lastFrame = m_jtDecoder->peekLastVideoFrame();
            MyFrame* curFrame = m_jtDecoder->peekCurVideoFrame();

            if (curFrame->serial != m_jtDecoder->m_jtDemux->m_videoPacketQueue.serial) {
                m_jtDecoder->setNextVideoFrame();  // 帧的序列号和包队列的序列号对不上，直接丢掉当前帧
                continue;
            }
            if (curFrame->serial != lastFrame->serial) {
                m_frameTimer = AVClock::getCurTimeStamp() / 1000000.0; // 新的帧和包队列相同且和前一帧不相同，说明是新序列号的第一帧
            }
            duration = vpDuration(curFrame, lastFrame, m_speed); // 理论播放时长
            delay = computeTargetDelay(duration);       // 实际应该的播放时长
            time = AVClock::getCurTimeStamp() / 1000000.0;
            if (time < m_frameTimer + delay) { // 说明此时这一帧还没到播完的时候，需要继续播
                auto sleepTime = (uint32_t)(FFMIN(AV_SYNC_REJUDGESHOLD, m_frameTimer + delay - time) * 1000);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
                continue;
            }
            m_frameTimer += delay;
            if (time - m_frameTimer > AV_SYNC_THRESHOLD_MAX) {
                m_frameTimer = time;
            }
            if (m_jtDecoder->getRemainingVideoFrame() > 1) {
                MyFrame* nextFrame = m_jtDecoder->peekNextVideoFrame();
                duration = nextFrame->pts - curFrame->pts;
                if (time > m_frameTimer + duration) { // 说明此时系统时间超过了本帧播放的时间，因此应该直接丢弃
                    m_jtDecoder->setNextVideoFrame();
                    qDebug() << "abandon the current video frame!\n";
                    continue;
                }
            }
            displayImage(&curFrame->frame);
            qDebug() << "display the frame, pts:" << curFrame->frame.pts * av_q2d(m_videoFrameTimeBase) << "!\n";
            m_jtDecoder->setNextVideoFrame();
        }
        else {
//            if (JTPlayer::get()->m_end) {
//                JTPlayer::get()->endPause();
//            }
            // 暂时废除这部分逻辑，当帧率较低时很有可能发生解复用到结束了但视频帧获取失败导致视频意外停止的错误逻辑
            // 还是按照ffplay的，当解复用到结尾时自动循环线程阻塞同时阻塞后续流程
            qDebug() << "remaining video frame is 0!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleepTime));
        }
    }
    qDebug() << "video callback exit!\n";
}

void JTOutput::displayImage(AVFrame *frame)
{
    if (frame && frame->format != -1) {
        m_swsCtx = sws_getCachedContext(m_swsCtx, frame->width, frame->height, (enum AVPixelFormat)frame->format,
                                        m_dstPixWidth, m_dstPixHeight, m_dstPixFmt,
                                        m_swsFlags, NULL, NULL, NULL);
        if (m_swsCtx == NULL) {
            qDebug() << "reallocate the SwsContext failed\n";
            return;
        }
        int ret = sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height,
                            m_pixels, m_pitch);
        if (ret <= 0) {
            qDebug() << "sws_scale failed\n";
            return;
        }
//        qDebug() << "planeY:" << m_pixels[0] << ",planeU:" << m_pixels[1] << ",planeV:" << m_pixels[2];
        std::shared_ptr<YUV420Frame> dst_frame = std::make_shared<YUV420Frame>(m_pixels[0], m_dstPixWidth, m_dstPixHeight);
        emit frameChanged(dst_frame);
        m_videoClock.setClock(frame->pts * av_q2d(m_videoFrameTimeBase));
        if (JTPlayer::get()->m_step) {
            JTPlayer::get()->m_step = false; // 当m_step为真时即逐帧状态时，播放完一帧后将m_step重新取为false从而达到播完一帧再次暂停的效果
        }
    }
    else {
        qDebug() << "the frame to display is null!\n";
    }
}

void JTOutput::initAVClock()
{
    m_audioClock.setClock(0.00);
    m_videoClock.setClock(0.00);
    m_clockInitFlag = true;
}

double JTOutput::vpDuration(MyFrame *curFrame, MyFrame *lastFrame, float speed)
{
    if (curFrame->serial == lastFrame->serial) {
        double duration = curFrame->pts - lastFrame->pts;
        // 如果时间差是NAN或大于等于AV_NOSYNC_THRESHOLD,则返回上一帧的持续时间
        if (isnan(duration) || duration > AV_NOSYNC_THRESHOLD) {
            return lastFrame->duration / speed;
        }
        else {
            return duration / speed;
        }
    }
    return 0.0;
}

double JTOutput::computeTargetDelay(double delay)
{
    double diff = m_videoClock.getClock() - m_audioClock.getClock();
    // 计算同步阈值
    double sync = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
    // 不同步时间超过阈值直接放弃同步
    if (!isnan(diff) && abs(diff) < AV_NOSYNC_THRESHOLD)
    {
        // diff为负值，视频比音频慢，加快
        if (diff <= -sync)
        {
            // 理论播放时间为delay，但是此时视频已经慢于音频了，因此实际的播放时间应该要等于理论播放时间减去已经慢的时间差
            // 相当于理论播放时间变小了
            delay = FFMAX(0, diff + delay);
        }
        // diff为正值，视频比音频快，减慢
        else if (diff >= sync && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
        {
            // 理论播放时间为delay，但是此时视频已经快于音频了，理论播放时间超过了0.1秒
            // 因此实际的播放时间应该要等于理论播放时间加上已经快的时间差
            // 相当于理论播放时间变长了
            // 这个逻辑需要delay大于0.1，这种情况一般是帧率小于10的视频才会出现，所以一般不会走这个逻辑
            delay = diff + delay;
        }
        // 视频比音频快，减慢
        else if (diff >= sync)
        {
            // 不同步时间小于阈值，且理论播放时间小于0.1秒，说明没有快那么多
            // 则将delay设置为当前时间戳与视频当前显示帧时间戳差值加上delay的2倍
            delay = 2 * delay;
        }
        // 当diff在-sync和sync之间时，即认为当前是同步的，实际播放时间就等于理论播放时间
    }
    return delay;
}

bool JTOutput::initAudio()
{
    int ret = SDL_Init(SDL_INIT_AUDIO);
    if (ret < 0) {
        qDebug() << "SDL_Init failed:" << SDL_GetError();
        return false;
    }

    m_audioIndex = JTPlayer::get()->m_audioStreamIndex;
    m_audioFrameTimeBase = JTPlayer::get()->m_avFmtCtx->streams[m_audioIndex]->time_base;
    m_audioCodecPar = JTPlayer::get()->m_avFmtCtx->streams[m_audioIndex]->codecpar;
    m_swrCtx = NULL;

    m_audioBuffer = NULL;
    m_audioBufferSize = 0;
    m_audioBufferIndex = 0;
    m_lastAudioPts = -1;
    m_sonicBufferSize = 0;
    m_sonicBufferIndex = 0;


    SDL_AudioSpec wantedSpec;
    wantedSpec.channels = m_audioCodecPar->channels;
    wantedSpec.freq = m_audioCodecPar->sample_rate;
    wantedSpec.format = AUDIO_S16SYS;
    wantedSpec.silence = 0; // 表示静音值，当设备暂停时会往设备中写入静音值
    wantedSpec.callback = audioCallBack;
    wantedSpec.userdata = this;
    wantedSpec.samples = FFMAX(512, 2 << av_log2(wantedSpec.freq / 30));
    SDL_AudioSpec actualSpec;

    ret = SDL_OpenAudio(&wantedSpec, &actualSpec);
    if (ret < 0) {
        qDebug() << "SDL_OpenAudio failed:" << SDL_GetError();
        return false;
    }

    m_dstSampleFmt = AV_SAMPLE_FMT_S16;
    m_dstChannels = actualSpec.channels;
    m_dstFreq = actualSpec.freq;
    m_initDstFreq = actualSpec.freq;
    m_dstChannelLayout = av_get_default_channel_layout(actualSpec.channels);

    m_audioFrame = av_frame_alloc();
    return true;
}

bool JTOutput::initSwrCtx(int inChannels, int inSampleRate, AVSampleFormat inFmt, int outChannels, int outSampleRate, AVSampleFormat outFmt)
{
    if (inChannels == outChannels && inSampleRate == outSampleRate && inFmt == outFmt) {
        return true;
    }

    m_swrCtx = swr_alloc_set_opts(NULL, av_get_default_channel_layout(outChannels), outFmt, outSampleRate,
                                        av_get_default_channel_layout(inChannels), inFmt, inSampleRate, 0, NULL);

   if (!m_swrCtx) {
       qDebug() << "swr_alloc_set_opts failed!\n";
       return false;
   }

   int ret = swr_init(m_swrCtx);
   if (ret != 0) {
       qDebug() << "swr_init failed!\n";
       return false;
   }

   m_srcChannels = inChannels;
   m_srcFreq = inSampleRate;
   m_srcSampleFmt = inFmt;
   m_srcChannelLayout = av_get_default_channel_layout(inChannels);
   return true;
}

bool JTOutput::isNormalSpeed()
{
    return m_speed == 1.0;
}

bool JTOutput::audioSpeedConvert()
{
    if (m_speedConverter == NULL) {
        m_speedConverter = sonicCreateStream(m_dstFreq, m_dstChannels);
    }
    sonicSetSpeed(m_speedConverter, m_speed);

    int audioBufferSamples = m_audioBufferSize / (m_dstChannels * av_get_bytes_per_sample(m_dstSampleFmt));

    int ret = sonicWriteShortToStream(m_speedConverter, (short*)m_audioBuffer, audioBufferSamples);
    if (ret == 0) {
        qDebug() << "sonicWriteShortToStream failed!\n";
        return false;
    }

    int estSonicNbSamples = 2 * audioBufferSamples / m_speed;
    uint32_t estSonicBufferSize = estSonicNbSamples * m_dstChannels * av_get_bytes_per_sample(m_dstSampleFmt);
    av_fast_malloc(&m_audioBuffer, &m_audioBufferSize, estSonicBufferSize);
    int actualSonicNbSamples = sonicReadShortFromStream(m_speedConverter, (short*)m_audioBuffer, estSonicNbSamples);
    uint32_t actualSonicBufferSize = actualSonicNbSamples * m_dstChannels * av_get_bytes_per_sample(m_dstSampleFmt);
    m_audioBufferSize = actualSonicBufferSize;

//    if (m_sonicBufferSize < estSonicBufferSize) {
//        m_sonicBuffer = (short*)av_realloc(m_sonicBuffer, estSonicBufferSize);
//        m_sonicBufferSize = estSonicBufferSize;
//    }

//    int actualSonicNbSamples = sonicReadShortFromStream(m_speedConverter, m_sonicBuffer, estSonicNbSamples);
//    uint32_t actualSonicBufferSize = actualSonicNbSamples * m_dstChannels * av_get_bytes_per_sample(m_dstSampleFmt);

//    av_freep(&m_audioBuffer);
//    m_audioBuffer = (uint8_t*)m_sonicBuffer;
//    m_audioBufferSize = actualSonicBufferSize;
    return true;
}

void JTOutput::audioCallBack(void *userData, uint8_t *stream, int len)
{
    memset(stream, 0, len);
    JTOutput* jtoutput = (JTOutput*) userData;
    double audioPts = -1.0;
    while (len > 0) {
        if (jtoutput->m_exit) {
            return;
        }
        if (jtoutput->m_pause) {
            std::this_thread::sleep_for(std::chrono::milliseconds(jtoutput->m_sleepTime));
            if(!JTPlayer::get()->m_step) {
                continue;  // 只有当m_pause为真且m_step为假时才是真正的暂停
            }
        }
        if (jtoutput->m_audioBufferIndex >= jtoutput->m_audioBufferSize) { // 1帧数据读完了已经，重新拿一帧
            bool ret = jtoutput->m_jtDecoder->getAudioFrame(jtoutput->m_audioFrame);
            if (ret) {
                jtoutput->m_audioBufferIndex = 0;
                if (jtoutput->m_swrCtx == NULL) {
                    jtoutput->initSwrCtx(jtoutput->m_audioFrame->channels, jtoutput->m_audioFrame->sample_rate, (enum AVSampleFormat)jtoutput->m_audioFrame->format,
                                         jtoutput->m_dstChannels, jtoutput->m_initDstFreq, jtoutput->m_dstSampleFmt);
                }
                if (jtoutput->m_swrCtx) { // 先进行数据格式转换
                    const uint8_t **in = (const uint8_t **)jtoutput->m_audioFrame->extended_data;
                    // qDebug() << "data:" << jtoutput->m_audioFrame->data << ",extended_data:" << jtoutput->m_audioFrame->extended_data;
                    // extended_data就是比如当音频存在9个通道且格式为planar时，由于data只有8个通道，因此放不下要放在extended_data中
                    int estDstNbSamples = (uint64_t)jtoutput->m_audioFrame->nb_samples * jtoutput->m_initDstFreq / jtoutput->m_audioFrame->sample_rate + 256;
                    // 估计重采样后一帧内单个通道的采样点数，一般把这个值估计的稍微大一点，反正最后实际的大小会由swr_convert返回值得到
                    int outSize = av_samples_get_buffer_size(NULL, jtoutput->m_dstChannels, estDstNbSamples, jtoutput->m_dstSampleFmt, 0);
                    // 根据采样点数、量化比特位数、通道数即可计算出需要的缓冲区字节数
                    if (outSize < 0) {
                        qDebug() << "swr av_samples_get_buffer_size failed!\n";
                        return;
                    }
                    av_fast_malloc(&jtoutput->m_audioBuffer, &jtoutput->m_audioBufferSize, outSize);
                    if (jtoutput->m_audioBuffer == NULL)
                    {
                        qDebug() << "swr av_fast_malloc failed!\n";
                        return;
                    }
                    int actualDstNbSamples = swr_convert(jtoutput->m_swrCtx, &jtoutput->m_audioBuffer, estDstNbSamples, in, jtoutput->m_audioFrame->nb_samples);
                    if (actualDstNbSamples < 0) {
                        qDebug() << "swr convert failed!\n";
                        return;
                    }
                    jtoutput->m_audioBufferSize = av_samples_get_buffer_size(NULL, jtoutput->m_dstChannels, actualDstNbSamples,
                                                                             jtoutput->m_dstSampleFmt, 1);
                }
                else { // 这个分支表示的是前后格式一样，因此不需要转换
                    // 由于不需要转换因此直接按照Frame设置对应缓冲区大小即可
                    // 注意这里不能选择内存对齐，选择内存对齐的话会把padding的数据也被读进去，这样就会有噪声
                    jtoutput->m_audioBufferSize = av_samples_get_buffer_size(NULL, jtoutput->m_audioFrame->channels, jtoutput->m_audioFrame->nb_samples,
                                                                             (enum AVSampleFormat)jtoutput->m_audioFrame->format, 1);
                    jtoutput->m_audioBuffer = jtoutput->m_audioFrame->data[0];
                }
            audioPts = jtoutput->m_audioFrame->pts * av_q2d(jtoutput->m_audioFrameTimeBase);
            av_frame_unref(jtoutput->m_audioFrame);
            }
            else {
                qDebug() << "get audio frame failed!\n";
                continue;
            }
            // 如果需要变速则需要对该帧数据进行进一步的数据转换
            if (jtoutput->m_speedChanged) {
                qDebug() << "speed changed:" << jtoutput->m_speed << "========================\n";
                jtoutput->m_speedChanged = false;
            }
            if (!jtoutput->isNormalSpeed()) {
                jtoutput->audioSpeedConvert();
            }
        }
        int len1 = jtoutput->m_audioBufferSize - jtoutput->m_audioBufferIndex; // 数据还没读完
        len1 = (len1 > len ? len : len1); // 如果缓冲区中的数据大小大于回调函数单次读取的大小，则先读len然后剩下的下次再读
        SDL_MixAudio(stream, jtoutput->m_audioBuffer + jtoutput->m_audioBufferIndex, len1, jtoutput->m_volume);
        len -= len1;
        jtoutput->m_audioBufferIndex += len1;
        stream += len1;
        // 需要注意一下 len的单位是字节,len1的单位也是字节,m_audioBufferIndex的单位也是字节
    }
    if (audioPts >= 0) {
        jtoutput->m_audioClock.setClock(audioPts);
        int64_t _pts = (int64_t)(audioPts + 0.5);
        if (jtoutput->m_lastAudioPts != _pts) {
            emit jtoutput->ptsChanged(_pts);
            jtoutput->m_lastAudioPts = _pts;
        }
    }
}



















