// ----------------------------------------------------------------------
// File: FmdSqlite.hh
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

/**
 * @file   FmdSqlite.hh
 * 
 * @brief  Classes for FST File Meta Data Handling.
 * 
 * 
 */

#ifndef __EOSFST_FmdSQLITE_HH__
#define __EOSFST_FmdSQLITE_HH__

/*----------------------------------------------------------------------------*/
#include "fst/Namespace.hh"
#include "common/Logging.hh"
#include "common/SymKeys.hh"
#include "common/FileId.hh"
#include "common/FileSystem.hh"
#include "common/LayoutId.hh"
#include "common/sqlite/sqlite3.h"
#include "fst/FmdClient.hh"
/*----------------------------------------------------------------------------*/
#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysPthread.hh"
/*----------------------------------------------------------------------------*/
// this is needed because of some openssl definition conflict!
#undef des_set_key
#include <google/dense_hash_map>
#include <google/sparse_hash_map>
#include <google/sparsehash/densehashtable.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <zlib.h>
#include <openssl/sha.h>

#ifdef __APPLE__
#define ECOMM 70
#endif

EOSFSTNAMESPACE_BEGIN

// ---------------------------------------------------------------------------
//! Class implementing file meta data
// ---------------------------------------------------------------------------
class FmdSqlite : public eos::common::LogId
{
public:
  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  FmdSqlite (int fid = 0, int fsid = 0)
  {
    Reset(fMd);
    fMd.fid = fid;
    fMd.fsid = fsid;
  }

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual ~FmdSqlite () {};

  // ---------------------------------------------------------------------------
  //! File Metadata object 
  struct Fmd fMd;
  // ---------------------------------------------------------------------------

  // ---------------------------------------------------------------------------
  //! File meta data object replication function (copy constructor)
  // ---------------------------------------------------------------------------

  void
  Replicate (struct Fmd &fmd)
  {
    fMd = fmd;
  }

  // ---------------------------------------------------------------------------
  //! Compute layout error
  // ---------------------------------------------------------------------------

  static int
  LayoutError (eos::common::FileSystem::fsid_t fsid, eos::common::LayoutId::layoutid_t lid, std::string locations)
  {
    if (lid == 0)
    {
      // an orphans has not lid at the MGM e.g. lid=0
      return eos::common::LayoutId::kOrphan;
    }

    int lerror = 0;
    std::vector<std::string> location_vector;
    std::set<eos::common::FileSystem::fsid_t> location_set;
    eos::common::StringConversion::Tokenize(locations, location_vector, ",");
    size_t validreplicas = 0;
    for (size_t i = 0; i < location_vector.size(); i++)
    {
      if (location_vector[i].length())
      {
        // unlinked locates have a '!' infront of the fsid
        if (location_vector[i][0] == '!')
        {
          location_set.insert(strtoul(location_vector[i].c_str() + 1, 0, 10));
        }
        else
        {
          location_set.insert(strtoul(location_vector[i].c_str(), 0, 10));
          validreplicas++;
        }
      }
    }
    size_t nstripes = eos::common::LayoutId::GetStripeNumber(lid) + 1;
    if (nstripes != validreplicas)
    {
      lerror |= eos::common::LayoutId::kReplicaWrong;
    }
    if (!location_set.count(fsid))
    {
      lerror |= eos::common::LayoutId::kUnregistered;
    }
    return lerror;
  }

  // ---------------------------------------------------------------------------
  //! File meta data object reset function
  // ---------------------------------------------------------------------------

  static void
  Reset (struct Fmd &fmd)
  {
    fmd.fid = 0;
    fmd.cid = 0;
    fmd.ctime = 0;
    fmd.ctime_ns = 0;
    fmd.mtime = 0;
    fmd.mtime_ns = 0;
    fmd.atime = 0;
    fmd.atime_ns = 0;
    fmd.checktime = 0;
    fmd.size = 0xfffffffffff1ULL;
    fmd.disksize = 0xfffffffffff1ULL;
    fmd.mgmsize = 0xfffffffffff1ULL;
    fmd.checksum = "";
    fmd.diskchecksum = "";
    fmd.mgmchecksum = "";
    fmd.lid = 0;
    fmd.uid = 0;
    fmd.gid = 0;
    fmd.filecxerror = 0;
    fmd.blockcxerror = 0;
    fmd.layouterror = 0;
    fmd.locations = "";
  }

  // ---------------------------------------------------------------------------
  //! Retrieve a set with all fsid locations
  // ---------------------------------------------------------------------------
  static std::set<eos::common::FileSystem::fsid_t> GetLocations(struct Fmd &fmd)
  {
    std::vector<std::string> location_vector;
    std::set<eos::common::FileSystem::fsid_t> location_set;
    eos::common::StringConversion::Tokenize(fmd.locations, location_vector, ",");
    for (size_t i=0; i< location_vector.size(); i++)
    {
      if (location_vector[i].length())
      {
	// unlinked locates have a '!' infront of the fsid
	if (location_vector[i][0] == '!')
	{
	  location_set.insert(strtoul(location_vector[i].c_str() + 1, 0, 10));
	}
	else
	{
	  location_set.insert(strtoul(location_vector[i].c_str(), 0, 10));
	}
      }
    }
    return location_set;
  }

  // ---------------------------------------------------------------------------
  //! Dump Fmd
  // ---------------------------------------------------------------------------
  static void Dump (struct Fmd* fmd);

  // ---------------------------------------------------------------------------
  //! Convert Fmd into env representation
  // ---------------------------------------------------------------------------
  XrdOucEnv* FmdSqliteToEnv ();
};

// ---------------------------------------------------------------------------
//! Class handling many Fmd changelog files at a time
// ---------------------------------------------------------------------------

class FmdSqliteHandler : public eos::fst::FmdClient
{
public:
  typedef std::vector<std::map< std::string, XrdOucString > > qr_result_t;

  struct FsCallBackInfo
  {
    eos::common::FileSystem::fsid_t fsid;
    google::dense_hash_map<unsigned long long, struct Fmd >* fmdmap;
  };

  typedef struct FsCallBackInfo fs_callback_info_t;

  XrdOucString DBDir; //< path to the directory with the SQLITE DBs
  eos::common::RWMutex Mutex; //< Mutex protecting the Fmd handler

  std::map<eos::common::FileSystem::fsid_t, sqlite3*> *
  GetDB ()
  {
    return &DB;
  }

  // ---------------------------------------------------------------------------
  //! Return's the syncing flag (if we sync, all files on disk are flagge as orphans until the MGM meta data has been verified and when this flag is set, we don't report orphans!
  // ---------------------------------------------------------------------------

  bool
  IsSyncing (eos::common::FileSystem::fsid_t fsid)
  {
    return isSyncing[fsid];
  }

  // ---------------------------------------------------------------------------
  //! Return's the dirty flag indicating a non-clean shutdown
  // ---------------------------------------------------------------------------

  bool
  IsDirty (eos::common::FileSystem::fsid_t fsid)
  {
    return isDirty[fsid];
  }

  // ---------------------------------------------------------------------------
  //! Set the stay dirty flag indicating a non completed bootup
  // ---------------------------------------------------------------------------

  void
  StayDirty (eos::common::FileSystem::fsid_t fsid, bool dirty)
  {
    stayDirty[fsid] = dirty;
  }

  // ---------------------------------------------------------------------------
  //! Define a DB file for a filesystem id
  // ---------------------------------------------------------------------------
  bool SetDBFile (const char* dbfile, int fsid, XrdOucString option = "");

  // ---------------------------------------------------------------------------
  //! Shutdown a DB for a filesystem
  // ---------------------------------------------------------------------------
  bool ShutdownDB (eos::common::FileSystem::fsid_t fsid);

  // ---------------------------------------------------------------------------
  //! Mark Clean
  // ---------------------------------------------------------------------------
  bool MarkCleanDB (eos::common::FileSystem::fsid_t fsid);

  // ---------------------------------------------------------------------------
  //! Read all Fmd entries from a DB file
  // ---------------------------------------------------------------------------
  bool ReadDBFile (eos::common::FileSystem::fsid_t, XrdOucString option = "");

  // ---------------------------------------------------------------------------
  //! Trim a DB file
  // ---------------------------------------------------------------------------
  bool TrimDBFile (eos::common::FileSystem::fsid_t fsid, XrdOucString option = "");

  // the meta data handling functions

  // ---------------------------------------------------------------------------
  //! attach or create a fmd record
  // ---------------------------------------------------------------------------
  FmdSqlite* GetFmd (eos::common::FileId::fileid_t fid, eos::common::FileSystem::fsid_t fsid, uid_t uid, gid_t gid, eos::common::LayoutId::layoutid_t layoutid, bool isRW = false, bool force = false);

  // ---------------------------------------------------------------------------
  //! Delete an fmd record
  // ---------------------------------------------------------------------------
  bool DeleteFmd (eos::common::FileId::fileid_t fid, eos::common::FileSystem::fsid_t fsid);

  // ---------------------------------------------------------------------------
  //! Commit a modified fmd record
  // ---------------------------------------------------------------------------
  bool Commit (FmdSqlite* fmd, bool lockit = true);

  // ---------------------------------------------------------------------------
  //! Commit a modified fmd record without locks and change of modification time
  // ---------------------------------------------------------------------------
  bool CommitFromMemory (eos::common::FileId::fileid_t fid, eos::common::FileSystem::fsid_t fsid);

  // ---------------------------------------------------------------------------
  //! Reset Disk Information
  // ---------------------------------------------------------------------------
  bool ResetDiskInformation (eos::common::FileSystem::fsid_t fsid);

  // ---------------------------------------------------------------------------
  //! Reset Mgm Information
  // ---------------------------------------------------------------------------
  bool ResetMgmInformation (eos::common::FileSystem::fsid_t fsid);

  // ---------------------------------------------------------------------------
  //! Update fmd from disk contents
  // ---------------------------------------------------------------------------
  bool UpdateFromDisk (eos::common::FileSystem::fsid_t fsid, eos::common::FileId::fileid_t fid, unsigned long long disksize, std::string diskchecksum, unsigned long checktime, bool filecxerror, bool blockcxerror, bool flaglayouterror);


  // ---------------------------------------------------------------------------
  //! Update fmd from mgm contents
  // ---------------------------------------------------------------------------
  bool UpdateFromMgm (eos::common::FileSystem::fsid_t fsid, eos::common::FileId::fileid_t fid, eos::common::FileId::fileid_t cid, eos::common::LayoutId::layoutid_t lid, unsigned long long mgmsize, std::string mgmchecksum, uid_t uid, gid_t gid, unsigned long long ctime, unsigned long long ctime_ns, unsigned long long mtime, unsigned long long mtime_ns, int layouterror, std::string locations);

  // ---------------------------------------------------------------------------
  //! Resync File meta data found under path
  // ---------------------------------------------------------------------------
  bool ResyncAllDisk (const char* path, eos::common::FileSystem::fsid_t fsid, bool flaglayouterror);

  // ---------------------------------------------------------------------------
  //! Resync a single entry from Disk
  // ---------------------------------------------------------------------------
  bool ResyncDisk (const char* fstpath, eos::common::FileSystem::fsid_t fsid, bool flaglayouterror, bool callautorepair=false);

  // ---------------------------------------------------------------------------
  //! Resync a single entry from Mgm
  // ---------------------------------------------------------------------------
  bool ResyncMgm (eos::common::FileSystem::fsid_t fsid, eos::common::FileId::fileid_t fid, const char* manager);

  // ---------------------------------------------------------------------------
  //! Resync all entries from Mgm
  // ---------------------------------------------------------------------------
  bool ResyncAllMgm (eos::common::FileSystem::fsid_t fsid, const char* manager);

  // ---------------------------------------------------------------------------
  //! Query list of fids
  // ---------------------------------------------------------------------------
  size_t Query (eos::common::FileSystem::fsid_t fsid, std::string query, std::vector<eos::common::FileId::fileid_t> &fidvector);

  // ---------------------------------------------------------------------------
  //! GetIncosistencyStatistics
  // ---------------------------------------------------------------------------
  bool GetInconsistencyStatistics (eos::common::FileSystem::fsid_t fsid, std::map<std::string, size_t> &statistics, std::map<std::string, std::set < eos::common::FileId::fileid_t> > &fidset);

  // ---------------------------------------------------------------------------
  //! Initialize the changelog hash
  // ---------------------------------------------------------------------------

  void
  Reset (eos::common::FileSystem::fsid_t fsid)
  {
    // you need to lock the RWMutex Mutex before calling this
    FmdSqliteMap[fsid].clear();
  }

  // ---------------------------------------------------------------------------
  //! Initialize the SQL DB
  // ---------------------------------------------------------------------------
  bool ResetDB (eos::common::FileSystem::fsid_t fsid);

  // ---------------------------------------------------------------------------
  //! Return Fmd from an mgm
  // ---------------------------------------------------------------------------
  int GetMgmFmd (const char* manager,
		 eos::common::FileId::fileid_t fid,
		 struct Fmd& fmd);

  // ---------------------------------------------------------------------------
  //! Comparison function for modification times
  // ---------------------------------------------------------------------------
  static int CompareMtime (const void* a, const void *b);

  // that is all we need for meta data handling

  // ---------------------------------------------------------------------------
  //! Hash map pointing from fsid to a map of file id to meta data
  // ---------------------------------------------------------------------------
  google::sparse_hash_map<eos::common::FileSystem::fsid_t, google::dense_hash_map<unsigned long long, struct Fmd > > FmdSqliteMap;

  // ---------------------------------------------------------------------------
  //! Hash map protecting each filesystem map in FmdSqliteMap
  // ---------------------------------------------------------------------------

  google::sparse_hash_map<eos::common::FileSystem::fsid_t, eos::common::RWMutex> FmdSqliteMutexMap;

  // ---------------------------------------------------------------------------
  //! Mutex protecting the previous map
  // ---------------------------------------------------------------------------
  eos::common::RWMutex FmdSqliteMutexMapMutex;

  inline void _FmdSqliteLock(const eos::common::FileSystem::fsid_t &fsid, bool write)
  {
    FmdSqliteMutexMapMutex.LockRead();
    if(FmdSqliteMutexMap.count(fsid))
    {
      if(write)
        FmdSqliteMutexMap[fsid].LockWrite();
      else
        FmdSqliteMutexMap[fsid].LockRead();
      FmdSqliteMutexMapMutex.UnLockRead();
    }
    else
    {
      FmdSqliteMutexMapMutex.UnLockRead();
      FmdSqliteMutexMapMutex.LockWrite();
      if(write)
        FmdSqliteMutexMap[fsid].LockWrite();
      else
        FmdSqliteMutexMap[fsid].LockRead();
      FmdSqliteMutexMapMutex.UnLockWrite();
    }
  }

  inline void _FmdSqliteUnLock(const eos::common::FileSystem::fsid_t &fsid, bool write)
  {
    FmdSqliteMutexMapMutex.LockRead();
    if(FmdSqliteMutexMap.count(fsid))
    {
      if(write)
        FmdSqliteMutexMap[fsid].UnLockWrite();
      else
        FmdSqliteMutexMap[fsid].UnLockRead();
      FmdSqliteMutexMapMutex.UnLockRead();
    }
    else
    {
      // this should NEVER happen as the entry should be in the map because mutex #i should have been locked at some point
      FmdSqliteMutexMapMutex.UnLockRead();
      FmdSqliteMutexMapMutex.LockWrite();
      if(write)
        FmdSqliteMutexMap[fsid].UnLockWrite();
      else
        FmdSqliteMutexMap[fsid].UnLockRead();
      FmdSqliteMutexMapMutex.UnLockWrite();
    }
  }

  inline void FmdSqliteLockRead(const eos::common::FileSystem::fsid_t &fsid) { _FmdSqliteLock(fsid, false); }
  inline void FmdSqliteLockWrite(const eos::common::FileSystem::fsid_t &fsid) { _FmdSqliteLock(fsid, true); }
  inline void FmdSqliteUnLockRead(const eos::common::FileSystem::fsid_t &fsid) { _FmdSqliteUnLock(fsid, false); }
  inline void FmdSqliteUnLockWrite(const eos::common::FileSystem::fsid_t &fsid) { _FmdSqliteUnLock(fsid, true); }


  // ---------------------------------------------------------------------------
  //! Create a new changelog filename in 'dir' (the fsid suffix is not added!)
  // ---------------------------------------------------------------------------

  const char*
  CreateDBFileName (const char* cldir, XrdOucString &clname)
  {
    clname = cldir;
    clname += "/";
    clname += "fmd";
    return clname.c_str();
  }

  // ---------------------------------------------------------------------------
  //! Constructor
  // ---------------------------------------------------------------------------

  FmdSqliteHandler ()
  {
    SetLogId("CommonFmdSqliteHandler");
  }

  // ---------------------------------------------------------------------------
  //! Destructor
  // ---------------------------------------------------------------------------

  ~FmdSqliteHandler ()
  {
    Shutdown();
  }

  // ---------------------------------------------------------------------------
  //! Shutdown
  // ---------------------------------------------------------------------------

  void
  Shutdown ()
  {
    // clean-up all open DB handles
    std::map<eos::common::FileSystem::fsid_t, sqlite3*>::const_iterator it;
    for (it = DB.begin(); it != DB.end(); it++)
    {
      ShutdownDB(it->first);
    }
    {
      // remove all
      eos::common::RWMutexWriteLock lock(Mutex);
      DB.clear();
    }
  }

private:
  bool isOpen;
  qr_result_t Qr;
  std::map<eos::common::FileSystem::fsid_t, sqlite3*> DB;
  std::map<eos::common::FileSystem::fsid_t, std::string> DBfilename;


  char* ErrMsg;
  static int CallBack (void * object, int argc, char **argv, char **ColName);
  static int ReadDBCallBack (void * object, int argc, char **argv, char **ColName);
  std::map<eos::common::FileSystem::fsid_t, bool> isDirty;
  std::map<eos::common::FileSystem::fsid_t, bool> stayDirty;

  std::map<eos::common::FileSystem::fsid_t, bool> isSyncing;
};


// ---------------------------------------------------------------------------
extern FmdSqliteHandler gFmdSqliteHandler;

class FmdSqliteReadLock
{
  eos::common::FileSystem::fsid_t mFsId;
public:
  FmdSqliteReadLock(const eos::common::FileSystem::fsid_t &fsid) : mFsId(fsid) {  gFmdSqliteHandler.FmdSqliteLockRead(mFsId); }
  ~FmdSqliteReadLock() {  gFmdSqliteHandler.FmdSqliteUnLockRead(mFsId); }
};

class FmdSqliteWriteLock
{
  eos::common::FileSystem::fsid_t mFsId;
public:
  FmdSqliteWriteLock(const eos::common::FileSystem::fsid_t &fsid) : mFsId(fsid) {  gFmdSqliteHandler.FmdSqliteLockWrite(mFsId); }
  ~FmdSqliteWriteLock() {  gFmdSqliteHandler.FmdSqliteUnLockWrite(mFsId); }
};

EOSFSTNAMESPACE_END

#endif
