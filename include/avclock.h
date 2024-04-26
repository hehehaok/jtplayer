#ifndef AVCLOCK_H
#define AVCLOCK_H

#include <chrono>
#include <QDebug>

class AVClock
{
public:
    AVClock() : m_pts(0.00), m_drift(0.00) {};
    ~AVClock() {}

    // 重置时间戳
    inline void reset() {
        m_pts = 0.00;
        m_drift = 0.00;
    }

    // 设置时间戳
    inline void setClock(double pts) {
        setClockAt(pts);
    }

    // 获取当前标准时钟的时间戳（比如以音频为基准的话得到的就是当前音频的时间戳）
    inline double getClock() {
        auto now = std::chrono::system_clock::now();
        auto cur_time_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

        qDebug() << "cur pts is " << m_drift + cur_time_us / 1000000.0 << "\n";
        return m_drift + cur_time_us / 1000000.0;
    }


private:
    // 设置时间戳
    inline void setClockAt(double pts) {
        auto now = std::chrono::system_clock::now();
        auto cur_time_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

        m_drift = pts - cur_time_us / 1000000.0;
        m_pts = pts;
    }


private:
    double m_pts;   // 当前pts
    double m_drift; // 当前pts与标准时钟的差值
};

#endif // AVCLOCK_H
