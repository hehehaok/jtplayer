#include "jtdemux.h"
#include "jtplayer.h"
#include <thread>

JTDemux::JTDemux() : m_maxPacketQueueSize(30)
{
    demuxInit();
}

JTDemux::~JTDemux()
{
    if (!m_exit) {
        exit();
    }
}

void JTDemux::demux(std::shared_ptr<void> param)
{
    AVPacket* avpkt = av_packet_alloc();
    int ret = -1;
    while (true) {
        if (m_exit) {
            break;
        }
        if (getPacketQueueSize(&m_audioPacketQueue) >= m_maxPacketQueueSize || getPacketQueueSize(&m_videoPacketQueue) >= m_maxPacketQueueSize)
        {
            qDebug() << "audio packet queue size is" << getPacketQueueSize(&m_audioPacketQueue) << ","
                     << "video packet queue size is" << getPacketQueueSize(&m_videoPacketQueue) << ", demux useless loop!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
//        if (m_isSeek)
        memset(avpkt, 0, sizeof(*avpkt));
        ret = av_read_frame(m_fmtCtx, avpkt);
        if (ret != 0) {
            av_strerror(ret, m_errorBuffer, sizeof(m_errorBuffer));
            qDebug() << "demux packet failed:" << m_errorBuffer << "\n";
            continue;
        }
        if (avpkt->stream_index == m_videoIndex) {
            qDebug() << "demux vedio packet pts:" << avpkt->pts * av_q2d(m_fmtCtx->streams[m_videoIndex]->time_base) << "\n";
            pushPacket(&m_videoPacketQueue, avpkt);
        }
        else if (avpkt->stream_index == m_audioIndex) {
            qDebug() << "demux audio packet pts:" << avpkt->pts * av_q2d(m_fmtCtx->streams[m_audioIndex]->time_base) << "\n";
            pushPacket(&m_audioPacketQueue, avpkt);
        }
        else {
            qDebug() << "demux useless packet!\n";
            av_packet_unref(avpkt);
        }
    }
    av_packet_free(&avpkt);
    qDebug() << "demux exit!\n";
}

void JTDemux::demuxInit()
{

    m_audioPacketQueue.pktQue.resize(m_maxPacketQueueSize);
    m_videoPacketQueue.pktQue.resize(m_maxPacketQueueSize);

    m_audioPacketQueue.size = 0;
    m_audioPacketQueue.pushIndex = 0;
    m_audioPacketQueue.readIndex = 0;
    m_audioPacketQueue.serial = 0;

    m_videoPacketQueue.size = 0;
    m_videoPacketQueue.pushIndex = 0;
    m_videoPacketQueue.readIndex = 0;
    m_videoPacketQueue.serial = 0;

    m_exit = false;
    m_errorBuffer[1023] = '\0';
    m_fmtCtx = jtPlayer::get()->avformtctx;
    m_videoIndex = jtPlayer::get()->Vstreamindex;
    m_audioIndex = jtPlayer::get()->Astreamindex;
}

void JTDemux::exit()
{
    m_exit = true;
    packetQueueFlush(&m_audioPacketQueue);
    packetQueueFlush(&m_videoPacketQueue);
    if (m_fmtCtx) {
        jtPlayer::get()->close();
        m_fmtCtx = NULL;
    }
    qDebug() << "demux exit!\n";
}

int JTDemux::getPacketQueueSize(PacketQueue *queue)
{
    std::unique_lock<std::mutex> lock(queue->mutex);
    return queue->size;
}

bool JTDemux::getPacket(PacketQueue *queue, AVPacket *pkt, PktDecoder *decoder)
{
    std::unique_lock<std::mutex> lock(queue->mutex);
    while (queue->size == 0) {
        bool ret = queue->cond.wait_for(lock, std::chrono::microseconds(100),
                                        [&] {return queue->size & !m_exit;});
        if (!ret) {
            if (decoder->codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
                qDebug() << "video packet queue size is" << queue->size << ", get packet failed!\n";
            else if (decoder->codecCtx->codec_type == AVMEDIA_TYPE_AUDIO)
                qDebug() << "audio packet queue size is" << queue->size << ", get packet failed!\n";
            return false;
        }
    }
    if (queue->serial != decoder->serial) {
        avcodec_flush_buffers(decoder->codecCtx);
        decoder->serial = queue->pktQue[queue->readIndex].serial;
        return false;
    }
    av_packet_move_ref(pkt, &queue->pktQue[queue->readIndex].packet);
    decoder->serial = queue->pktQue[queue->readIndex].serial;
    queue->readIndex = (queue->readIndex + 1) % m_maxPacketQueueSize;
    queue->size--;
    return true;
}

void JTDemux::pushPacket(PacketQueue *queue, AVPacket *pkt)
{
    std::unique_lock<std::mutex> lock(queue->mutex);
    av_packet_move_ref(&queue->pktQue[queue->pushIndex].packet, pkt);
    queue->pktQue[queue->pushIndex].serial = queue->serial;
    queue->pushIndex = (queue->pushIndex + 1) % m_maxPacketQueueSize;
    queue->size++;
}

void JTDemux::packetQueueFlush(PacketQueue *queue)
{
    std::lock_guard<std::mutex> lock(queue->mutex);
    while (queue->size) {
        av_packet_unref(&queue->pktQue[queue->readIndex].packet);
        queue->readIndex = (queue->readIndex + 1) % m_maxPacketQueueSize;
        queue->size--;
    }
    queue->serial++;
}
