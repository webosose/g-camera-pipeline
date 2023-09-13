#ifndef SIGNAL_LISTENER_H_
#define SIGNAL_LISTENER_H_

#include <signal.h>
#include <condition_variable>
#include <thread>

class SignalListener
{
public:
    SignalListener();
    ~SignalListener();
    void initialize(int signum);
    void setTimeout(int seconds, int nano_seconds);
    int run();
    void quit();
    void wait();
private:
    struct sig_option_t
    {
        sigset_t set;
        struct timespec timeout;
    };
    bool on_monitor_;
    int pid_;
    sig_option_t option_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::thread listen_thread_;
    void listen();
};

#endif /* SIGNAL_LISTENER_H_ */