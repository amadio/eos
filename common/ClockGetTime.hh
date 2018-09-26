#include <time.h>

#ifdef __MACH__
#include <mach/mach_time.h>
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
#define CLOCK_REALTIME_COARSE 0
#define clockid_t int
#define clock_gettime _clock_gettime
#endif

int _clock_gettime(clockid_t clk_id, struct timespec* t)
{
  if (clock_gettime == 0) {
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer) / ((
                        double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer) / ((
                       double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return 0;
  } else {
    return clock_gettime(clk_id, t);
  }
}
#else
#define _clock_gettime clock_gettime
#endif
