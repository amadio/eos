// ----------------------------------------------------------------------
// File: Readlink.cc
// Author: Andreas-Joachim Peters - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2011 CERN/Switzerland                                  *
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


// -----------------------------------------------------------------------
// This file is included source code in XrdMgmOfs.cc to make the code more
// transparent without slowing down the compilation time.
// -----------------------------------------------------------------------

{
  ACCESSMODE_R;
  MAYSTALL;
  MAYREDIRECT;

  gOFS->MgmStats.Add("Fuse-Readlink", vid.uid, vid.gid, 1);

  XrdOucString link = "";

  int retc = 0;

  if (readlink(spath.c_str(),
  error,
  link,
  client))
  {
    retc = error.getErrInfo();

    if (!retc) {
      retc = -1;
    }
  }

  XrdOucString response = "readlink: retc=";
  response += retc;

  if (!retc)
  {
    response += " ";

    if (env.Get("eos.encodepath")) {
      link = eos::common::StringConversion::curl_escaped(link.c_str()).c_str();
    }

    response += link.c_str();
  }

  error.setErrInfo(response.length() + 1, response.c_str());
  return SFS_DATA;
}
