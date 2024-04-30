#include "widget.h"
#include "./ui_widget.h"
#include "openglwidget.h"
#include "jtplayer.h"
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
{
    ui->setupUi(this);
    init();
}

Widget::~Widget()
{
    delete ui;
}

void Widget::init()
{
    qRegisterMetaType<std::shared_ptr<YUV420Frame>>("std::shared_ptr<YUV420Frame>");
//    QString url = "../clock.avi";
    QString url = "../4.mp4";
    jtPlayer::get()->processInput(url);
    connect(jtPlayer::get()->m_jtoutput.get(), JTOutput::frameChanged,
            ui->opengl_widget, OpenGLWidget::onShowYUV, Qt::QueuedConnection);
    jtPlayer::get()->play();
}

void Widget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setBrush(QBrush(QColor(46, 46, 54)));
    painter.drawRect(rect());
}
