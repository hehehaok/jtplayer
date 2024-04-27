#ifndef PLAYEROUTPUT_H
#define PLAYEROUTPUT_H

#include "ffmpeginclude.h"
#include "avclock.h"
#include <memory>

class PlayerOutput
{
public:
    PlayerOutput();


public:
    void videoCallBack(std::shared_ptr<void> par);
    void displayImage(AVFrame *frame);


public:
    bool m_exit;
    AVClock m_videoClock;

};

#endif // PLAYEROUTPUT_H
