#ifndef RATE_TRACKER_H
#define RATE_TRACKER_H

#include "socketaddress.h"
#include "fasthash.h"

struct RateTracker
{
    uint64_t count; // how many packets since firstEntryTime
    time_t firstEntryTime; // what time the first entry came in at
    time_t lastEntryTime;  // what time the last entry came in at (may not be needed
    time_t penaltyTime;    // what time he's allowed out of penalty (0 if no penalty)
};

struct RateTrackerAddress
{
    uint64_t addrbytes[2];
    bool operator==(const RateTrackerAddress& other)
    {
        return ((other.addrbytes[0] == addrbytes[0]) && (other.addrbytes[1] == addrbytes[1]));
    }
    RateTrackerAddress() : addrbytes()
    {

    }
};

inline size_t FastHash_Hash(const RateTrackerAddress& addr)
{
    size_t result;

    if (sizeof(size_t) >= 8)
    {
        result = addr.addrbytes[0] ^ addr.addrbytes[1];
    }
    else
    {
        uint32_t* pTmp = (uint32_t*)(addr.addrbytes);
        result = pTmp[0] ^ pTmp[1] ^ pTmp[2] ^ pTmp[3];
    }
    return result;
}


class RateLimiter
{
protected:
    
    virtual time_t get_time();
    uint64_t get_rate(const RateTracker* pRT);
    
    FastHashDynamic<RateTrackerAddress, RateTracker> _table;
    
    bool _isUsingLock;
    
    pthread_mutex_t _mutex;
    
    bool RateCheckImpl(const CSocketAddress& addr);
    
    
public:
    static const uint64_t MAX_RATE = 3600; // 60/minute normalized to an hourly rate
    static const uint64_t MIN_COUNT_FOR_CONSIDERATION = 60;
    static const time_t RESET_INTERVAL_SECONDS = 120;  // if he ever exceeds the hourly rate within a two minute interval, he gets penalized
    static const time_t PENALTY_TIME_SECONDS = 3600;   // one hour penalty
    
    bool RateCheck(const CSocketAddress& addr);
    
    RateLimiter(size_t tablesize, bool isUsingLock);
    virtual ~RateLimiter();
};

#endif
