#include "jtdemux.h"
#include "jtplayer.h"
#include <thread>

JTDemux::JTDemux() : m_maxPacketQueueSize(30)
{
    m_audioPacketQueue.size = 0;
    m_videoPacketQueue.size = 0;
}

JTDemux::~JTDemux()
{
    if (!m_exit) {
        exit();
    }
}

void JTDemux::demux(std::shared_ptr<void> param)
{
    Q_UNUSED(param);
    AVPacket* avpkt = av_packet_alloc();
    int ret = -1;
    while (true) {
        if (m_exit) {
            break;
        }
        if (m_pause) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
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
        ret = av_read_frame(m_avFmtCtx, avpkt);
        if (ret != 0) {
            av_strerror(ret, m_errorBuffer, sizeof(m_errorBuffer));
            qDebug() << "demux packet failed:" << m_errorBuffer << "\n";
            continue;
        }
        if (avpkt->stream_index == m_videoStreamIndex) {
            qDebug() << "demux vedio packet pts:" << avpkt->pts * av_q2d(m_avFmtCtx->streams[m_videoStreamIndex]->time_base) << "\n";
            pushPacket(&m_videoPacketQueue, avpkt);
        }
        else if (avpkt->stream_index == m_audioStreamIndex) {
            qDebug() << "demux audio packet pts:" << avpkt->pts * av_q2d(m_avFmtCtx->streams[m_audioStreamIndex]->time_base) << "\n";
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
    m_pause= false;
    m_errorBuffer[1023] = '\0';
    m_avFmtCtx = JTPlayer::get()->m_avFmtCtx;
    m_videoStreamIndex = JTPlayer::get()->m_videoStreamIndex;
    m_audioStreamIndex = JTPlayer::get()->m_audioStreamIndex;
}

void JTDemux::exit()
{
    m_exit = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    packetQueueFlush(&m_audioPacketQueue);
    packetQueueFlush(&m_videoPacketQueue);
    m_avFmtCtx = NULL;
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
                                        [&] {return queue->size && !m_exit;});
        if (!ret) {
            if (decoder->codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
                qDebug() << "video packet queue size is" << queue->size << ", get packet failed!\n";
            else if (decoder->codecCtx->codec_type == AVMEDIA_TYPE_AUDIO)
                qDebug() << "audio packet queue size is" << queue->size << ", get packet failed!\n";
            return false;
        }
    }
    // getPakcet主要在解码线程中用到，当解码线程发现自己的解码器序号与包队列的解码器序号对不上时
    // 就知道已经发生跳转了，此时解码器需要清空自己的缓存，然后更新序列号
    // 但是为什么用packet的序列号更新，而不是用queue的序列号更新呢
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
