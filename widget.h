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

private slots:
    void onPauseBtnClicked();
    void updatePauseBtnStyle(PlayerState playerState);
    void onStopBtnClicked();
    void totalTimeChanged(int64_t duration);
    void curTimeChanged(int64_t curPts);
    void muteChanged(bool checked);

signals:
    void volumeChanged(int volume);


private:
    Ui::Widget *ui;
    int m_lastVolume;

};
#endif // WIDGET_H
