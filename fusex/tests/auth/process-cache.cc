//------------------------------------------------------------------------------
// File: process-cache.cc
// Author: Georgios Bitzes - CERN
//------------------------------------------------------------------------------

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

#include "auth/ProcessCache.hh"
#include "test-utils.hh"
#include <gtest/gtest.h>

TEST_F(UnixAuthF, BasicSanity)
{
  injectProcess(1234, 1, 1234, 1234, 9999, 0);
  ProcessSnapshot snapshot = processCache()->retrieve(1234, 5, 6, false);
  ASSERT_EQ(snapshot->getXrdLogin(), LoginIdentifier(5, 6, 1234,
            0).getStringID());
  ProcessSnapshot snapshot2 = processCache()->retrieve(1234, 5, 6, false);
  ASSERT_EQ(snapshot2->getXrdLogin(), LoginIdentifier(5, 6, 1234,
            0).getStringID());
  ProcessSnapshot snapshot3 = processCache()->retrieve(1234, 5, 6, true);
  ASSERT_EQ(snapshot3->getXrdLogin(), LoginIdentifier(5, 6, 1234,
            1).getStringID());
  ProcessSnapshot snapshot4 = processCache()->retrieve(1234, 7, 6, false);
  ASSERT_EQ(snapshot4->getXrdLogin(), LoginIdentifier(7, 6, 1234,
            0).getStringID());
  injectProcess(1235, 1, 1235, 1235, 9999, 0);
  ProcessSnapshot snapshot5 = processCache()->retrieve(1235, 8, 6, false);
  ASSERT_EQ(snapshot5->getXrdLogin(), LoginIdentifier(8, 6, 1235,
            0).getStringID());
}

TEST_F(Krb5AuthF, BasicSanity)
{
  injectProcess(1234, 1, 1234, 1234, 9999, 0);
  securityChecker()->inject(localJail().id, "/tmp/my-creds", 1000, 0400, 1);
  environmentReader()->inject(1234, createEnv("/tmp/my-creds", ""));
  ProcessSnapshot snapshot = processCache()->retrieve(1234, 1000, 1000, false);
  ASSERT_EQ(snapshot->getXrdLogin(), LoginIdentifier(1).getStringID());
  ASSERT_EQ(snapshot->getXrdCreds(),
            "xrd.k5ccname=/tmp/my-creds&xrd.wantprot=krb5,unix&xrdcl.secgid=1000&xrdcl.secuid=1000");
}

TEST_F(Krb5AuthF, UnixFallback)
{
  injectProcess(1234, 1, 1234, 1234, 9999, 0);
  ProcessSnapshot snapshot = processCache()->retrieve(1234, 1000, 1000, false);
  ASSERT_EQ(snapshot->getXrdLogin(), LoginIdentifier(1000, 1000, 1234,
            0).getStringID());
  ASSERT_EQ(snapshot->getXrdCreds(), "xrd.wantprot=unix");
}
