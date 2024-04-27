#include "jtdecoder.h"
#include "jtdemux.h"
#include "jtplayer.h"
#include <thread>

JTDecoder::JTDecoder(std::shared_ptr<JTDemux> demux) : m_maxFrameQueueSize(16), m_demux(demux)
{
    decoderInit();
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

    m_audioPktDecoder.codecCtx = jtPlayer::get()->Acodecctx;
    m_audioPktDecoder.serial = 0;
    m_videoPktDecoder.codecCtx = jtPlayer::get()->Vcodecctx;
    m_videoPktDecoder.serial = 0;

    m_exit = false;
    m_errorBuffer[1023] = '\0';
    m_audioFrameTimeBase = jtPlayer::get()->avformtctx->streams[jtPlayer::get()->Vstreamindex]->time_base;
    m_videoFrameTimeBase = jtPlayer::get()->avformtctx->streams[jtPlayer::get()->Vstreamindex]->time_base;
}

void JTDecoder::pushAFrame(AVFrame *frame)
{
    std::unique_lock<std::mutex> lock(m_audioFrameQueue.mutex);
    av_frame_move_ref(&m_audioFrameQueue.frameQue[m_audioFrameQueue.pushIndex].frame, frame);
    m_audioFrameQueue.frameQue[m_audioFrameQueue.pushIndex].serial = m_audioPktDecoder.serial;
    m_audioFrameQueue.pushIndex = (m_audioFrameQueue.pushIndex + 1) % m_maxFrameQueueSize;
    m_audioFrameQueue.frameQue[m_audioFrameQueue.pushIndex].pts =
        m_audioFrameQueue.frameQue[m_audioFrameQueue.pushIndex].frame.pts * av_q2d(m_audioFrameTimeBase);
    m_audioFrameQueue.size++;
}

void JTDecoder::pushVFrame(AVFrame *frame)
{
    std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
    av_frame_move_ref(&m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].frame, frame);
    m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].serial = m_videoPktDecoder.serial;
    m_videoFrameQueue.pushIndex = (m_videoFrameQueue.pushIndex + 1) % m_maxFrameQueueSize;
    m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].pts =
        m_videoFrameQueue.frameQue[m_videoFrameQueue.pushIndex].frame.pts * av_q2d(m_videoFrameTimeBase);
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

void JTDecoder::audioDecoder(std::shared_ptr<void> param)
{
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int errtimes = 10; // 最多允许失败10次
    while (true) {
        if (m_exit) {
            break;
        }
        if (m_audioFrameQueue.size >= m_maxFrameQueueSize) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            qDebug() << "audio frame queue is full, audio decode useless loop!\n";
            continue;
        }
        bool ret = m_demux->getPacket(&m_demux->m_audioPacketQueue, packet, &m_audioPktDecoder);
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
                pushAFrame(frame);
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    av_packet_free(&packet);
    av_frame_free(&frame);
    qDebug() << "audio decode exit!\n";
}

void JTDecoder::videoDecoder(std::shared_ptr<void> param)
{
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    while (true) {
        if (m_exit) {
            break;
        }
        if (m_videoFrameQueue.size >= m_maxFrameQueueSize) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            qDebug() << "video frame queue is full, video decode useless loop!\n";
            continue;
        }
        bool ret = m_demux->getPacket(&m_demux->m_videoPacketQueue, packet, &m_videoPktDecoder);
        if (ret) {
            int err = avcodec_send_packet(m_videoPktDecoder.codecCtx, packet);
            if (err < 0 || err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "send video packet failed : " << m_errorBuffer << "\n";
                continue;
            }
            err = avcodec_receive_frame(m_videoPktDecoder.codecCtx, frame);
            if (err < 0) {
                av_strerror(err, m_errorBuffer, sizeof(m_errorBuffer));
                qDebug() << "receive video frame failed : " << m_errorBuffer << "\n";
                continue;
            }
            if (frame == nullptr || frame->format != 0) {
                qDebug() << "decode video frame failed!\n";
                continue;
            }
            else {
                pushVFrame(frame);
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    av_packet_free(&packet);
    av_frame_free(&frame);
    qDebug() << "video decode exit!\n";
}

void JTDecoder::exit()
{
    m_exit = true;
    frameQueueFlush(&m_audioFrameQueue);
    frameQueueFlush(&m_videoFrameQueue);
    qDebug() << "decoder exit!\n";
}















