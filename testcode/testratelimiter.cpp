#include "commonincludes.hpp"
#include "unittest.h"
#include "ratelimiter.h"
#include "testratelimiter.h"


class RateLimiterMockTime : public RateLimiter
{
public:
    time_t _time;
    
    RateLimiterMockTime(size_t tablesize) : RateLimiter(tablesize, false), _time(0)
    {
    }
    
    void set_time(time_t t)
    {
        _time = t;
    }
    
    virtual time_t get_time()
    {
        return _time;
    }
    
};

HRESULT CTestRateLimiter::Run()
{
    HRESULT hr = S_OK;
    
    hr = Test1(); 
    if (SUCCEEDED(hr))
    {
        hr = Test2();
    }
    return hr;
}



HRESULT CTestRateLimiter::Test1()
{
    // simulate 60 packets within one minute
    CSocketAddress sockaddr(0x12341234,1234);
    CSocketAddress sockaddr_goodguy(0x67896789,6789);
    RateLimiterMockTime ratelimiter(20000);
    HRESULT hr = S_OK;
    bool result = false;
    
    for (int x = 0; x < 59; x++)
    {
        ratelimiter.set_time(x / 2);
        result = ratelimiter.RateCheck(sockaddr);
        ChkIf(result == false, E_FAIL);
        
        if ((x % 4) == 0)
        {
            result = ratelimiter.RateCheck(sockaddr_goodguy);
            ChkIf(result == false, E_FAIL);
        }
    }
    
    // 60th packet should fail for bad guy
    result  = ratelimiter.RateCheck(sockaddr);
    ChkIf(result, E_FAIL);
    
    
    // should not impact good guy
    result  = ratelimiter.RateCheck(sockaddr_goodguy);
    ChkIf(result == false, E_FAIL);
    
    // at the one hour mark, he should still be in the penalty box
    ratelimiter.set_time(RateLimiter::PENALTY_TIME_SECONDS);
    result  = ratelimiter.RateCheck(sockaddr);
    ChkIf(result, E_FAIL);
    
    // but at the 30 second mark he should be out
    ratelimiter.set_time(RateLimiter::PENALTY_TIME_SECONDS+31);
    result  = ratelimiter.RateCheck(sockaddr);
    ChkIf(result==false, E_FAIL);

Cleanup:    
    return hr;
}

HRESULT CTestRateLimiter::Test2()
{
    // simulate 20000 ip addresses getting in
    RateLimiterMockTime ratelimiter(20000);
    CSocketAddress badguy_addr(10000, 9999);
    CSocketAddress goodguy_addr(40000, 9999);
    bool result;
    HRESULT hr = S_OK;
    uint32_t ip = 0;
    
    for (int x = 0; x < 20000; x++)
    {
        ip = (uint32_t)x;
        CSocketAddress addr(ip, 9999);
        result = ratelimiter.RateCheck(addr);
        ChkIf(result == false, E_FAIL);
    }

    // force an entry in the table to be in the penalty boxy
    for (int x = 0; x < 60; x++)
    {
        ratelimiter.RateCheck(badguy_addr);
    }
    result = ratelimiter.RateCheck(badguy_addr);
    ChkIf(result, E_FAIL);

    // force another entry that should flush the table
    result = ratelimiter.RateCheck(goodguy_addr);
    ChkIf(result==false, E_FAIL);
    
    // bad guy will no longer be in table
    result = ratelimiter.RateCheck(badguy_addr);
    ChkIf(result==false, E_FAIL);
    
Cleanup:    
    return hr;
}

