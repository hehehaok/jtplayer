#ifndef PTSSLIDER_H
#define PTSSLIDER_H

#include <QSlider>

class PtsSlider : public QSlider
{
    Q_OBJECT
public:
    PtsSlider(QWidget *parent = nullptr);
    inline double getPercent() {return m_percent;}
    inline double getCursorPercent() {return m_cursorPercent;}
    inline bool getMousePressed() {return m_mousePressed;}
    inline int calValue(double percent);
    void setSliderPos(double percent);


signals:
    void sliderReleased();
    void sliderMoved();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    double m_percent;            // 当前进度条的百分比
    double m_cursorPercent;      // 当前光标所在位置的百分比
    bool m_mousePressed;         // 鼠标是否已经按下
};

#endif // PTSSLIDER_H
