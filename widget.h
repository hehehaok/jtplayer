#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include "jtplayer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    void init();
    void setWidgetsState(PlayerState playerState);

private slots:
    void onOpenFileBtnClicked();
    void onPauseBtnClicked();
    void updatePauseBtnStyle(PlayerState playerState);
    void onStopBtnClicked();
    void totalTimeChanged(int64_t duration);
    void sliderTimeChanged(int64_t curPts);
    void muteChanged(bool checked);
    void volumeSliderMoved();
    void setStep(bool checked);
    void onStepBtnClicked();
    void onForwardBtnClicked();
    void onBackwardBtnClicked();
    void ptsSliderMoved();
    void ptsSliderReleased();
    void speedChanged(const QString& curText);

signals:
    void volumeChanged(int volume);


private:
    Ui::Widget *ui;
    int64_t m_duration;  // 当前播放视频的播放时长/s
    int m_lastVolume;    // 静音前的音量值

};
#endif // WIDGET_H
