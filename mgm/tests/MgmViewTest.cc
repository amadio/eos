// ----------------------------------------------------------------------
// File: MgmViewTest.cc
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

#include "mq/XrdMqSharedObject.hh"
#include "mq/XrdMqMessaging.hh"
#include "mgm/FsView.hh"
#include "mgm/IConfigEngine.hh"

using namespace eos::common;
using namespace eos::mgm;

int main()
{
  srand(0);
  Logging::Init();
  Logging::SetUnit("MgmViewTest");
  Logging::SetLogPriority(LOG_DEBUG);
  XrdMqMessage::Configure("");
  XrdMqSharedObjectManager ObjectManager;
  ObjectManager.SetDebug(true);
  XrdMqMessaging messaging("root://localhost:1097//eos/test/worker",
                           "/eos/*/worker", false, false, &ObjectManager);
  messaging.StartListenerThread();
  std::string queuepath;
  std::string queue;
  std::string schedgroup;
  int iloop = 10;
  int jloop = 10;

  for (int i = 0; i < iloop; i++) {
    char n[1024];
    sprintf(n, "%03d", i);
    std::string queue = "/eos/test";
    queue += n;
    queue += "/fst";

    for (int j = 0; j < jloop; j++) {
      schedgroup = "default.";
      char m[1024];
      sprintf(m, "%03d", j);
      queuepath = queue;
      queuepath += "/data";
      queuepath += m;
      schedgroup += m;
      //printf("Setting up schedgroup %s\n", schedgroup.c_str());
      ObjectManager.CreateSharedHash(queuepath.c_str(), queue.c_str(),
                                     &ObjectManager);
      XrdMqSharedHash* hash = ObjectManager.GetObject(queuepath.c_str(), "hash");

      if (!hash) {
        std::cerr << "Error: Unable to get hash object!" << std::endl;
        return -1;
      }

      hash->OpenTransaction();
      hash->Set("id", (i * iloop) + j);
      hash->Set("schedgroup", schedgroup.c_str());
      hash->Set("queuepath", queuepath.c_str());
      hash->Set("queue", queue.c_str());
      hash->Set("errmsg", "");
      hash->Set("errc", 0);
      hash->Set("errc", 0);
      hash->Set("status", eos::mgm::FileSystem::kDown);
      hash->Set("configstatus", eos::mgm::FileSystem::kUnknown);
      hash->Set("bootSentTime", 0);
      hash->Set("bootDoneTime", 0);
      hash->Set("lastHeartBeat", 0);
      char geotag[1024];
      snprintf(geotag, 1024, "branch%1.1d::leaf%1.1d", (i + j) % 2, (i + j) % 3);
      hash->Set("stat.geotag", geotag);
      hash->Set("statfs.disk.load", 0);
      hash->Set("statfs.disk.in", 0);
      hash->Set("statfs.disk.out", 0);
      hash->Set("statfs.net.load", 0);
      hash->Set("statfs.type", 0);
      hash->Set("statfs.bsize", 0);
      hash->Set("statfs.blocks", 1000000.0 * rand() / RAND_MAX);
      hash->Set("statfs.bfree", 0);
      hash->Set("statfs.bavail", 0);
      hash->Set("statfs.files", 0);
      hash->Set("statfs.ffree", 0);
      hash->Set("statfs.namelen", 0);
      hash->Set("statfs.ropen", 0);
      hash->Set("statfs.wopen", 0);
      hash->CloseTransaction();
      eos::mgm::FileSystem* fs = new eos::mgm::FileSystem(queuepath.c_str(),
          queue.c_str(), &ObjectManager);
      FsView::gFsView.Register(fs);
    }
  }

  // test the print function
  std::string output = "";
  std::string format1 =
    "header=1:member=type:width=20:format=-s|sep=   |member=name:width=20:format=-s|sep=   |avg=stat.geotag:width=32:format=s|sep=   |sum=statfs.blocks:width=20:format=-l|sep=   |avg=statfs.blocks:width=20:format=-f |sep=   |sig=statfs.blocks:width=20:format=-f";
  std::string format2 =
    "header=1:member=type:width=20:format=+s|sep=   |member=name:width=20:format=+s|sep=   |avg=stat.geotag:width=32:format=s|sep=   |sum=statfs.blocks:width=20:format=+l:unit=B|sep=   |avg=statfs.blocks:width=20:format=+f:unit=B|sep=   |sig=statfs.blocks:width=20:format=+f:unit=B";
  std::string format3 =
    "header=1:member=type:width=1:format=os|sep=&|member=name:width=1:format=os|sep=&|avg=stat.geotag:width=1:format=os|sep=&|sum=statfs.blocks:width=1:format=ol|sep=&|avg=statfs.blocks:width=1:format=ol|sep=&|sig=statfs.blocks:width=1:format=ol";
  std::string listformat1 =
    "header=1:key=queuepath:width=30:format=s|sep=   |key=schedgroup:width=10:format=s|sep=   |key=blocks:width=10:format=l|sep=   |key=statfs.wopen:width=10:format=l|sep= |key=stat.geotag:width=16:format=s";
  std::string listformat2 =
    "key=queuepath:width=2:format=os|sep=&|key=schedgroup:width=1:format=os|sep=&|key=blocks:width=1:format=os|sep=&|key=statfs.wopen:width=1:format=os";
  output += "[ next test ]\n";
  std::vector<std::string> selections;
  FsView::gFsView.mSpaceView["default"]->Print(output, format1, "", 2,
      selections);
  output += "[ next test ]\n";
  FsView::gFsView.PrintSpaces(output, format1, "", 2);
  output += "[ next test ]\n";
  FsView::gFsView.PrintGroups(output, format1, "", 2);
  output += "[ next test ]\n";
  FsView::gFsView.PrintNodes(output, format1, "", 2);
  output += "[ next test ]\n";
  FsView::gFsView.mSpaceView["default"]->Print(output, format2, "", 2,
      selections);
  output += "[ next test ]\n";
  FsView::gFsView.PrintSpaces(output, format2, "", 2);
  output += "[ next test ]\n";
  FsView::gFsView.PrintGroups(output, format2, "", 2);
  output += "[ next test ]\n";
  FsView::gFsView.PrintNodes(output, format2, "", 2);
  output += "[ next test ]\n";
  FsView::gFsView.PrintNodes(output, format3, "", 2);
  output += "[ next test ]\n";
  FsView::gFsView.PrintGroups(output, format2, listformat1, 2);
  output += "[ next test ]\n";
  FsView::gFsView.PrintGroups(output, format2, listformat2, 2);
  output += "[ next test ]\n";
  FsView::gFsView.PrintSpaces(output, format2, listformat1, 2);
  fprintf(stdout, "%s\n", output.c_str());

  // remove filesystems
  for (int i = 0; i < iloop; i++) {
    char n[1024];
    sprintf(n, "%02d", i);
    std::string queue = "/eos/test";
    queue += n;
    queue += "/fst";

    for (int j = 0; j < jloop; j++) {
      schedgroup = "default.";
      char m[1024];
      sprintf(m, "%02d", j);
      queuepath = queue;
      queuepath += "/data";
      queuepath += m;
      schedgroup += m;
      //      printf("Setting up %s\n", queuepath.c_str());
      unsigned int fsid = (i * iloop) + j;
      FsView::gFsView.ViewMutex.LockRead();
      eos::mgm::FileSystem* fs = FsView::gFsView.mIdView[fsid];
      FsView::gFsView.ViewMutex.UnLockRead();

      if (fs) {
        FsView::gFsView.UnRegister(fs);
      }
    }
  }

  return 0;
}
