#include "commonincludes.hpp"
#include "socketaddress.h"
#include "fasthash.h"
#include "ratelimiter.h"


RateLimiter::RateLimiter(size_t tablesize, bool isUsingLock)
{
    _table.InitTable(tablesize, tablesize/2);
    this->_isUsingLock = isUsingLock;
    pthread_mutex_init(&_mutex, NULL);
}

RateLimiter::~RateLimiter()
{
    pthread_mutex_destroy(&_mutex);
}


time_t RateLimiter::get_time()
{
    return time(NULL);
}

uint64_t RateLimiter::get_rate(const RateTracker* pRT)
{
    if (pRT->count < MIN_COUNT_FOR_CONSIDERATION)
    {
        return 0;
    }
    
    time_t seconds = 1;
    
    if (pRT->lastEntryTime > pRT->firstEntryTime)
    {
        seconds = pRT->lastEntryTime - pRT->firstEntryTime;
    }
    
    uint64_t rate = (pRT->count * 3600) / seconds;
    
    return rate;
}

bool RateLimiter::RateCheck(const CSocketAddress& addr)
{
    if (_isUsingLock)
    {
        pthread_mutex_lock(&_mutex);
    }
    
    bool result = RateCheckImpl(addr);
    
    if (_isUsingLock)
    {
        pthread_mutex_unlock(&_mutex);
    }
    
    return result;
    
}

bool RateLimiter::RateCheckImpl(const CSocketAddress& addr)
{
    RateTrackerAddress rtaddr;
    addr.GetIP(rtaddr.addrbytes, sizeof(rtaddr.addrbytes));
    time_t currentTime = get_time();
    
    RateTracker* pRT = this->_table.Lookup(rtaddr);
    uint64_t rate = 0;
    
    
    // handle these cases differently:
    // 1. Not in the table
    //      Insert into table
    //      if table is full, reset the table and re-insert
    //      return true
    // 2. In the penalty box
    //      Check for parole eligibility
    //      otherwise, return false
    // 3. Not in the penalty box.
    //      Remove from table if his previous record is sufficiently old (and return true)
    //      
    
    
    if (pRT == NULL)
    {
        RateTracker rt;
        rt.count = 1;
        rt.firstEntryTime = currentTime;
        rt.lastEntryTime = rt.firstEntryTime;
        rt.penaltyTime = 0;
        
        int result = _table.Insert(rtaddr, rt);
        
        if (result == -1)
        {
            // the table is full - try again after dumping the table
            // I've considered a half dozen alternatives to doing this, but this is the simplest
            _table.Reset();
            _table.Insert(rtaddr, rt);
        }
        return true;
    }
    

    pRT->count++;
    pRT->lastEntryTime = currentTime;
    rate = get_rate(pRT);
    
    
    if (pRT->penaltyTime != 0)
    {
        if (pRT->penaltyTime >= currentTime)
        {
            return false;
        }

        if (rate < MAX_RATE)
        {
            pRT->penaltyTime = 0;
            // not reseting firstEntryTime or lastEntryTime.
            return true;
        }
        
        // he's out of penalty, but is still flooding us.  Tack on another hour
        pRT->penaltyTime = currentTime + PENALTY_TIME_SECONDS;
        return false;
    }
    
    if (rate >= MAX_RATE)
    {
        pRT->penaltyTime = currentTime + PENALTY_TIME_SECONDS; // welcome to the penalty box
        return false;
    }
    
    if ((pRT->lastEntryTime - pRT->firstEntryTime) > RESET_INTERVAL_SECONDS)
    {
        // he's been a good citizen this whole time, we can take him out of the table
        // to save room for another entry
        _table.Remove(rtaddr);
    }
    
    return true;
}

