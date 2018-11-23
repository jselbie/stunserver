#include "commonincludes.hpp"
#include "condmutex.h"


ConditionAndMutex::ConditionAndMutex()
{
    pthread_cond_init(&_cond, nullptr);
    pthread_mutex_init(&_mutex, nullptr);
}

ConditionAndMutex::~ConditionAndMutex()
{
    pthread_cond_destroy(&_cond);
    pthread_mutex_destroy(&_mutex);
}


void ConditionAndMutex::Wait(std::function<bool()> func)
{
    pthread_mutex_lock(&_mutex);

    bool testcondition = func();
    while (testcondition == false)
    {
        pthread_cond_wait(&_cond, &_mutex);
        testcondition = func();
    }
    pthread_mutex_unlock(&_mutex);
}

void ConditionAndMutex::ExecuteAndSignal(std::function<void()> func)
{
    pthread_mutex_lock(&_mutex);
    func();
    pthread_mutex_unlock(&_mutex);
    pthread_cond_broadcast(&_cond);
}

