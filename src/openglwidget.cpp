#include "openglwidget.h"
#include "vframe.h"
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QDebug>

#define VERTEXIN 0
#define TEXTUREIN 1

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent), m_isDoubleClicked(false)
{
    connect(&m_timer, &QTimer::timeout, [this]()
            {
            if (!this->m_isDoubleClicked)
                emit this->mouseClicked();
            this->m_isDoubleClicked = false;
            this->m_timer.stop(); });
    m_timer.setInterval(400);
}

OpenGLWidget::~OpenGLWidget()
{
    // 销毁OpenGL资源
    makeCurrent();
    vbo.destroy();
    textureU->destroy();
    textureV->destroy();
    textureY->destroy();
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    static const GLfloat vertices[]{
        // 顶点坐标
        -1.0f,
        -1.0f,
        -1.0f,
        +1.0f,
        +1.0f,
        +1.0f,
        +1.0f,
        -1.0f,
        // 纹理坐标
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
    };

    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    program = new QOpenGLShaderProgram(this);

    program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/yuv.vert");
    program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/yuv.frag");

    // 绑定输入的顶点坐标和纹理坐标属性
    program->bindAttributeLocation("vertexIn", VERTEXIN);
    program->bindAttributeLocation("textureIn", TEXTUREIN);

    // Link shader pipeline
    if (!program->link())
    {
        qDebug() << "program->link error";
        close();
    }

    // Bind shader pipeline for use
    if (!program->bind())
    {
        qDebug() << "program->bind error";
        close();
    }

    // 启用并且设置定点位置和纹理坐标
    program->enableAttributeArray(VERTEXIN);
    program->enableAttributeArray(TEXTUREIN);
    program->setAttributeBuffer(VERTEXIN, GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
    program->setAttributeBuffer(TEXTUREIN, GL_FLOAT, 8 * sizeof(GLfloat), 2, 2 * sizeof(GLfloat));

    // 定位shader中 uniform变量
    posUniformY = program->uniformLocation("tex_y");
    posUniformU = program->uniformLocation("tex_u");
    posUniformV = program->uniformLocation("tex_v");

    // 创建纹理并且获取id
    textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureY->create();
    textureU->create();
    textureV->create();
    m_idY = textureY->textureId();
    m_idU = textureU->textureId();
    m_idV = textureV->textureId();

    glClearColor(0.0, 0.0, 0.0, 0.0);
}

void OpenGLWidget::paintGL()
{
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_2D, m_idY);

    if (!m_frame)
        return;
    uint32_t videoW = m_frame->getPixelW();
    uint32_t videoH = m_frame->getPixelH();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW, videoH, 0, GL_RED, GL_UNSIGNED_BYTE, m_frame->getBufferY()); // m_yPtr 大小是(videoW*videoH)

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 激活纹理单元GL_TEXTURE1
    glActiveTexture(GL_TEXTURE1);
    // 绑定u分量纹理对象id到激活的纹理单元
    glBindTexture(GL_TEXTURE_2D, m_idU);
    // 使用内存中的数据创建真正的u分量纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW >> 1, videoH >> 1, 0, GL_RED, GL_UNSIGNED_BYTE, m_frame->getBufferU()); // m_uPtr 大小是(videoW*videoH / 2)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 激活纹理单元GL_TEXTURE2
    glActiveTexture(GL_TEXTURE2);
    // 绑定v分量纹理对象id到激活的纹理单元
    glBindTexture(GL_TEXTURE_2D, m_idV);
    // 使用内存中的数据创建真正的v分量纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW >> 1, videoH >> 1, 0, GL_RED, GL_UNSIGNED_BYTE, m_frame->getBufferV()); // m_vPtr 大小是(videoW*videoH / 2)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // 指定y纹理要使用新值
    glUniform1i(posUniformY, 0);
    // 指定u纹理要使用新值
    glUniform1i(posUniformU, 1);
    // 指定v纹理要使用新值
    glUniform1i(posUniformV, 2);
    // 使用顶点数组方式绘制图形
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!m_timer.isActive()) {
        m_timer.start();
    }
}

void OpenGLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_isDoubleClicked = true;
    emit mouseDoubleClicked();
}

void OpenGLWidget::onShowYUV(std::shared_ptr<YUV420Frame> frame)
{
    if (!frame)
    {
        qDebug() << "show YUV failed, null frame!\n";
        return;
    }
    m_frame = frame; // 直接赋值给 m_frame，旧的 m_frame 会在此时自动释放
//    qDebug() << "receive success!!!! " << m_frame->getBufferU() << m_frame->getBufferV() << m_frame->getBufferY();
    update();
}

void OpenGLWidget::clearWidget()
{
    makeCurrent();
    m_frame = NULL;
    glClear(GL_COLOR_BUFFER_BIT);
    doneCurrent();
    update();
}








