#ifndef TEST_POLLING_H
#define TEST_POLLING_H

struct PipePair
{
    int readpipe;
    int writepipe;
    bool fDataPending;
};


class CTestPolling : public IUnitTest
{
    
public:
    CRefCountedPtr<IPolling> _spPolling;
    std::vector<PipePair> _pipes;
    
    uint32_t _polltype;
    
    void TestUnInit();
    
    HRESULT TestInit(size_t sizePolling, size_t sizePipeArray); // creates pipes and polling mechanism;
    
    HRESULT CreateAndAddPipe();
    HRESULT WritePipe(PipePair* pPipePair);
    HRESULT SetNonBlocking(int fd);
    HRESULT ConsumeEvent(int* pFD, int* pCount); // returns file descriptor of what was last handled
    int FindPipePairIndex(int fd);
    size_t GetPendingCount();
    HRESULT RemovePipe(int pipeindex);
    
    HRESULT Test1();
    HRESULT Test2();
    
    HRESULT Run();
    
    CTestPolling();
    ~CTestPolling();
    
    UT_DECLARE_TEST_NAME("CTestPolling");
    
};


#endif
