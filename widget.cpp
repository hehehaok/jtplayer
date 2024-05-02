#include "widget.h"
#include "./ui_widget.h"
#include "openglwidget.h"
#include "jtoutput.h"

#include <QMetaMethod>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QPainter>

Q_DECLARE_METATYPE(std::shared_ptr<YUV420Frame>)

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , m_lastVolume(0)
{
    ui->setupUi(this);

    // 设置一些界面的初始属性
    QStringList urls;
    urls << "../Forrest_Gump_IMAX.mp4";
    urls << "E:/Program Files (x86)/game/reLive/VALORANT/VALORANT_replay_2024.04.12-23.30.mp4";
    ui->fileSelect->addItems(urls);
    ui->fileSelect->setCurrentIndex(0);
    QStringList speeds;
    speeds << "1.0x";
    speeds << "1.5x";
    speeds << "2.0x";
    ui->speedSelect->addItems(speeds);
    ui->speedSelect->setCurrentIndex(0);
    init(); // 将信号与槽连接好
}

Widget::~Widget()
{
    delete ui;
}

void Widget::init()
{
    ui->backwardBtn->setEnabled(false);
    ui->forwardBtn->setEnabled(false);
    ui->stopBtn->setEnabled(false);
    ui->muteBtn->setCheckable(true);
    qRegisterMetaType<std::shared_ptr<YUV420Frame>>("std::shared_ptr<YUV420Frame>");
    connect(ui->pauseBtn, &QPushButton::clicked, this, &Widget::onPauseBtnClicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &Widget::onStopBtnClicked);
    connect(JTPlayer::get()->m_jtOutput.get(), &JTOutput::frameChanged,
            ui->opengl_widget, &OpenGLWidget::onShowYUV, Qt::QueuedConnection);
    connect(ui->stopBtn, &QPushButton::clicked, ui->opengl_widget, &OpenGLWidget::clearWidget);
    connect(JTPlayer::get(), &JTPlayer::durationChanged, this, &Widget::totalTimeChanged);
    connect(JTPlayer::get()->m_jtOutput.get(), &JTOutput::AVPtsChanged,
            this, &Widget::curTimeChanged);
    connect(JTPlayer::get(), &JTPlayer::playerStateChanged, JTPlayer::get(), &JTPlayer::setPlayerState);
    connect(JTPlayer::get(), &JTPlayer::playerStateChanged, this, &Widget::updatePauseBtnStyle);
    connect(ui->muteBtn, &QPushButton::toggled, this, &Widget::muteChanged);
    connect(this, &Widget::volumeChanged, JTPlayer::get(), &JTPlayer::setVolume);
}

void Widget::onPauseBtnClicked()
{
    switch (JTPlayer::get()->getPlayerState())
    {
    case PlayerState::STOPED:
        ui->backwardBtn->setEnabled(true);
        ui->forwardBtn->setEnabled(true);
        ui->stopBtn->setEnabled(true);
        JTPlayer::get()->processInput(ui->fileSelect->currentText());
        JTPlayer::get()->play();
        break;
    case PlayerState::PAUSED:
        JTPlayer::get()->pause(false);
        break;
    case PlayerState::PLAYING:
        JTPlayer::get()->pause(true);
        break;
    default:
        break;
    }
}

void Widget::updatePauseBtnStyle(PlayerState playerState)
{
    switch (playerState) {
    case PlayerState::STOPED:
    case PlayerState::PAUSED:
        ui->pauseBtn->setText(QString("play"));
        break;
    case PlayerState::PLAYING:
        ui->pauseBtn->setText(QString("pause"));
    default:
        break;
    }
}

void Widget::onStopBtnClicked()
{
    switch (JTPlayer::get()->getPlayerState())
    {
    case PlayerState::PAUSED:
    case PlayerState::PLAYING:
        ui->backwardBtn->setEnabled(false);
        ui->forwardBtn->setEnabled(false);
        ui->stopBtn->setEnabled(false);
        ui->totalTime->setText(QString("00:00:00"));
        ui->curTime->setText(QString("00:00:00"));
        JTPlayer::get()->close();
        break;
    default:
        break;
    }
}

void Widget::totalTimeChanged(int64_t duration)
{
    int hours, mins, secs;
    mins = duration / 60;
    secs = duration % 60;
    hours = mins / 60;
    mins %= 60;
    ui->totalTime->setText(QString("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0')).
                           arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0')));
}

void Widget::curTimeChanged(int64_t curPts)
{
    int hours, mins, secs;
    mins = curPts / 60;
    secs = curPts % 60;
    hours = mins / 60;
    mins %= 60;
    ui->curTime->setText(QString("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0')).
                           arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0')));

}

void Widget::muteChanged(bool checked)
{
    if(checked) {
        m_lastVolume = JTPlayer::get()->m_jtOutput->m_volume;
        emit volumeChanged(0);
    }
    else {
        emit volumeChanged(m_lastVolume);
    }
}







