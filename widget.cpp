#include "widget.h"
#include "./ui_widget.h"
#include "openglwidget.h"
#include "jtoutput.h"

#include <QMetaMethod>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>

Q_DECLARE_METATYPE(std::shared_ptr<YUV420Frame>)
Q_DECLARE_METATYPE(int64_t)
// 由于信号与槽只支持int double等基本数据类型，因此如果要
// 在信号与槽中传递其他类型的变量时需要先声明并注册

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , m_duration(0)
    , m_lastVolume(0)
{
    ui->setupUi(this);

    // 设置一些界面的初始属性
    QString url = "../Forrest_Gump_IMAX.mp4";
    ui->fileName->setText(url);
    QStringList speeds;
    speeds << "0.5x";
    speeds << "1.0x";
    speeds << "1.5x";
    speeds << "2.0x";
    ui->speedSelect->addItems(speeds);
    ui->speedSelect->setCurrentIndex(1);
    ui->volumeSlider->setValue(30);
    JTPlayer::get()->m_jtOutput->m_volume = ui->volumeSlider->value();
    init(); // 将信号与槽连接好
}

Widget::~Widget()
{
    delete ui;
}

void Widget::init()
{
    setWidgetsState(JTPlayer::get()->getPlayerState());
    qRegisterMetaType<std::shared_ptr<YUV420Frame>>("std::shared_ptr<YUV420Frame>");
    qRegisterMetaType<int64_t>("int64_t");
    connect(ui->openFileBtn, &QPushButton::clicked, this, &Widget::onOpenFileBtnClicked);
    connect(ui->pauseBtn, &QPushButton::clicked, this, &Widget::onPauseBtnClicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &Widget::onStopBtnClicked);
    connect(JTPlayer::get()->m_jtOutput.get(), &JTOutput::frameChanged,
            ui->opengl_widget, &OpenGLWidget::onShowYUV, Qt::QueuedConnection);
    connect(JTPlayer::get(), &JTPlayer::durationChanged, this, &Widget::totalTimeChanged);
    connect(JTPlayer::get()->m_jtOutput.get(), &JTOutput::ptsChanged, this, &Widget::sliderTimeChanged);
    connect(JTPlayer::get(), &JTPlayer::playerStateChanged, JTPlayer::get(), &JTPlayer::setPlayerState);
    connect(JTPlayer::get(), &JTPlayer::playerStateChanged, this, &Widget::updatePauseBtnStyle);
    connect(ui->muteBtn, &QRadioButton::toggled, this, &Widget::muteChanged);
    connect(ui->volumeSlider, &PtsSlider::sliderMoved, this, &Widget::volumeSliderMoved);
    connect(this, &Widget::volumeChanged, JTPlayer::get(), &JTPlayer::setVolume);
    connect(ui->isStepBtn, &QRadioButton::toggled, this, &Widget::setStep);
    connect(ui->stepBtn, &QPushButton::clicked, this, &Widget::onStepBtnClicked);
    connect(ui->backwardBtn, &QPushButton::clicked, this, &Widget::onBackwardBtnClicked);
    connect(ui->forwardBtn, &QPushButton::clicked, this, &Widget::onForwardBtnClicked);
    connect(ui->ptsSlider, &PtsSlider::sliderMoved, this, &Widget::ptsSliderMoved);
    connect(ui->ptsSlider, &PtsSlider::sliderReleased, this, &Widget::ptsSliderReleased);
    connect(JTPlayer::get(), &JTPlayer::playerAtFileEnd, this, &Widget::onStopBtnClicked);
    connect(ui->speedSelect, &QComboBox::currentTextChanged, this, &Widget::speedChanged);
}

void Widget::setWidgetsState(PlayerState playerState)
{
    switch (playerState)
    {
    case PlayerState::STOPED:
        ui->backwardBtn->setEnabled(false);
        ui->forwardBtn->setEnabled(false);
        ui->stopBtn->setEnabled(false);
        ui->stepBtn->setEnabled(false);
        ui->isStepBtn->setChecked(false);
        ui->muteBtn->setChecked(false);
        ui->ptsSlider->setEnabled(false);
        ui->ptsSlider->setValue(0);
        ui->totalTime->setText(QString("00:00:00"));
        ui->curTime->setText(QString("00:00:00"));
        ui->speedSelect->setCurrentIndex(1);
        break;
    case PlayerState::PLAYING:
    case PlayerState::PAUSED:
        ui->backwardBtn->setEnabled(true);
        ui->forwardBtn->setEnabled(true);
        ui->stopBtn->setEnabled(true);
        ui->ptsSlider->setEnabled(true);
    default:
        break;
    }
}

void Widget::onOpenFileBtnClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "open file", "E:\\Program Files (x86)\\game\\reLive\\VALORANT\\");
    if(filePath.isEmpty())
     {
         return;
     }
     else
     {
         ui->fileName->setText(filePath);
     }
}

void Widget::onPauseBtnClicked()
{
    switch (JTPlayer::get()->getPlayerState())
    {
    case PlayerState::STOPED:
        JTPlayer::get()->processInput(ui->fileName->text());
        JTPlayer::get()->play();
        setWidgetsState(JTPlayer::get()->getPlayerState());
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
        JTPlayer::get()->close();
        ui->opengl_widget->clearWidget();
        setWidgetsState(JTPlayer::get()->getPlayerState());
        break;
    default:
        break;
    }
}

void Widget::totalTimeChanged(int64_t duration)
{
    m_duration = duration;
    int hours, mins, secs;
    mins = duration / 60;
    secs = duration % 60;
    hours = mins / 60;
    mins %= 60;
    ui->totalTime->setText(QString("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0')).
                           arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0')));
}

void Widget::sliderTimeChanged(int64_t curPts)
{
    if (ui->ptsSlider->getMousePressed())
        return;
    int hours, mins, secs;
    mins = curPts / 60;
    secs = curPts % 60;
    hours = mins / 60;
    mins %= 60;
    ui->curTime->setText(QString("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0')).
                           arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0')));
    ui->ptsSlider->setSliderPos(double(curPts) / m_duration);
}


void Widget::muteChanged(bool checked)
{
    if(checked) {
        m_lastVolume = JTPlayer::get()->m_jtOutput->m_volume;
        ui->volumeSlider->setValue(0);
        emit volumeChanged(0);
    }
    else {
        ui->volumeSlider->setValue(m_lastVolume);
        emit volumeChanged(m_lastVolume);
    }
}

void Widget::volumeSliderMoved()
{
    int curVolume = ui->volumeSlider->value();
    emit volumeChanged(curVolume);
}

void Widget::setStep(bool checked)
{
    ui->stepBtn->setEnabled(checked);
    if (checked) {
        JTPlayer::get()->pause(true);
    }
    else {
        JTPlayer::get()->pause(false);
    }
}

void Widget::onStepBtnClicked()
{
    JTPlayer::get()->step(true);
}

void Widget::onForwardBtnClicked()
{
    double changeTime = 5;
    double curTime = m_duration * ui->ptsSlider->getPercent() + changeTime;
    if (curTime > m_duration) {
        curTime = m_duration;
    }
    JTPlayer::get()->seekTo(curTime);
}

void Widget::onBackwardBtnClicked()
{
    double changeTime = -5;
    double curTime = m_duration * ui->ptsSlider->getPercent() + changeTime;
    if (curTime < 0) {
        curTime = 0;
    }
    JTPlayer::get()->seekTo(curTime);
}

void Widget::ptsSliderMoved()
{
    int64_t curTime = m_duration * ui->ptsSlider->getCursorPercent();
    int hours, mins, secs;
    mins = curTime / 60;
    secs = curTime % 60;
    hours = mins / 60;
    mins %= 60;
    QString curTimeInHMS = QString("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0')).
            arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'));
    if (ui->ptsSlider->getMousePressed()) {
        ui->curTime->setText(curTimeInHMS);
    }
    else {
        ui->ptsSlider->setToolTip(curTimeInHMS);
    }
}

void Widget::ptsSliderReleased()
{
    double curTime = m_duration * ui->ptsSlider->getPercent();
    JTPlayer::get()->seekTo(curTime);
}

void Widget::speedChanged(const QString& curText)
{
    QString temp = const_cast<QString &>(curText);
    float speed = temp.remove("x").toFloat();
    JTPlayer::get()->setSpeed(speed);
}







