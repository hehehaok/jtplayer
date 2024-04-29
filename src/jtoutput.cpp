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
          m_jtdecoder(jtPlayer::get()->m_jtdecoder),
          m_clockInitFlag(false),
          m_frameTimer(0.00),
          m_exit(false),
          m_pause(false),
          m_speed(1.0),
          m_volume(30)
{
    outputInit();
}

JTOutput::~JTOutput()
{
    if (!m_exit) {
        exit();
    }
}

bool JTOutput::outputInit()
{
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
    qDebug() << "output exit!\n";
}

bool JTOutput::initVideo()
{
    m_videoIndex = jtPlayer::get()->Vstreamindex;
    m_videoFrameTimeBase = jtPlayer::get()->avformtctx->streams[m_videoIndex]->time_base;
    m_videoCodecPar = jtPlayer::get()->avformtctx->streams[m_videoIndex]->codecpar;
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
    m_buffer = NULL;
    m_buffer = (uint8_t*)av_realloc(m_buffer, bufferSize * sizeof(uint8_t));                   // 按照大小分配对应内存
    av_image_fill_arrays(m_pixels, m_pitch, m_buffer, m_dstPixFmt, m_dstPixWidth, m_dstPixHeight, 1); // 提供缓冲区中各分量入口以及对应大小
    return true;
}

void JTOutput::videoCallBack(std::shared_ptr<void> par)
{
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
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        if (m_jtdecoder->getRemainingVFrame()) {
            MyFrame* lastFrame = m_jtdecoder->peekLastVFrame();
            MyFrame* curFrame = m_jtdecoder->peekCurVFrame();

            if (curFrame->serial != m_jtdecoder->m_jtdemux->m_videoPacketQueue.serial) {
                m_jtdecoder->setNextVFrame();
                continue;
            }
            if (curFrame->serial != lastFrame->serial) {
                m_frameTimer = AVClock::getCurTimeStamp() / 1000000.0;
            }
            duration = vpDuration(curFrame, lastFrame); // 理论播放时长
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
            if (m_jtdecoder->getRemainingVFrame() > 1) {
                MyFrame* nextFrame = m_jtdecoder->peekNextVFrame();
                duration = nextFrame->pts - curFrame->pts;
                if (time > m_frameTimer + duration) { // 说明此时系统时间超过了本帧播放的时间，因此应该直接丢弃
                    m_jtdecoder->setNextVFrame();
                    qDebug() << "abandon the current video frame!\n";
                    continue;
                }
            }
            displayImage(&curFrame->frame);
            qDebug() << "display the frame, pts:" << curFrame->frame.pts * av_q2d(m_videoFrameTimeBase) << "!\n";
            m_jtdecoder->setNextVFrame();
        }
        else {
            qDebug() << "remaining video frame is 0!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

double JTOutput::vpDuration(MyFrame *curFrame, MyFrame *lastFrame)
{
    if (curFrame->serial == lastFrame->serial) {
        double duration = curFrame->pts - lastFrame->pts;
        // 如果时间差是NAN或大于等于AV_NOSYNC_THRESHOLD,则返回上一帧的持续时间
        if (isnan(duration) || duration > AV_NOSYNC_THRESHOLD) {
            return lastFrame->duration;
        }
        else {
            return duration;
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
        // 视频比音频慢，加快,diff为负值
        if (diff <= -sync)
        {
            // 不同步时间超过阈值，但当前时间戳与视频当前显示帧时间戳差值大于阈值，则将delay设置为当前时间戳与视频当前显示帧时间戳差值加上delay
            delay = FFMAX(0, diff + delay);
        }
        // 视频比音频快，减慢
        else if (diff >= sync && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
        {
            // 不同步时间超过阈值，但当前时间戳与视频当前显示帧时间戳差值小于阈值，则将delay设置为当前时间戳与视频当前显示帧时间戳差值加上delay
            delay = diff + delay;
        }
        // 视频比音频快，减慢
        else if (diff >= sync)
        {
            // 不同步时间小于阈值，且当前时间戳与视频当前显示帧时间戳差值小于阈值，则将delay设置为当前时间戳与视频当前显示帧时间戳差值加上delay的2倍
            delay = 2 * delay;
        }
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

    m_audioIndex = jtPlayer::get()->Astreamindex;
    m_audioFrameTimeBase = jtPlayer::get()->avformtctx->streams[m_audioIndex]->time_base;
    m_audioCodecPar = jtPlayer::get()->avformtctx->streams[m_audioIndex]->codecpar;

    m_audioBuffer = NULL;
    m_audioBufferSize = 0;
    m_audioBufferIndex = 0;
    m_lastAudioPts = -1;

    SDL_AudioSpec wantedSpec;
    wantedSpec.channels = m_audioCodecPar->channels;
    wantedSpec.freq = m_audioCodecPar->sample_rate * m_speed;
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
    m_dstChannelLayout = av_get_default_channel_layout(actualSpec.channels);
    m_dstNbSamples = av_samples_get_buffer_size(NULL, actualSpec.channels,
                                                1, m_dstSampleFmt, 1);

    m_audioFrame = av_frame_alloc();
    return true;
}

void JTOutput::audioCallBack(void *userData, uint8_t *stream, int len)
{
    memset(stream, 0, len);
    JTOutput* jtoutput = (JTOutput*) userData;
    double audioPts = 0.00;
    while (len > 0) {
        if (jtoutput->m_exit) {
            return;
        }
        if (jtoutput->m_audioBufferIndex >= jtoutput->m_audioBufferSize) { // 1帧数据读完了已经，重新拿一帧
            bool ret = jtoutput->m_jtdecoder->getAFrame(jtoutput->m_audioFrame);
            if (ret) {
                jtoutput->m_audioBufferIndex = 0;
                if ((jtoutput->m_dstSampleFmt != jtoutput->m_audioFrame->format ||
                     jtoutput->m_dstChannelLayout != jtoutput->m_audioFrame->channel_layout ||
                     jtoutput->m_dstFreq != jtoutput->m_audioFrame->sample_rate ||
                     jtoutput->m_dstNbSamples != jtoutput->m_audioFrame->nb_samples) &&
                     jtoutput->m_swrCtx == NULL) {
                    jtoutput->m_swrCtx = swr_alloc_set_opts(NULL, jtoutput->m_dstChannelLayout, jtoutput->m_dstSampleFmt, jtoutput->m_dstFreq,
                                                            jtoutput->m_audioFrame->channel_layout, (enum AVSampleFormat)jtoutput->m_audioFrame->format,
                                                            jtoutput->m_audioFrame->sample_rate, 0, NULL);
                    if (jtoutput->m_swrCtx == NULL || swr_init(jtoutput->m_swrCtx) < 0) {
                        qDebug() << "swr context init failed!\n";
                        return;
                    }
                }
                if (jtoutput->m_swrCtx) { // 先进行数据格式转换
                    const uint8_t **in = (const uint8_t **)jtoutput->m_audioFrame->extended_data;
                    qDebug() << "data:" << jtoutput->m_audioFrame->data << ",extended_data:" << jtoutput->m_audioFrame->extended_data;
                    // extended_data就是比如当音频存在8个通道且格式为planar时，由于data只有4个通道，因此放不下要放在extended_data中
                    int outCount = (uint64_t)jtoutput->m_audioFrame->nb_samples * jtoutput->m_dstFreq / jtoutput->m_audioFrame->sample_rate + 256;
                    int outSize = av_samples_get_buffer_size(NULL, jtoutput->m_dstChannels, jtoutput->m_audioFrame->nb_samples,
                                                              jtoutput->m_dstSampleFmt, 1);
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
                    int len2 = swr_convert(jtoutput->m_swrCtx, &jtoutput->m_audioBuffer, outCount, in, jtoutput->m_audioFrame->nb_samples);
                    if (len2 < 0) {
                        qDebug() << "swr convert failed!\n";
                        return;
                    }
                    jtoutput->m_audioBufferSize = av_samples_get_buffer_size(NULL, jtoutput->m_dstChannels, len2,
                                                                             jtoutput->m_dstSampleFmt, 0);
                }
                else { // 这个分支表示的是前后格式一样，因此不需要转换
                    jtoutput->m_audioBufferSize = av_samples_get_buffer_size(NULL, jtoutput->m_dstChannels, jtoutput->m_dstNbSamples,
                                                                             jtoutput->m_dstSampleFmt, 0);
                    av_fast_malloc(&jtoutput->m_audioBuffer, &jtoutput->m_audioBufferSize, jtoutput->m_audioBufferSize + 256);
                    if (jtoutput->m_audioBuffer == NULL) {
                        qDebug() << "av_fast_malloc failed!\n";
                        return;
                    }
                    memcpy(jtoutput->m_audioBuffer, jtoutput->m_audioFrame->data[0], jtoutput->m_audioBufferSize);
                }
                audioPts = jtoutput->m_audioFrame->pts * av_q2d(jtoutput->m_audioFrameTimeBase);
                av_frame_unref(jtoutput->m_audioFrame);
            }
            else {
                qDebug() << "get audio frame failed!\n";
                continue;
            }
        }
        int len1 = jtoutput->m_audioBufferSize - jtoutput->m_audioBufferIndex; // 数据还没读完
        len1 = (len1 > len ? len : len1);
        SDL_MixAudio(stream, jtoutput->m_audioBuffer + jtoutput->m_audioBufferIndex, len1, jtoutput->m_volume);
        len -= len1;
        jtoutput->m_audioBufferIndex += len1;
        stream += len1;
    }
    jtoutput->m_audioClock.setClock(audioPts);
    uint32_t _pts = (uint32_t)audioPts;
    if (jtoutput->m_lastAudioPts != _pts) {
        emit jtoutput->AVPtsChanged(_pts);
        jtoutput->m_lastAudioPts = _pts;
    }
}



















