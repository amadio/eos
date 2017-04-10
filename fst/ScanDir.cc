// ----------------------------------------------------------------------
// File: ScanDir.cc
// Author: Elvin Sindrilaru - CERN
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
#include "common/Attr.hh"
#include "common/Logging.hh"
#include "common/FileId.hh"
#include "common/Path.hh"
#include "fst/ScanDir.hh"
#include "fst/Config.hh"
#include "fst/XrdFstOfs.hh"
/*----------------------------------------------------------------------------*/
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#ifndef __APPLE__
#include <sys/syscall.h>
#endif
#include <fts.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
/*----------------------------------------------------------------------------*/

// --------------------------------------------------------------------------- 
// - we miss ioprio.h and gettid
// ---------------------------------------------------------------------------

static int
ioprio_set (int which, int who, int ioprio)
{
#ifdef __APPLE__
  return 0;
#else
  return syscall(SYS_ioprio_set, which, who, ioprio);
#endif
}

//static int ioprio_get(int which, int who)
//{
//  return syscall(SYS_ioprio_get, which, who);
//}

/*
 * Gives us 8 prio classes with 13-bits of data for each class
 */
#define IOPRIO_BITS             (16)
#define IOPRIO_CLASS_SHIFT      (13)
#define IOPRIO_PRIO_MASK        ((1UL << IOPRIO_CLASS_SHIFT) - 1)

#define IOPRIO_PRIO_CLASS(mask) ((mask) >> IOPRIO_CLASS_SHIFT)
#define IOPRIO_PRIO_DATA(mask)  ((mask) & IOPRIO_PRIO_MASK)
#define IOPRIO_PRIO_VALUE(class, data)  (((class) << IOPRIO_CLASS_SHIFT) | data)

#define ioprio_valid(mask)      (IOPRIO_PRIO_CLASS((mask)) != IOPRIO_CLASS_NONE)

/*
 * These are the io priority groups as implemented by CFQ. RT is the realtime
 * class, it always gets premium service. BE is the best-effort scheduling
 * class, the default for any process. IDLE is the idle scheduling class, it
 * is only served when no one else is using the disk.
 */

enum
{
  IOPRIO_CLASS_NONE,
  IOPRIO_CLASS_RT,
  IOPRIO_CLASS_BE,
  IOPRIO_CLASS_IDLE,
};

/*
 * 8 best effort priority levels are supported
 */
#define IOPRIO_BE_NR    (8)

enum
{
  IOPRIO_WHO_PROCESS = 1,
  IOPRIO_WHO_PGRP,
  IOPRIO_WHO_USER,
};

EOSFSTNAMESPACE_BEGIN

/*----------------------------------------------------------------------------*/
ScanDir::~ScanDir ()
{
  if ((bgThread && thread))
  {
    XrdSysThread::Cancel(thread);
    XrdSysThread::Join(thread, NULL);
    closelog();
  }
  if (buffer)
  {
    free(buffer);
  }
}

/*----------------------------------------------------------------------------*/
void
scandir_cleanup_paths (void *arg)
{
  char **paths = (char**) arg;
  if (paths)
  {
    free(paths);
    paths = 0;
  }
}

/*----------------------------------------------------------------------------*/
void
scandir_cleanup_fts (void *arg)
{
  FTS *tree = (FTS*) arg;
  if (tree)
  {
    fts_close(tree);
    tree = 0;
  }
}

/*----------------------------------------------------------------------------*/
void
ScanDir::ScanFiles ()
{
  char **paths = (char**) calloc(2, sizeof (char*));
  if (!paths)
  {
    return;
  }

  pthread_cleanup_push(scandir_cleanup_paths, paths);

  paths[0] = (char*) dirPath.c_str();
  paths[1] = 0;

  FTS *tree = fts_open(paths, FTS_NOCHDIR, 0);

  if (!tree)
  {
    if (bgThread)
    {
      eos_err("fts_open failed");
    }
    else
    {
      fprintf(stderr, "error: fts_open failed! \n");
    }

    free(paths);
    return;
  }

  pthread_cleanup_push(scandir_cleanup_fts, tree);


  FTSENT *node;
  while ((node = fts_read(tree)))
  {
    if (node->fts_level > 0 && node->fts_name[0] == '.')
    {
      fts_set(tree, node, FTS_SKIP);
    }
    else
    {
      if (node->fts_info == FTS_F)
      {
        XrdOucString filePath = node->fts_accpath;
        if (!filePath.matches("*.xsmap"))
        {
          if (!bgThread)
            fprintf(stderr, "[ScanDir] processing file %s\n", filePath.c_str());
          CheckFile(filePath.c_str());
        }
      }
    }
    if (bgThread)
      XrdSysThread::CancelPoint();
  }
  if (fts_close(tree))
  {
    if (bgThread)
    {
      eos_err("fts_close failed");
    }
    else
    {
      fprintf(stderr, "error: fts_close failed \n");
    }
  }

  free(paths);

  pthread_cleanup_pop(0);
  pthread_cleanup_pop(0);
}

/*----------------------------------------------------------------------------*/
void
ScanDir::CheckFile (const char* filepath)
{
  float scantime;
  unsigned long layoutid = 0;
  unsigned long long scansize;
  std::string filePath, checksumType, checksumStamp, logicalFileName;
  char checksumVal[SHA_DIGEST_LENGTH];
  size_t checksumLen;

  filePath = filepath;
  eos::common::Attr *attr = eos::common::Attr::OpenAttr(filePath.c_str());

  noTotalFiles++;

  // get last modification time
  struct stat buf1;
  struct stat buf2;
  if (stat(filePath.c_str(), &buf1))
  {
    if (bgThread)
    {
      eos_err("cannot stat %s", filePath.c_str());
    }
    else
    {
      fprintf(stderr, "error: cannot stat %s\n", filePath.c_str());
    }
    if (attr)
      delete attr;
    return;
  }

#ifndef _NOOFS
  if (bgThread) {
    eos::common::Path cPath(filePath.c_str());
    eos::common::FileId::fileid_t fid = strtoul(cPath.GetName(), 0, 16);
    
    // check if somebody is still writing on that file and skip in that case
    XrdSysMutexHelper wLock(gOFS.OpenFidMutex);
    if (gOFS.WOpenFid[fsId].count(fid)) {
      syslog(LOG_ERR, "skipping scan w-open file: localpath=%s fsid=%d fid=%x\n", filePath.c_str(), (int)fid, fsId);
      eos_warning("skipping scan of w-open file: localpath=%s fsid=%d fid=%x", filePath.c_str(), (int)fid, fsId);
      return;
    }
  }
#endif

  if (attr)
  {
    checksumType = attr->Get("user.eos.checksumtype");
    memset(checksumVal, 0, sizeof (checksumVal));
    checksumLen = SHA_DIGEST_LENGTH;
    if (!attr->Get("user.eos.checksum", checksumVal, checksumLen))
    {
      checksumLen = 0;
    }

    checksumStamp = attr->Get("user.eos.timestamp");
    logicalFileName = attr->Get("user.eos.lfn");

    bool rescan = RescanFile(checksumStamp);
    
    if (rescan || forcedScan)
    {
      if (1)
      {
        bool blockcxerror = false;
        bool filecxerror = false;
	bool skiptosettime = false;

        XrdOucString envstring = "eos.layout.checksum=";
        envstring += checksumType.c_str();
        XrdOucEnv env(envstring.c_str());
        unsigned long checksumtype = eos::common::LayoutId::GetChecksumFromEnv(env);
        layoutid = eos::common::LayoutId::GetId(eos::common::LayoutId::kPlain, checksumtype);
        if (rescan && (!ScanFileLoadAware(filePath.c_str(), scansize, scantime, checksumVal, layoutid, logicalFileName.c_str(), filecxerror, blockcxerror)))
        {
	  bool reopened=false;

#ifndef _NOOFS
	  if (bgThread) {
	    eos::common::Path cPath(filePath.c_str());
	    eos::common::FileId::fileid_t fid = strtoul(cPath.GetName(), 0, 16);
	    
	    // check if somebody is again writing on that file and skip in that case
	    XrdSysMutexHelper wLock(gOFS.OpenFidMutex);
	    if (gOFS.WOpenFid[fsId].count(fid)) 
	    {
	      eos_err("file %s has been reopened for update during the scan ... ignoring checksum error", filePath.c_str());
	      reopened = true;
	    }
	  }
#endif

          if ((!stat(filePath.c_str(), &buf2)) && (buf1.st_mtime == buf2.st_mtime) && (!reopened))
          {
            if (filecxerror)
            {
              if (bgThread)
              {
                syslog(LOG_ERR, "corrupted file checksum: localpath=%s lfn=\"%s\" \n", filePath.c_str(), logicalFileName.c_str());
                eos_err("corrupted file checksum: localpath=%s lfn=\"%s\"", filePath.c_str(), logicalFileName.c_str());
              }
              else
                fprintf(stderr, "[ScanDir] corrupted  file checksum: localpath=%s lfn=\"%s\" \n", filePath.c_str(), logicalFileName.c_str());
            }
          }
          else
          {
	    // if the file was changed in the meanwhile or is reopened for update, the checksum might have changed in the meanwhile, we cannot know now and leave it up to a later moment
	    blockcxerror = false;
	    filecxerror = false;
	    skiptosettime = true;
            if (bgThread)
            {
              eos_err("file %s has been modified during the scan ... ignoring checksum error", filePath.c_str());
            }
            else
            {
              fprintf(stderr, "[ScanDir] file %s has been modified during the scan ... ignoring checksum error\n", filePath.c_str());
            }
          }
        }
        //collect statistics
	if (rescan)
	{
	  durationScan += scantime;
	  totalScanSize += scansize;
	}

	bool failedtoset=false;

	if (rescan)
	{
	  if (!skiptosettime) 
	  {
	    if (!attr->Set("user.eos.timestamp", GetTimestampSmeared()))
	      failedtoset |= true;
	  }
	  if (!attr->Set("user.eos.filecxerror", filecxerror ? "1" : "0"))
	    failedtoset |= true;
	  if(!attr->Set("user.eos.blockcxerror", blockcxerror ? "1" : "0"))
	    failedtoset |= true;
	  
	  if (failedtoset)
	  {
	    if (bgThread)
	    {
	      eos_err("Can not set extended attributes to file %s", filePath.c_str());
	    }
	    else
	    {
	      fprintf(stderr, "error: [CheckFile] Can not set extended attributes to file. \n");
	    }
	  }
	}

        if (bgThread)
        {
	  XrdOucString manager = "";
	  {
	    XrdSysMutexHelper lock(eos::fst::Config::gConfig.Mutex);
	    manager = eos::fst::Config::gConfig.Manager.c_str();
	  }
	  if (manager.length())
          {
	    errno = 0;
	    eos::common::Path cPath(filePath.c_str());
	    eos::common::FileId::fileid_t fid = strtoul(cPath.GetName(), 0, 16);

	    if (fid && !errno)
	    {
	      // check if we have this file in the local DB, if not, we
	      // resync first the disk and then the mgm meta data
	      FmdSqlite* fmd = gFmdSqliteHandler.GetFmd ( fid, 
							  fsId,
							  0,0,0,false, true );

	      bool orphaned = fmd?(fmd->fMd.layouterror == eos::common::LayoutId::kOrphan):false;

	      if (fmd)
		delete fmd;

	      if (filecxerror || blockcxerror || !fmd || orphaned)
	      {
		eos_notice("msg=\"resyncing from disk\" fsid=%d fid=%lx", fsId, fid);
		// ask the meta data handling class to update the error flags for this file
		gFmdSqliteHandler.ResyncDisk(filePath.c_str(), fsId, false);

		eos_notice("msg=\"resyncing from mgm\" fsid=%d fid=%lx", fsId, fid);

		bool resynced = false;
		resynced = gFmdSqliteHandler.ResyncMgm(fsId, fid, manager.c_str());

		fmd = gFmdSqliteHandler.GetFmd ( fid, 
						 fsId,
						 0,0,0,false, true );
		if (resynced && fmd)
		{
		  if (fmd->fMd.layouterror ==  eos::common::LayoutId::kOrphan)
		  {
		    char oname[4096];
		    snprintf(oname, sizeof(oname), "%s/.eosorphans/%08x", dirPath.c_str(), (unsigned int) fid);
		    
		    // store the original path name as an extended attribute in case ...
		    attr->Set("user.eos.orphaned", filePath.c_str());
		    
		    // if this is an orphaned file - we move it into the orphaned directory
		    if (!rename(filePath.c_str(), oname))
		    {
		      eos_warning("msg=\"orphaned quarantened\" fst-path=%s orphan-path=%s", filePath.c_str(), oname);
		    }
		    else
		    {
		      eos_err("msg=\"failed to quarantene orphaned\" fst-path=%s orphan-path=%s", filePath.c_str(), oname);
		    }
		    
		    // remove the entry from the FMD database
		    gFmdSqliteHandler.DeleteFmd( fid, fsId);
		  }
		  delete fmd;
		}

		// call the autorepair method on the MGM
		// if the MGM has autorepair disabled it won't do anything
		gFmdSqliteHandler.CallAutoRepair(manager.c_str(), fid);
              }
            }
          }
        }
      }
      else
      {
        noNoChecksumFiles++;
      }
    }
    else
    {
      SkippedFiles++;
    }
    delete attr;
  }
}

/*----------------------------------------------------------------------------*/
eos::fst::CheckSum*
ScanDir::GetBlockXS (const char* filepath, unsigned long long maxfilesize)
{
  unsigned long layoutid = 0;
  std::string checksumType, checksumSize, logicalFileName;
  XrdOucString fileXSPath = filepath;

  eos::common::Attr *attr = eos::common::Attr::OpenAttr(fileXSPath.c_str());

  if (attr)
  {
    checksumType = attr->Get("user.eos.blockchecksum");
    checksumSize = attr->Get("user.eos.blocksize");
    logicalFileName = attr->Get("user.eos.lfn");
    delete attr;

    if (checksumType.compare(""))
    {
      XrdOucString envstring = "eos.layout.blockchecksum=";
      envstring += checksumType.c_str();
      XrdOucEnv env(envstring.c_str());
      unsigned long checksumtype = eos::common::LayoutId::GetBlockChecksumFromEnv(env);

      int blockSize = atoi(checksumSize.c_str());
      int blockSizeSymbol = eos::common::LayoutId::BlockSizeEnum(blockSize);

      layoutid = eos::common::LayoutId::GetId(eos::common::LayoutId::kPlain, eos::common::LayoutId::kNone, 0, blockSizeSymbol, checksumtype);

      eos::fst::CheckSum *checksum = eos::fst::ChecksumPlugins::GetChecksumObject(layoutid, true);

      if (checksum)
      {
        // get size of XS file
        struct stat info;

        if (stat(fileXSPath.c_str(), &info))
        {
          if (bgThread)
          {
            eos_err("cannot open file %s", fileXSPath.c_str());
          }
          else
          {
            fprintf(stderr, "error: cannot open file %s\n", fileXSPath.c_str());
          }
        }

        if (checksum->OpenMap(fileXSPath.c_str(), maxfilesize, blockSize, false))
        {
          return checksum;
        }
        else
        {
          delete checksum;
          return NULL;
        }
      }
      else
      {
        if (bgThread)
        {
          eos_err("cannot get checksum object for layout id %lx", layoutid);
        }
        else
        {
          fprintf(stderr, "error: cannot get checksum object for layout id %lx\n", layoutid);
        }
      }
    }
    else
      return NULL;
  }

  return NULL;
}

/*----------------------------------------------------------------------------*/
std::string
ScanDir::GetTimestamp ()
{

  char buffer[65536];
  size_t size = sizeof (buffer) - 1;
  long long timestamp;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  timestamp = tv.tv_sec * 1000000 + tv.tv_usec;

  snprintf(buffer, size, "%lli", timestamp);
  return std::string(buffer);
}

/*----------------------------------------------------------------------------*/
std::string
ScanDir::GetTimestampSmeared ()
{

  char buffer[65536];
  size_t size = sizeof (buffer) - 1;
  long long timestamp;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  timestamp = tv.tv_sec * 1000000 + tv.tv_usec;

  // smear +- 20% of testInterval around the value 
  long int smearing = (long int) ((0.2 * 2 * testInterval * random() / RAND_MAX)) - ((long int) (0.2 * testInterval));
  snprintf(buffer, size, "%lli", timestamp + smearing);
  return std::string(buffer);
}

/*----------------------------------------------------------------------------*/
bool
ScanDir::RescanFile (std::string fileTimestamp)
{
  if (!fileTimestamp.compare(""))
    return true; //first time we check

  long long oldTime = atoll(fileTimestamp.c_str());
  long long newTime = atoll(GetTimestamp().c_str());

  if (((newTime - oldTime) / 1000000) < testInterval)
  {
    return false;
  }
  else
  {
    return true;
  }
}

/*----------------------------------------------------------------------------*/
void*
ScanDir::StaticThreadProc (void* arg)
{
  return reinterpret_cast<ScanDir*> (arg)->ThreadProc();
}

/*----------------------------------------------------------------------------*/
void*
ScanDir::ThreadProc (void)
{

  if (bgThread)
  {
    // set low IO priority
    int retc = 0;
    pid_t tid = (pid_t) syscall(SYS_gettid);

    if ((retc = ioprio_set(IOPRIO_WHO_PROCESS, tid, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, 7))))
    {
      eos_err("cannot set io priority to lowest best effort = retc=%d errno=%d\n", retc, errno);
    }
    else
    {
      eos_notice("setting io priority to 7(lowest best-effort) for PID %u", tid);
    }
  }


  if (bgThread)
    XrdSysThread::SetCancelOn();

  forcedScan = false;

  struct stat buf;
  std::string forcedrun = dirPath.c_str();
  forcedrun += "/.eosscan";
  
  if (!stat(forcedrun.c_str(), &buf))
  {
    forcedScan = true;
    eos_notice("msg=\"scanner is in forced mode\"");
  }
  else
  {
    if (forcedScan)
    {
      forcedScan = false;
      eos_notice("msg=\"scanner is back to non-forced mode\"");
    }
  }

  if (bgThread && !forcedScan)
  {
    // get a random smearing and avoid that all start at the same time!
    // start in the range of 0 to 4 hours 
    size_t sleeper = (4 * 3600.0 * random() / RAND_MAX);
    for (size_t s = 0; s < (sleeper); s++)
    {
      if (bgThread)
        XrdSysThread::CancelPoint();
      XrdSysTimer sleeper;
      sleeper.Wait(1000);
    }
  }

  do
  {
    struct timezone tz;
    struct timeval tv_start, tv_end;

    {
      struct stat buf;
      
      if (!stat(forcedrun.c_str(), &buf))
      {
	if (!forcedScan)
	{
	  forcedScan = true;
	  eos_notice("msg=\"scanner is in forced mode\"");
	}
      }
      else
      {
	if (forcedScan)
	{
	  forcedScan = false;
	  eos_notice("msg=\"scanner is back to non-forced mode\"");
	}
      }
    }

    noScanFiles = 0;
    totalScanSize = 0;
    noCorruptFiles = 0;
    noNoChecksumFiles = 0;
    noTotalFiles = 0;
    SkippedFiles = 0;

    gettimeofday(&tv_start, &tz);
    ScanFiles();
    gettimeofday(&tv_end, &tz);

    durationScan = ((tv_end.tv_sec - tv_start.tv_sec) * 1000.0) + ((tv_end.tv_usec - tv_start.tv_usec) / 1000.0);
    if (bgThread)
    {
      syslog(LOG_ERR, "Directory: %s, files=%li scanduration=%.02f [s] scansize=%lli [Bytes] [ %lli MB ] scannedfiles=%li  corruptedfiles=%li nochecksumfiles=%li skippedfiles=%li\n", dirPath.c_str(), noTotalFiles, (durationScan / 1000.0), totalScanSize, ((totalScanSize / 1000) / 1000), noScanFiles, noCorruptFiles, noNoChecksumFiles, SkippedFiles);
      eos_notice("Directory: %s, files=%li scanduration=%.02f [s] scansize=%lli [Bytes] [ %lli MB ] scannedfiles=%li  corruptedfiles=%li nochecksumfiles=%li skippedfiles=%li", dirPath.c_str(), noTotalFiles, (durationScan / 1000.0), totalScanSize, ((totalScanSize / 1000) / 1000), noScanFiles, noCorruptFiles, noNoChecksumFiles, SkippedFiles);
    }
    else
    {
      fprintf(stderr, "[ScanDir] Directory: %s, files=%li scanduration=%.02f [s] scansize=%lli [Bytes] [ %lli MB ] scannedfiles=%li  corruptedfiles=%li nochecksumfiles=%li skippedfiles=%li\n", dirPath.c_str(), noTotalFiles, (durationScan / 1000.0), totalScanSize, ((totalScanSize / 1000) / 1000), noScanFiles, noCorruptFiles, noNoChecksumFiles, SkippedFiles);
    }

    if (!bgThread)
      break;
    else
    {
      if (!forcedScan) 
      {
	// run again after 4 hours
	for (size_t s = 0; s < (4 * 3600); s++)
	{
	  if (bgThread)
	    XrdSysThread::CancelPoint();
	  XrdSysTimer sleeper;
	  sleeper.Wait(1000);
	}
      }
    }

    if (bgThread)
      XrdSysThread::CancelPoint();
  }
  while (1);
  return NULL;
}

/*----------------------------------------------------------------------------*/
bool
ScanDir::ScanFileLoadAware (const char* path, unsigned long long &scansize, float &scantime, const char* checksumVal, unsigned long layoutid, const char* lfn, bool &filecxerror, bool &blockcxerror)
{
  double load;
  bool retVal, corruptBlockXS = false;
  int currentRate = rateBandwidth;
  std::string filePath, fileXSPath;
  struct timezone tz;
  struct timeval opentime;
  struct timeval currenttime;
  eos::fst::CheckSum *normalXS, *blockXS;

  scansize = 0;
  scantime = 0;

  filePath = path;
  fileXSPath = filePath + ".xsmap";

  normalXS = eos::fst::ChecksumPlugins::GetChecksumObject(layoutid);

  gettimeofday(&opentime, &tz);

  int fd = open(path, O_RDONLY);
  if (fd < 0)
  {
    delete normalXS;
    return false;
  }

  struct stat current_stat;
  if (fstat(fd, &current_stat)) 
  {
    delete normalXS;
    return false;
  }

  blockXS = GetBlockXS(fileXSPath.c_str(), current_stat.st_size);

  if ((!normalXS) && (!blockXS))
  {
    // there is nothing to do here
    close(fd);
    return false;
  }

  if (normalXS) normalXS->Reset();

  int nread = 0;
  off_t offset = 0;

  do
  {
    errno = 0;
    nread = read(fd, buffer, bufferSize);
    if (nread < 0)
    {
      close(fd);
      if (blockXS)
      {
        blockXS->CloseMap();
        delete blockXS;
      }
      if (normalXS) delete normalXS;
      return false;
    }

    if (nread)
    {
      if (!corruptBlockXS && blockXS)
        if (!blockXS->CheckBlockSum(offset, buffer, nread))
          corruptBlockXS = true;

      //      fprintf(stderr,"adding %ld %llu\n", nread,offset);
      if (normalXS) normalXS->Add(buffer, nread, offset);

      offset += nread;
      if (currentRate)
      {
        // regulate the verification rate
        gettimeofday(&currenttime, &tz);
        scantime = (((currenttime.tv_sec - opentime.tv_sec)*1000.0) + ((currenttime.tv_usec - opentime.tv_usec) / 1000.0));
        float expecttime = (1.0 * offset / currentRate) / 1000.0;
        if (expecttime > scantime)
        {
          XrdSysTimer sleeper;
          sleeper.Wait(expecttime - scantime);
        }
        //adjust the rate according to the load information
        load = fstLoad->GetDiskRate("sda", "millisIO") / 1000.0;
        if (load > 0.7)
        {
          //adjust currentRate
          if (currentRate > 5)
            currentRate = 0.9 * currentRate;
        }
        else
        {
          currentRate = rateBandwidth;
        }
      }
    }
  }
  while (nread == bufferSize);

  gettimeofday(&currenttime, &tz);
  scantime = (((currenttime.tv_sec - opentime.tv_sec)*1000.0) + ((currenttime.tv_usec - opentime.tv_usec) / 1000.0));
  scansize = (unsigned long long) offset;

  if (normalXS) normalXS->Finalize();

  //check file checksum only for replica layouts
  if ((normalXS) && (!normalXS->Compare(checksumVal)))
  {
    if (bgThread)
    {
      eos_err("Computed checksum is %s scansize %llu\n", normalXS->GetHexChecksum(), scansize);
    }
    else
    {
      fprintf(stderr, "error: computed checksum is %s scansize %llu\n", normalXS->GetHexChecksum(), scansize);
      if (setChecksum)
      {
        eos::common::Attr *attr = eos::common::Attr::OpenAttr(filePath.c_str());
        if (attr)
        {
          int checksumlen = 0;
          normalXS->GetBinChecksum(checksumlen);
          if ((!attr->Set("user.eos.checksum", normalXS->GetBinChecksum(checksumlen), checksumlen)) ||
              (!attr->Set("user.eos.filecxerror", "0")))
          {
            fprintf(stderr, "error: failed to reset existing checksum \n");
          }
          else
          {
            fprintf(stdout, "success: reset checksum of %s to %s\n", filePath.c_str(), normalXS->GetHexChecksum());
          }
          delete attr;
        }
      }
    }
    noCorruptFiles++;
    retVal = false;
    filecxerror = true;
  }
  else
  {
    retVal = true;
  }

  //check block checksum
  if (corruptBlockXS)
  {
    blockcxerror = true;
    if (bgThread)
    {
      syslog(LOG_ERR, "corrupted block checksum: localpath=%s blockxspath=%s lfn=%s\n", path, fileXSPath.c_str(), lfn);
      eos_crit("corrupted block checksum: localpath=%s blockxspath=%s lfn=%s", path, fileXSPath.c_str(), lfn);
    }
    else
    {
      fprintf(stderr, "[ScanDir] corrupted block checksum: localpath=%s blockxspath=%s lfn=%s\n", path, fileXSPath.c_str(), lfn);
    }

    retVal &= false;
  }
  else
  {
    retVal &= true;
  }

  //collect statistics
  noScanFiles++;

  if (normalXS) normalXS->Finalize();
  if (blockXS)
  {
    blockXS->CloseMap();
    delete blockXS;
  }

  if (normalXS) delete normalXS;
  close(fd);

  if (bgThread)
    XrdSysThread::CancelPoint();
  return retVal;
}

EOSFSTNAMESPACE_END
