#ifndef TEST_H
#define TEST_H

#include "avclock.h"
#include "threadpool.h"
#include "VFrame.h"
#include "jtplayer.h"
#include "jtdemux.h"
#include "jtdecoder.h"

#include <thread>
#include <chrono>
#include <iostream>


extern void testAVClock();

extern void test(std::shared_ptr<void>(param));

extern void testThreadPool();

extern void testDemux(QString url);

extern void testOpenFile(QString url);

extern void testPlayer(QString url);
#endif
