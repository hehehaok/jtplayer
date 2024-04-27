#include "playeroutput.h"
#include "avclock.h"
#include <QDebug>

PlayerOutput::PlayerOutput()
{

}

void PlayerOutput::videoCallBack(std::shared_ptr<void> par)
{

}

void PlayerOutput::displayImage(AVFrame *frame)
{
    if (frame) {

    }
    else {
        qDebug() << "decode frame is null\n";
    }
}
