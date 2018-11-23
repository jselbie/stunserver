#include "commonincludes.hpp"
#include "condmutex.h"
#include <atomic>
#include <iostream>
#include <functional>
#include "stuncore.h"
#include "oshelper.h"

class Benchmark
{
    pthread_mutex_t _mutexReady;
    ConditionAndMutex _cmReady;
    ConditionAndMutex _cmGo;
    ConditionAndMutex _cmCompleted;

    bool _shouldRun;
    size_t _readyCount;
    size_t _completedCount;

    std::atomic<size_t> _counter;
    uint32_t _startTimeMS;
    uint32_t _endTimeMS;
    size_t _iterations;
    size_t _threads;

public:
    Benchmark(size_t iterations, size_t threads);
    void Run();
    void* ThreadFunc();
    static void* s_ThreadFunction(void* pThis);
};

Benchmark::Benchmark(size_t iterations, size_t threads):
_shouldRun(false),
_readyCount(threads),
_completedCount(threads),
_startTimeMS(0),
_endTimeMS(0),
_iterations(iterations),
_threads(threads)
{

}

void* Benchmark::s_ThreadFunction(void* pThis)
{
    Benchmark* pBM = reinterpret_cast<Benchmark*>(pThis);
    return pBM->ThreadFunc();
}

void* Benchmark::ThreadFunc()
{
    // simulate a binding request coming in
    TransportAddressSet tsa;
    tsa.set[0].fValid = true;

    CStunMessageBuilder builder;
    builder.AddHeader(StunMsgTypeBinding, StunMsgClassRequest);
    builder.AddRandomTransactionId(nullptr);
    CRefCountedBuffer buffer;
    builder.GetResult(&buffer);

    StunMessageOut msgOut;
    auto spBufferOut = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    auto spBufferReader = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    msgOut.spBufferOut = spBufferOut;
    size_t stoppoint = _iterations;

    CStunMessageReader reader;

    _cmReady.ExecuteAndSignal([this](){_readyCount--;});

    _cmGo.Wait([this]() {return _shouldRun;});

    while (_counter.load() < _iterations)
    {
        reader.Reset();
        spBufferReader->SetSize(0);
        reader.GetStream().Attach(spBufferReader, true);
        reader.AddBytes(buffer->GetData(), buffer->GetSize());

        if (reader.GetState() != CStunMessageReader::BodyValidated)
        {
            Logging::LogMsg(LL_ALWAYS, "Error in CStunMessageReader::GetState");
            break;
        }

        StunMessageIn msgIn;
        msgIn.fConnectionOriented = false;
        msgIn.pReader = &reader;
        msgIn.socketrole = RolePP;


        HRESULT hr = CStunRequestHandler::ProcessRequest(msgIn, msgOut, &tsa, nullptr);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Error in ProcessRequest (hr == %x)", hr);
            break;
        }
        size_t prev = _counter.fetch_add(1);
        if (prev == (stoppoint - 1))
        {
            _endTimeMS = GetMillisecondCounter();
        }
    }

    _cmCompleted.ExecuteAndSignal([this](){_completedCount--;});

    return nullptr;
}

void Benchmark::Run()
{
    _readyCount = _threads;
    _completedCount = _threads;
    _shouldRun = false;
    _counter.store(0);
    _endTimeMS = 0;
    _startTimeMS = 0;


   std::vector<pthread_t> threadList;

    for (size_t t = 0; t < _threads; t++)
    {
        pthread_t pt = 0;
        pthread_create(&pt, nullptr, s_ThreadFunction, this);
        threadList.push_back(pt);
    }    


    _cmReady.Wait([this]() {return (_readyCount <= 0);});

    _endTimeMS = GetMillisecondCounter();
    _startTimeMS = _endTimeMS;

    _cmGo.ExecuteAndSignal([this](){_shouldRun = true;});
    _cmCompleted.Wait([this]() {return _completedCount <= 0;});

    for (size_t t = 0; t < _threads; t++)
    {
        pthread_join(threadList[t], nullptr);
    }

    std::cout << "Start Time: " << _startTimeMS << std::endl;
    std::cout << "End Time: " << _endTimeMS << std::endl;
    std::cout << "Final Count: " << _counter.load() << std::endl;
    std::cout << "Result: " << (_endTimeMS - _startTimeMS) << std::endl;
}


void benchmark_test(int threads)
{
    if (threads <= 0)
    {
        threads = 1;
    }
    std::cout << "Running benchmark witih " << threads << " threads." << std::endl;

    auto iterations = 100000000;
    Benchmark bm(iterations, threads);
    bm.Run();
}
