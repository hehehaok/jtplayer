#ifndef VFRAME_H
#define VFRAME_H

#include <memory>
#include <stdlib.h>
#include <QDebug>

class YUV420Frame {
public:
    YUV420Frame(uint8_t* buffer, uint32_t pixelW, uint32_t pixelH) : m_buffer(nullptr) {
        create(buffer, pixelW, pixelH);
    }
    ~YUV420Frame() {
        if (m_buffer) {
            free(m_buffer);
            m_buffer = nullptr;
        }
    }
    // 获取缓冲区Y
    inline uint8_t* getBufferY() const {
        return m_buffer;
    }
    // 获取缓冲区U
    inline uint8_t* getBufferU() const {
        return m_buffer + m_pixelH * m_pixelW;
    }
    // 获取缓冲区V
    inline uint8_t* getBufferV() const {
        return m_buffer + m_pixelH * m_pixelW * 5 / 4;
    }
    // 获取像素宽
    inline uint32_t getPixelW() const {
        return m_pixelW;
    }
    // 获取像素高
    inline uint32_t getPixelH() const {
        return m_pixelH;
    }

private:
    void create(uint8_t* buffer, uint32_t pixelW, uint32_t pixelH) {
        m_pixelW = pixelW;
        m_pixelH = pixelH;
        int sizeY = pixelW * pixelH;
        int sizeUV = sizeY >> 2;
        if (buffer == nullptr) {
            m_buffer = (uint8_t*)malloc(sizeY + sizeUV * 2);
        }
        if (m_buffer) {
            memcpy(m_buffer, buffer, sizeY);                                     // 复制Y分量
            memcpy(m_buffer + sizeY, buffer + sizeY, sizeUV);                    // 复制U分量
            memcpy(m_buffer + sizeY + sizeUV, buffer + sizeY + sizeUV, sizeUV);  // 复制V分量
        }
        else {
            qDebug() << "malloc buffer failed!\n";
        }
    }

    uint32_t m_pixelW;  // 像素宽度
    uint32_t m_pixelH;  // 像素高度
    uint8_t* m_buffer;  // 缓冲区入口
};

#endif // VFRAME_H
