#include "signal_listener.h"
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <log/log.h>
#include <string.h>

#define DEFAULT_SIGNAL_WAIT_TIMEOUT_SEC  10

SignalListener::SignalListener() :
    on_monitor_(false),
    pid_(-1),
    condition_(false)
{
    memset(&option_, 0, sizeof(sig_option_t));
    pthread_cond_init(&cond_, NULL);
    pthread_mutex_init(&mutex_, NULL);
}

SignalListener::~SignalListener()
{
    pthread_cond_destroy(&cond_);
    pthread_mutex_destroy(&mutex_);

    setTimeout(0, 100000);
    quit();
}

void SignalListener::initialize(int signum)
{
    sigemptyset(&option_.set);
    sigaddset(&option_.set, signum);
    sigprocmask(SIG_SETMASK, &option_.set, NULL);
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
    int ret = 0;
    struct timeval now;
    struct timespec timeout_now;

    pthread_mutex_lock(&mutex_);
    gettimeofday(&now, NULL);
    timeout_now.tv_sec = now.tv_sec + option_.timeout.tv_sec;
    timeout_now.tv_nsec = now.tv_usec * 1000 + option_.timeout.tv_nsec;
    while (condition_ == false)
    {
        ret = pthread_cond_timedwait(&cond_, &mutex_, &timeout_now);
    }
    condition_ = false;
    pthread_mutex_unlock(&mutex_);
    if (ret != 0)
    {
        CMP_DEBUG_PRINT("signal wait timeout or fail");
    }
    else
    {
        CMP_DEBUG_PRINT("signal received");
    }
}

void SignalListener::listen()
{
    pid_ = syscall(__NR_gettid);
    while (on_monitor_)
    {
        if (-1 != sigtimedwait(&option_.set, NULL, &option_.timeout))
        {
            pthread_mutex_lock(&mutex_);
            if (condition_ == false)
            {
                condition_ = true;
                pthread_cond_signal(&cond_);
            }
            pthread_mutex_unlock(&mutex_);
        }
    }
}