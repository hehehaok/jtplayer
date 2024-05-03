#include "ptsslider.h"
#include <QMouseEvent>

PtsSlider::PtsSlider(QWidget *parent) : QSlider(parent), m_percent(0.00), m_cursorPercent(0.00), m_mousePressed(false)
{
    setMouseTracking(true);
}

int PtsSlider::calValue(double percent)
{
    return (percent * (maximum() - minimum())) + minimum();
}

void PtsSlider::setSliderPos(double percent)
{
    m_percent = percent;
    setValue(calValue(percent));
}

void PtsSlider::mousePressEvent(QMouseEvent *event)
{
    double percent = double(event->pos().x()) / width();
    setValue(calValue(percent));
    m_mousePressed = true;
}

void PtsSlider::mouseMoveEvent(QMouseEvent *event)
{
    int posX = event->pos().x();
    if (posX > width())
        posX = width();
    if (posX < 0)
        posX = 0;
    m_cursorPercent = double(posX) / width();
    if (m_mousePressed) {
        setValue(calValue(m_cursorPercent));
    }
    emit sliderMoved();
}

void PtsSlider::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_percent = double(value()) / (maximum() - minimum());
    emit sliderReleased();
    m_mousePressed = false;
}
