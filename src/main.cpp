#include <iostream>
#include <thread>

#include "MsgChannel.hpp"
#include "Encoder.hpp"
#include "RTSP.hpp"
#include "Logger.hpp"
#include "IMP.hpp"
#include "Config.hpp"
#include "Motion.hpp"

#include "version.hpp"

template <class T> void start_component(T c) {
    c.run();
}

Encoder enc;
Encoder jpg;
Encoder sub;
Motion motion;
RTSP rtsp;

bool timesync_wait() {
    // I don't really have a better way to do this than
    // a no-earlier-than time. The most common sync failure
    // is time() == 0
    int timeout = 0;
    while (time(NULL) < 1647489843) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++timeout;
        if (timeout == 60)
            return false;
    }
    return true;
}

int main(int argc, const char *argv[]) {

    LOG_INFO("PRUDYNT Video Daemon: " << VERSION);

    std::thread enc_thread;
    std::thread sub_thread;
    std::thread rtsp_thread;
    std::thread motion_thread;

    if (Logger::init()) {
        LOG_ERROR("Logger initialization failed.");
        return 1;
    }
    LOG_INFO("Starting Prudynt Video Server.");

    if (!timesync_wait()) {
        LOG_ERROR("Time is not synchronized.");
        return 1;
    }
    if (IMP::init()) {
        LOG_ERROR("IMP initialization failed.");
        return 1;
    }
    if (Config::singleton()->motionEnable) {
        if (motion.init()) {
        std::cout << "Motion initialization failed." << std::endl;
        return 1;
        }
    }
    if (enc.init()) {
        LOG_ERROR("Encoder initialization failed.");
        return 1;
    }


    enc_thread = std::thread(start_component<Encoder>, enc);
    rtsp_thread = std::thread(start_component<RTSP>, rtsp);

    if (Config::singleton()->motionEnable) {
        LOG_DEBUG("Motion detection enabled");
        motion_thread = std::thread(start_component<Motion>, motion);
    }

    if (Config::singleton()->stream1jpegEnable) {
        LOG_DEBUG("JPEG support enabled");
        std::thread jpegThread(&Encoder::jpeg_snap, &jpg);
        jpegThread.detach();
    }

    if (Config::singleton()->substream0enable) {
        if (sub.init_substream()) {
            LOG_ERROR("Substream Encoder initialization failed.");
            return 1;
        }
        LOG_DEBUG("Substream enabled");
        sub_thread = std::thread(&Encoder::run_substream, &sub);
    }

    enc_thread.join();
    sub_thread.join();
    rtsp_thread.join();
    motion_thread.join();

    return 0;
}
