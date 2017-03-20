// ----------------------------------------------------------------------
// File: Timing.hh
// Author: Andreas-Joachim Peters - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2011 CERN/ASwitzerland                                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

//------------------------------------------------------------------------------
//! @brief Class providing real-time code measurements
//------------------------------------------------------------------------------

#ifndef __EOSCOMMON__TIMING__HH
#define __EOSCOMMON__TIMING__HH

#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "common/Namespace.hh"
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string>

EOSCOMMONNAMESPACE_BEGIN

/*----------------------------------------------------------------------------*/
//! Class implementing comfortable time measurements through methods/functions
//!
//! Example
//! eos::common::Timing tm("Test");
//! COMMONTIMING("START",&tm);
//! ...
//! COMMONTIMING("CHECKPOINT1",&tm);
//! ...
//! COMMONTIMING("CHECKPOINT2",&tm);
//! ...
//! COMMONTIMING("STOP", &tm);
//! tm.Print();
//! fprintf(stdout,"realtime = %.02f", tm.RealTime());
/*----------------------------------------------------------------------------*/
class Timing
{
public:
  struct timeval tv;
  XrdOucString tag;
  XrdOucString maintag;
  Timing* next;
  Timing* ptr;

  //----------------------------------------------------------------------------
  //! Constructor - used only internally
  //----------------------------------------------------------------------------
  Timing(const char* name, struct timeval& i_tv):
    tv{0}, tag(name), next(0)
  {
    memcpy(&tv, &i_tv, sizeof(struct timeval));
    ptr = this;
  }

  //----------------------------------------------------------------------------
  //! Constructor - tag is used as the name for the measurement in Print
  //----------------------------------------------------------------------------
  Timing(const char* i_maintag):
    tv{0}, tag("BEGIN"), maintag(i_maintag), next(0)
  {
    ptr = this;
  }

  //----------------------------------------------------------------------------
  //! Get time elapsed between the two tags in miliseconds
  //----------------------------------------------------------------------------
  float
  GetTagTimelapse(const std::string& tagBegin, const std::string& tagEnd)
  {
    float time_elapsed = 0;
    Timing* ptr = this->next;
    Timing* ptrBegin = 0;
    Timing* ptrEnd = 0;

    while (ptr) {
      if (tagBegin.compare(ptr->tag.c_str()) == 0) {
        ptrBegin = ptr;
      }

      if (tagEnd.compare(ptr->tag.c_str()) == 0) {
        ptrEnd = ptr;
      }

      if (ptrBegin && ptrEnd) {
        break;
      }

      ptr = ptr->next;
    }

    if (ptrBegin && ptrEnd) {
      time_elapsed = static_cast<float>(((ptrEnd->tv.tv_sec - ptrBegin->tv.tv_sec) *
                                         1000000 +
                                         (ptrEnd->tv.tv_usec - ptrBegin->tv.tv_usec)) / 1000.0);
    }

    return time_elapsed;
  }

  //----------------------------------------------------------------------------
  //! Get current time in nanoseconds
  //----------------------------------------------------------------------------
  static long long
  GetNowInNs()
  {
    struct timespec ts;
    GetTimeSpec(ts);
    return (1000000000 * ts.tv_sec + ts.tv_nsec);
  }

  //----------------------------------------------------------------------------
  //! Return the age of a timespec
  //----------------------------------------------------------------------------
  static long long
  GetAgeInNs(const struct timespec* ts, const struct timespec* now = NULL)
  {
    struct timespec tsn;

    if (!now) {
      GetTimeSpec(tsn);
      now = &tsn;
    }

    return (now->tv_sec - ts->tv_sec) * 1000000000 + (now->tv_nsec - ts->tv_nsec);
  }

  //----------------------------------------------------------------------------
  //! Return the age of a ns timestamp
  //----------------------------------------------------------------------------
  static long long
  GetAgeInNs(long long ts , const struct timespec* now = NULL)
  {
    struct timespec tsn;

    if (!now) {
      GetTimeSpec(tsn);
      now = &tsn;
    }

    return (now->tv_sec * 1000000000 + now->tv_nsec) - ts;
  }

  //----------------------------------------------------------------------------
  //! Return the coarse age of a ns timestamp
  //----------------------------------------------------------------------------
  static long long
  GetCoarseAgeInNs(long long ts , const struct timespec* now = NULL)
  {
    struct timespec tsn;

    if (!now) {
      GetTimeSpec(tsn, true);
      now = &tsn;
    }

    return (now->tv_sec * 1000000000 + now->tv_nsec) - ts;
  }

  //----------------------------------------------------------------------------
  //! Print method to display measurements on STDERR
  //----------------------------------------------------------------------------
  void
  Print()
  {
    char msg[512];
    Timing* p = this->next;
    Timing* n;
    cerr << std::endl;

    while (p && (n = p->next)) {
      sprintf(msg,
              "                                        [%12s] %12s<=>%-12s : %.03f\n",
              maintag.c_str(), p->tag.c_str(), n->tag.c_str(),
              (float)((n->tv.tv_sec - p->tv.tv_sec) * 1000000 + (n->tv.tv_usec -
                      p->tv.tv_usec)) / 1000.0);
      cerr << msg;
      p = n;
    }

    n = p;
    p = this->next;
    sprintf(msg,
            "                                        =%12s= %12s<=>%-12s : %.03f\n",
            maintag.c_str(), p->tag.c_str(), n->tag.c_str(),
            (float)((n->tv.tv_sec - p->tv.tv_sec) * 1000000 + (n->tv.tv_usec -
                    p->tv.tv_usec)) / 1000.0);
    cerr << msg;
  }

  //----------------------------------------------------------------------------
  //! Return total Realtime
  //----------------------------------------------------------------------------
  double
  RealTime()
  {
    Timing* p = this->next;
    Timing* n;

    while (p && (n = p->next)) {
      p = n;
    }

    n = p;
    p = this->next;
    return (double)((n->tv.tv_sec - p->tv.tv_sec) * 1000000 +
                    (n->tv.tv_usec - p->tv.tv_usec)) / 1000.0;
  }

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual
  ~Timing()
  {
    Timing* n = next;

    if (n) {
      delete n;
    }
  }

  //----------------------------------------------------------------------------
  //! Time Conversion Function for timestamp time strings
  //----------------------------------------------------------------------------
  static std::string
  UnixTimstamp_to_Day(time_t when)
  {
    struct tm* now = localtime(&when);
    std::string year;
    std::string month;
    std::string day;
    char sDay[4096];
    snprintf(sDay, sizeof(sDay), "%04u%02u%02u",
             (unsigned int)(now->tm_year + 1900),
             (unsigned int)(now->tm_mon + 1),
             (unsigned int)(now->tm_mday));
    return sDay;
  }

  //----------------------------------------------------------------------------
  //! Time Conversion Function for strings to unix time
  //----------------------------------------------------------------------------
  static time_t
  Day_to_UnixTimestamp(std::string day)
  {
    tzset();
    struct tm ctime;
    memset(&ctime, 0, sizeof(struct tm));
    strptime(day.c_str(), "%Y%m%d", &ctime);
    time_t ts = mktime(&ctime) - timezone;
    return ts;
  }

  //----------------------------------------------------------------------------
  //! Wrapper Function to hide difference between Apple and Linux
  //----------------------------------------------------------------------------
  static void
  GetTimeSpec(struct timespec& ts, bool coarse = false)
  {
#ifdef __APPLE__
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
#else

    if (coarse) {
#ifdef CLOCK_REALTIME_COARSE
      clock_gettime(CLOCK_REALTIME_COARSE, &ts);
#else
      clock_gettime(CLOCK_REALTIME, &ts);
#endif
    } else {
      clock_gettime(CLOCK_REALTIME, &ts);
    }

#endif
  }

  //----------------------------------------------------------------------------
  //! Time Conversion Function for ISO8601 time strings
  //----------------------------------------------------------------------------
  static std::string
  UnixTimstamp_to_ISO8601(time_t now)
  {
    struct tm* utctime;
    char str[21];
    struct tm utc;
    utctime = gmtime_r(&now, &utc);

    if (!utctime) {
      now = 0;
      utctime = gmtime_r(&now, &utc);
    }

    strftime(str, 21, "%Y-%m-%dT%H:%M:%SZ", utctime);
    return str;
  }

  //----------------------------------------------------------------------------
  //! Time Conversion Function for strings to ISO8601 time
  //----------------------------------------------------------------------------
  static time_t
  ISO8601_to_UnixTimestamp(std::string iso)
  {
    tzset();
    char temp[64];
    memset(temp, 0, sizeof(temp));
    strncpy(temp, iso.c_str(), (iso.length() < 64) ? iso.length() : 64);
    struct tm ctime;
    memset(&ctime, 0, sizeof(struct tm));
    strptime(temp, "%FT%T%z", &ctime);
    time_t ts = mktime(&ctime) - timezone;
    return ts;
  }

  static
  std::string utctime(time_t ttime)
  {
    struct tm utc;

    if (!gmtime_r(&ttime, &utc)) {
      time_t zt = 0;
      gmtime_r(&zt, &utc);
    }

    static const char wday_name[][4] = {
      "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static const char mon_name[][4] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static char result[40];
    sprintf(result, "%.3s, %02d %.3s %d %.2d:%.2d:%.2d GMT",
            wday_name[utc.tm_wday], utc.tm_mday, mon_name[utc.tm_mon],
            1900 + utc.tm_year, utc.tm_hour, utc.tm_min, utc.tm_sec);
    return std::string(result);
  }
};

//------------------------------------------------------------------------------
//! Macro to place a measurement throughout the code
//------------------------------------------------------------------------------
#define COMMONTIMING( __ID__,__LIST__)    \
  do {                                    \
    struct timeval tp = {0};              \
    struct timezone tz = {0};             \
    gettimeofday(&tp, &tz);                                   \
    (__LIST__)->ptr->next=new eos::common::Timing(__ID__,tp); \
    (__LIST__)->ptr = (__LIST__)->ptr->next;                  \
  } while(false);

EOSCOMMONNAMESPACE_END

#endif
