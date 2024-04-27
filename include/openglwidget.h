#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QTimer>
#include <memory>

class YUV420Frame;

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture)

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

signals:
    void mouseClicked();
    void mouseDoubleClicked();

public Q_SLOTS:
    void onShowYUV(std::shared_ptr<YUV420Frame> frame);

public:
    void initializeGL() override;
    void paintGL() override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QOpenGLShaderProgram *program;
    QOpenGLBuffer vbo;

    // opengl中y、u、v分量位置
    GLuint posUniformY;
    GLuint posUniformU;
    GLuint posUniformV;

    // 纹理
    QOpenGLTexture *textureY;
    QOpenGLTexture *textureU;
    QOpenGLTexture *textureV;

    // 纹理ID，创建错误返回0
    GLuint m_idY;
    GLuint m_idU;
    GLuint m_idV;

    // 原始数据
    std::shared_ptr<YUV420Frame> m_frame;

    bool m_isDoubleClick;

    QTimer m_timer;
};

#endif // OPENGLWIDGET_H
