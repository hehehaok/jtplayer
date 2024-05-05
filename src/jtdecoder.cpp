#include "jtdecoder.h"
#include "jtdemux.h"
#include "jtplayer.h"
#include <thread>

#define CAL_VIDEODECODE_TIME

JTDecoder::JTDecoder() : m_maxFrameQueueSize(16), m_sleepTime(10)
{
    m_audioFrameQueue.size = 0;
    m_videoFrameQueue.size = 0;
}

JTDecoder::~JTDecoder()
{
    if (!m_exit) {
        exit();
    }
}

void JTDecoder::decoderInit()
{
    m_audioFrameQueue.frameQue.resize(m_maxFrameQueueSize);
    m_videoFrameQueue.frameQue.resize(m_maxFrameQueueSize);

    m_audioFrameQueue.size = 0;
    m_audioFrameQueue.readIndex = 0;
    m_audioFrameQueue.pushIndex = 0;
    m_audioFrameQueue.shown = 0;

    m_videoFrameQueue.size = 0;
    m_videoFrameQueue.readIndex = 0;
    m_videoFrameQueue.pushIndex = 0;
    m_videoFrameQueue.shown = 0;

    m_audioPktDecoder.codecCtx = JTPlayer::get()->m_audioCodecCtx;
    m_audioPktDecoder.serial = 0;
    m_videoPktDecoder.codecCtx = JTPlayer::get()->m_videoCodecCtx;
    m_videoPktDecoder.serial = 0;

    m_exit = false;
    m_pause = false;
    m_errorBuffer[1023] = '\0';
    m_audioFrameTimeBase = JTPlayer::get()->m_avFmtCtx->streams[JTPlayer::get()->m_audioStreamIndex]->time_base;
    m_videoFrameTimeBase = JTPlayer::get()->m_avFmtCtx->streams[JTPlayer::get()->m_videoStreamIndex]->time_base;
    AVRational videoFrameRate = av_guess_frame_rate(JTPlayer::get()->m_avFmtCtx,
                                           JTPlayer::get()->m_avFmtCtx->streams[JTPlayer::get()->m_videoStreamIndex], NULL);
    m_videoFrameDuration = (videoFrameRate.den && videoFrameRate.num) ? av_q2d(AVRational{videoFrameRate.den, videoFrameRate.num}) : 0.00;

    m_jtDemux = JTPlayer::get()->m_jtDemux;
}

void JTDecoder::pushAudioFrame(AVFrame *frame)
{
    std::unique_lock<std::mutex> lock(m_audioFrameQueue.mutex);
    av_frame_move_ref(&m_audioFrameQueue.frameQue[m_audioFrameQueue.pushIndex].frame, frame);
    m_audioFrameQueue.frameQue[m_audioFrameQueue.pushIndex].serial = m_audioPktDecoder.serial;
    m_audioFrameQueue.frameQue[m_audioFrameQueue.pushIndex].pts =
        m_audioFrameQueue.frameQue[m_audioFrameQueue.pushIndex].frame.pts * av_q2d(m_audioFrameTimeBase);
    m_audioFrameQueue.pushIndex = (m_audioFrameQueue.pushIndex + 1) % m_maxFrameQueueSize;
    m_audioFrameQueue.size++;
}

void JTDecoder::pushVideoFrame(AVFrame *frame)
{
    std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
    av_frame_move_ref(&m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].frame, frame);
    m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].serial = m_videoPktDecoder.serial;
    m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].duration = m_videoFrameDuration;
    m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].pts =
        m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].frame.pts * av_q2d(m_videoFrameTimeBase);
    m_videoFrameQueue.pushIndex = (m_videoFrameQueue.pushIndex + 1) % m_maxFrameQueueSize;
    m_videoFrameQueue.size++;
}

void JTDecoder::frameQueueFlush(FrameQueue *queue)
{
    std::lock_guard<std::mutex> lock(queue->mutex);
    while (queue->size) {
        av_frame_unref(&queue->frameQue[queue->readIndex].frame);
        queue->readIndex = (queue->readIndex + 1) % m_maxFrameQueueSize;
        queue->size--;
    }
}

int JTDecoder::getRemainingVideoFrame()
{
    std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
    if(!m_videoFrameQueue.size) {
        return 0;
    }
    return m_videoFrameQueue.size - m_videoFrameQueue.shown;
}

MyFrame *JTDecoder::peekLastVideoFrame()
{
    std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
    MyFrame* frame = &m_videoFrameQueue.frameQue[m_videoFrameQueue.readIndex];
    return frame;
}

MyFrame *JTDecoder::peekCurVideoFrame()
{
    std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
    while (!m_videoFrameQueue.size) {
        bool ret = m_videoFrameQueue.cond.wait_for(lock, std::chrono::milliseconds(100), [this]
                                                   { return m_videoFrameQueue.size && !m_exit; });
        if (!ret)
        {
            return nullptr;
        }
    }
    int index = (m_videoFrameQueue.readIndex + m_videoFrameQueue.shown) % m_maxFrameQueueSize;
    MyFrame* frame = &m_videoFrameQueue.frameQue[index];
    return frame;
}

MyFrame *JTDecoder::peekNextVideoFrame()
{
    std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
    while (m_videoFrameQueue.size < 2) { // 小于2的逻辑就是因为队列中会保留前一帧
        bool ret = m_videoFrameQueue.cond.wait_for(lock, std::chrono::milliseconds(100), [this]
                                                   { return m_videoFrameQueue.size && !m_exit; });
        if (!ret)
        {
            return nullptr;
        }
    }
    int index = (m_videoFrameQueue.readIndex + m_videoFrameQueue.shown + 1) % m_maxFrameQueueSize;
    MyFrame* frame = &m_videoFrameQueue.frameQue[index];
    return frame;
}

void JTDecoder::setNextVideoFrame()
{
    std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
    if (!m_videoFrameQueue.size)
    {
        return;
    }
    // 如果队列中的帧未被显示，则将其设置为已显示，并返回
    if (!m_videoFrameQueue.shown)
    {
        // 具体的逻辑就是，当读完第一帧(readIndex + shown = 0)，然后准备setNextVideoFrame的时候，发现目前读完的是第一帧
        // 因此只是将shown置为1，即加1，然后直接返回，不会真的将readIndex加1，切换到下一帧，即保留了前一帧
        // 当再次取读取的时候(readIndex + shown = 1)，由于shown加了1，因此同样能读取到第二帧，同时不影响保留前一帧，
        // 因此索引readIndex + shown对应的是最新要去读取的帧，而readIndex对应的是前一帧，且前一帧是一直得到保留的
        m_videoFrameQueue.shown = 1;
        return;
    }
    // 将当前读取到的帧reference释放，并将下一个帧设置为当前帧
    av_frame_unref(&m_videoFrameQueue.frameQue[m_videoFrameQueue.readIndex].frame);
    m_videoFrameQueue.readIndex = (m_videoFrameQueue.readIndex + 1) % m_maxFrameQueueSize;
    if (m_videoFrameQueue.frameQue[m_videoFrameQueue.readIndex].frame.format == -1)
    {
        qDebug() << "error of this frame!" << m_videoFrameQueue.readIndex + 1 << "\n";
    }
    m_videoFrameQueue.size--;
}

bool JTDecoder::getAudioFrame(AVFrame *frame)
{
    if (frame == nullptr) {
        qDebug() << "the frame is nullptr, get audio frame failed!\n";
        return false;
    }
    std::unique_lock<std::mutex> lock(m_audioFrameQueue.mutex);
    while (m_audioFrameQueue.size == 0) {
        bool ret = m_audioFrameQueue.cond.wait_for(lock, std::chrono::milliseconds(100), [&]
                                                {return m_audioFrameQueue.size && !m_exit; });
        if (!ret) {
            return false;
        }
    }
    if (m_audioFrameQueue.frameQue[m_audioFrameQueue.readIndex].serial != m_jtDemux->m_audioPacketQueue.serial)
    {
        av_frame_unref(&m_audioFrameQueue.frameQue[m_audioFrameQueue.readIndex].frame);
        m_audioFrameQueue.readIndex = (m_audioFrameQueue.readIndex + 1) % m_maxFrameQueueSize;
        m_audioFrameQueue.size--;
        qDebug() << "the frame serial changed, get audio frame failed!\n";
        return false;
    }
    av_frame_move_ref(frame, &m_audioFrameQueue.frameQue[m_audioFrameQueue.readIndex].frame);
    m_audioFrameQueue.readIndex = (m_audioFrameQueue.readIndex + 1) % m_maxFrameQueueSize;
    m_audioFrameQueue.size--;
    return true;
}

void JTDecoder::audioDecoder(std::shared_ptr<void> param)
{
    Q_UNUSED(param);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int errtimes = 10; // 最多允许失败10次
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
        if (m_audioFrameQueue.size >= m_maxFrameQueueSize) {
            qDebug() << "audio frame queue size is" << m_audioFrameQueue.size << ", audio decode useless loop!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleepTime));
            continue;
        }
        bool ret = m_jtDemux->getPacket(&m_jtDemux->m_audioPacketQueue, packet, &m_audioPktDecoder);
        if (ret) {
            int err = avcodec_send_packet(m_audioPktDecoder.codecCtx, packet);
            if (err < 0 || err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "send audio packet failed : " << m_errorBuffer << "\n";
                continue;
            }
            err = avcodec_receive_frame(m_audioPktDecoder.codecCtx, frame);
            if (err < 0) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "receive audio frame failed : " << m_errorBuffer << "\n";
                continue;
            }
            if (frame == nullptr) {
                if (errtimes == 0) {
                    qDebug() << "decode audio packet failed!\n";
                    break;
                }
                errtimes--;
                qDebug() << "error times--\n";
                continue;
            }
            else {
                pushAudioFrame(frame);
            }
        }
        else {
            qDebug() << "get audio packet failed!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleepTime));
        }
    }
    av_packet_free(&packet);
    av_frame_free(&frame);
    qDebug() << "audio decode exit!\n";
}

void JTDecoder::videoDecoder(std::shared_ptr<void> param)
{
    Q_UNUSED(param);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
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
        if (m_videoFrameQueue.size >= m_maxFrameQueueSize) {
            qDebug() << "video frame queue size is" << m_videoFrameQueue.size << ", video decode useless loop!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleepTime));
            continue;
        }
        bool ret = m_jtDemux->getPacket(&m_jtDemux->m_videoPacketQueue, packet, &m_videoPktDecoder);
        if (ret) {
            #ifdef CAL_VIDEODECODE_TIME
            auto start = std::chrono::system_clock::now();
            #endif
            int err = avcodec_send_packet(m_videoPktDecoder.codecCtx, packet);
            if (err < 0 || err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "send video packet failed:" << m_errorBuffer << "\n";
                continue;
            }
            err = avcodec_receive_frame(m_videoPktDecoder.codecCtx, frame);
            if (err < 0) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "receive video frame failed:" << m_errorBuffer << "\n";
                continue;
            }
            if (frame == nullptr || frame->format == -1) {
                qDebug() << "decode video frame failed!\n";
                continue;
            }
            else {
                #ifdef CAL_VIDEODECODE_TIME
                auto end = std::chrono::system_clock::now();
                auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                qDebug() << "decode time is" << time << "==================================\n";
                #endif
                qDebug() << "push a video frame!video frame size is" << m_videoFrameQueue.size << "\n";
                pushVideoFrame(frame);
            }
        }
        else {
            qDebug() << "get video packet failed!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleepTime));
        }
    }
    av_packet_free(&packet);
    av_frame_free(&frame);
    qDebug() << "video decode exit!\n";
}

void JTDecoder::exit()
{
    m_exit = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_jtDemux.reset();
    frameQueueFlush(&m_audioFrameQueue);
    frameQueueFlush(&m_videoFrameQueue);
    qDebug() << "decoder exit!\n";
}















