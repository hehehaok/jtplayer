#include "test.h"

void testAVClock() {
    AVClock clk;
    clk.setClock(0.00);
    clk.getClock();
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    clk.getClock();
}

void test(std::shared_ptr<void>(param)) {
    int* num = reinterpret_cast<int*>(param.get());
    std::cout << "thread " << std::this_thread::get_id() << " cout num" << *num << std::endl;
}

void testThreadPool() {
    ThreadPool::init();
    for (int i = 0; i < 100; i++) {
        ThreadPool::addTask(test, std::make_shared<int>(i));
    }
    ThreadPool::releasePool();
}

void testOpenFile(QString url) {
    jtPlayer::get()->processInput(url);
    jtPlayer::get()->close();
}

void testDemux(QString url)
{
    jtPlayer::get()->processInput(url);
    JTDemux demux;
    demux.demux(std::shared_ptr<void>(nullptr));
    demux.exit();
}

void testPlayer(QString url) {
    jtPlayer::get()->processInput(url);
    jtPlayer::get()->play();
    int i = 2;
    while (i--) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    jtPlayer::get()->m_jtdemux->m_exit = true;
    jtPlayer::get()->m_jtdecoder->m_exit = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    jtPlayer::get()->close();
}
