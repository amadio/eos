// ----------------------------------------------------------------------
// File: Stat.cc
// Author: Andreas-Joachim Peters - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2018 CERN/Switzerland                                  *
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

#include "common/Logging.hh"
#include "mgm/Stat.hh"
#include "mgm/XrdMgmOfs.hh"
#include "mgm/Macros.hh"

#include <XrdOuc/XrdOucEnv.hh>

//----------------------------------------------------------------------------
// Stat a file or directory.
// Will redirect to the RW master.
//----------------------------------------------------------------------------
int
XrdMgmOfs::FuseStat(const char *path,
                    const char *ininfo,
                    XrdOucEnv &env,
                    XrdOucErrInfo &error,
                    eos::common::LogId &ThreadLogId,
                    eos::common::VirtualIdentity &vid,
                    const XrdSecEntity *client)
{
  ACCESSMODE_R_MASTER;
  MAYSTALL;
  MAYREDIRECT;

  gOFS->MgmStats.Add("Fuse-Stat", vid.uid, vid.gid, 1);

  struct stat buf;
  int retc = lstat(path, &buf, error, client, ininfo);

  if (retc == SFS_OK) {
    char* statinfo = static_cast<char*>(malloc(16384));

    sprintf(statinfo,
            "stat: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu\n",
            (unsigned long long) buf.st_dev,
            (unsigned long long) buf.st_ino,
            (unsigned long long) buf.st_mode,
            (unsigned long long) buf.st_nlink,
            (unsigned long long) buf.st_uid,
            (unsigned long long) buf.st_gid,
            (unsigned long long) buf.st_rdev,
            (unsigned long long) buf.st_size,
            (unsigned long long) buf.st_blksize,
            (unsigned long long) buf.st_blocks,
#ifdef __APPLE__
            (unsigned long long) buf.st_atimespec.tv_sec,
            (unsigned long long) buf.st_mtimespec.tv_sec,
            (unsigned long long) buf.st_ctimespec.tv_sec,
            (unsigned long long) buf.st_atimespec.tv_nsec,
            (unsigned long long) buf.st_mtimespec.tv_nsec,
            (unsigned long long) buf.st_ctimespec.tv_nsec
#else
            (unsigned long long) buf.st_atime,
            (unsigned long long) buf.st_mtime,
            (unsigned long long) buf.st_ctime,
            (unsigned long long) buf.st_atim.tv_nsec,
            (unsigned long long) buf.st_mtim.tv_nsec,
            (unsigned long long) buf.st_ctim.tv_nsec
#endif
           );

    // Ownership of statinfo is taken by the xrd_buff object.
    // Error then takes ownership of the xrd_buff object
    XrdOucBuffer* xrd_buff = new XrdOucBuffer(statinfo, strlen(statinfo));
    error.setErrInfo(xrd_buff->BuffSize(), xrd_buff);
    return SFS_DATA;
  }

  XrdOucString response = "stat: retc=";
  response += error.getErrInfo();
  error.setErrInfo(response.length() + 1, response.c_str());
  return SFS_DATA;
}
