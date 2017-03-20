// ----------------------------------------------------------------------
// File: com_version.cc
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

/*----------------------------------------------------------------------------*/
#include "console/ConsoleMain.hh"
/*----------------------------------------------------------------------------*/

/* Get the server version*/
int
com_version(char* arg)
{
  XrdOucString in = "mgm.cmd=version";
  eos::common::StringTokenizer subtokenizer(arg);
  XrdOucString option = "";
  XrdOucString options = "";
  subtokenizer.GetLine();

  if (wants_help(arg)) {
    goto com_version_usage;
  }

  do {
    option = subtokenizer.GetToken();

    if (!option.length()) {
      break;
    }

    if (option == "-f") {
      options += "f";
    } else if (option == "-m") {
      options += "m";
    } else {
      goto com_version_usage;
    }
  } while (1);

  if (options.length()) {
    in += "&mgm.option=";
    in += options;
  }

  global_retc = output_result(client_user_command(in));
  fprintf(stdout, "EOS_CLIENT_VERSION=%s EOS_CLIENT_RELEASE=%s\n", VERSION,
          RELEASE);
  return (0);
com_version_usage:
  fprintf(stdout,
          "usage: version [-f] [-m]                                             :  print EOS version number\n");
  fprintf(stdout,
          "                -f                                                   -  print the list of supported features\n");
  fprintf(stdout,
          "                -m                                                   -  print in monitoring format\n");
  global_retc = EINVAL;
  return (0);
}

