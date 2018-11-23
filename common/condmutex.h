#ifndef STUN_COND_AND_MUTEX
#define STUN_COND_AND_MUTEX

#include <functional>

class ConditionAndMutex
{
    pthread_mutex_t _mutex;
    pthread_cond_t _cond;

public:
    ConditionAndMutex();
    ~ConditionAndMutex();
    void Wait(std::function<bool()> func);
    void ExecuteAndSignal(std::function<void()> func);
};

#endif

