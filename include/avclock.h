#ifndef AVCLOCK_H
#define AVCLOCK_H

#include <chrono>
#include <QDebug>

class AVClock
{
public:
    AVClock() : m_drift(0.0), m_pauseDrift(0.0), m_pausePts(0.0), m_initFlag(false) {}
    ~AVClock() {}

    // 初始化时钟
    inline void initClock() {
        if (!m_initFlag) {
            setClock(0.0);
            m_initFlag = true;
        }
    }

    // 设置时间戳
    inline void setClock(double pts) {
        int64_t cur_time_us = getCurTimeStamp();
        m_drift = pts - cur_time_us / 1000000.0;
    }

    // 获取当前标准时钟的时间戳（比如以音频为基准的话得到的就是当前音频的时间戳）
    inline double getClock() {
        int64_t cur_time_us = getCurTimeStamp();
        return m_drift + cur_time_us / 1000000.0;
    }

    inline void pauseClock(bool pause) {
        if (pause) {
            m_pausePts = getCurTimeStamp() / 1000000.0;
            m_pauseDrift = m_drift;
        }
        else {
            double offset = getCurTimeStamp() / 1000000.0 - m_pausePts;
            m_drift = m_pauseDrift - offset;
        }
    }

    inline void seekClock(double seekTarget) {
        setClock(seekTarget);
    }

    // 获取当前系统时间
    static inline int64_t getCurTimeStamp() {
        auto now = std::chrono::system_clock::now();
        int64_t cur_time_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return cur_time_us;
    }

private:
    double m_drift;       // 输入的pts与标准时钟的差值
    double m_pauseDrift;  // 暂停时的时钟差值
    double m_pausePts;    // 暂停时的时间戳
    bool m_initFlag;      // 时钟是否已被初始化
};

#endif // AVCLOCK_H
