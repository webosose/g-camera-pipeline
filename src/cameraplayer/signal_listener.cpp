#include "signal_listener.h"
#include <sys/types.h>
#include <sys/syscall.h>
#include <log/log.h>
#include <string.h>

#define DEFAULT_SIGNAL_WAIT_TIMEOUT_SEC  10

SignalListener::SignalListener() :
    on_monitor_(false),
    pid_(-1),
    mutex_{},
    cond_{},
    condition_(false)
{
    memset(&option_, 0, sizeof(sig_option_t));
}

SignalListener::~SignalListener()
{
    setTimeout(0, 100000);
    quit();
}

void SignalListener::initialize(int signum)
{
    sigemptyset(&option_.set);
    sigaddset(&option_.set, signum);
    if (sigprocmask(SIG_SETMASK, &option_.set, NULL) < 0)
    {
        CMP_LOG_ERROR("sigprocmask error");
    }
    option_.timeout.tv_sec = DEFAULT_SIGNAL_WAIT_TIMEOUT_SEC;
    option_.timeout.tv_nsec = 0;
    on_monitor_ = false;
}

void SignalListener::setTimeout(int seconds, int nano_seconds)
{
    option_.timeout.tv_sec = seconds;
    option_.timeout.tv_nsec = nano_seconds;
}

int SignalListener::run()
{
    on_monitor_ = true;
    listen_thread_ = std::thread{[this]() { this->listen(); }};
    usleep(100); // wait for the thread to catch pid
    return pid_;
}

void SignalListener::quit()
{
    on_monitor_ = false;
    if (listen_thread_.joinable())
    {
        listen_thread_.join();
    }
}

void SignalListener::wait()
{
    std::chrono::seconds timeout(option_.timeout.tv_sec);
    std::unique_lock<std::mutex> mlock(mutex_);
    if (false == cond_.wait_for(mlock, timeout, [&]{ return condition_; }))
    {
        CMP_LOG_INFO("signal wait timeout reached");
    }
    else
    {
        CMP_LOG_DEBUG("signal received");
    }
    condition_ = false;
}

void SignalListener::listen()
{
    pid_ = syscall(__NR_gettid);
    while (on_monitor_)
    {
        if (-1 != sigtimedwait(&option_.set, NULL, &option_.timeout))
        {
            std::lock_guard<std::mutex> guard(mutex_);
            cond_.notify_one();
            condition_ = true;
        }
    }
}