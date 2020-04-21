/*
 *    Copyright (c) 2019, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OT_COMM_LIBRARY_TIMER_HPP_
#define OT_COMM_LIBRARY_TIMER_HPP_

#include <functional>

#include "commissioner/error.hpp"

#include "common/utils.hpp"
#include "library/event.hpp"
#include "library/time.hpp"

namespace ot {

namespace commissioner {

// The implementation of timer based on libevent.
class Timer
{
public:
    using Action = std::function<void(Timer &aTimer)>;

    Timer(struct event_base *aEventBase, Action aAction, bool aIsSingle = true)
        : mAction(aAction)
        , mIsSingle(aIsSingle)
        , mEnabled(false)
    {
        int flags = mIsSingle ? 0 : EV_PERSIST;

        ASSERT(event_assign(&mTimerEvent, aEventBase, -1, flags, HandleEvent, this) == 0);
    }

    ~Timer() { Stop(); }

    void Start(TimePoint aFireTime)
    {
        // In case the timer has already been started, stop it first.
        if (mEnabled)
        {
            Stop();
        }

        auto delay = std::chrono::duration_cast<std::chrono::microseconds>(aFireTime - Clock::now());

        struct timeval tv;
        evutil_timerclear(&tv);
        tv.tv_sec  = delay.count() / 1000000;
        tv.tv_usec = delay.count() % 1000000;

        ASSERT(event_add(&mTimerEvent, &tv) == 0);
        mFireTime = aFireTime;
        mEnabled  = true;
    }

    void Start(Duration aDelay) { Start(Clock::now() + aDelay); }

    void Stop()
    {
        if (mEnabled)
        {
            event_del(&mTimerEvent);
        }
        mEnabled = false;
    }

    bool IsRunning() const { return mEnabled; }

    TimePoint GetFireTime() { return mFireTime; }

private:
    static void HandleEvent(evutil_socket_t, short, void *aContext)
    {
        auto timer = reinterpret_cast<Timer *>(aContext);
        if (timer->mIsSingle)
        {
            timer->mEnabled = false;
            // The event will be automatically deleted by libevent.
        }

        timer->mAction(*timer);
    }

    struct event mTimerEvent;
    TimePoint    mFireTime;
    const Action mAction;
    const bool   mIsSingle;
    bool         mEnabled;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_TIMER_HPP_
