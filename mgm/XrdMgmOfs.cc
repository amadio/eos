// ----------------------------------------------------------------------
// File: XrdMgmOfs.cc
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
#include "common/Mapping.hh"
#include "common/FileId.hh"
#include "common/LayoutId.hh"
#include "common/Path.hh"
#include "common/Timing.hh"
#include "common/StringConversion.hh"
#include "common/SecEntity.hh"
#include "common/StackTrace.hh"
#include "common/SymKeys.hh"
#include "common/http/OwnCloud.hh"
#include "namespace/Constants.hh"
#include "mgm/Access.hh"
#include "mgm/FileSystem.hh"
#include "mgm/XrdMgmOfs.hh"
#include "mgm/XrdMgmOfsDirectory.hh"
#include "mgm/XrdMgmOfsFile.hh"
#include "mgm/XrdMgmOfsTrace.hh"
#include "mgm/XrdMgmOfsSecurity.hh"
#include "mgm/Policy.hh"
#include "mgm/Quota.hh"
#include "mgm/Acl.hh"
#include "mgm/txengine/TransferEngine.hh"
#include "mgm/Recycle.hh"
#include "mgm/Macros.hh"
/*----------------------------------------------------------------------------*/
#include "XrdVersion.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsAio.hh"
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
/*----------------------------------------------------------------------------*/

#ifdef __APPLE__
#define ECOMM 70
#endif

#ifndef S_IAMB
#define S_IAMB  0x1FF
#endif


/*----------------------------------------------------------------------------*/
XrdSysError gMgmOfsEroute (0);
XrdSysError *XrdMgmOfs::eDest;
XrdOucTrace
gMgmOfsTrace (&gMgmOfsEroute);

const char* XrdMgmOfs::gNameSpaceState[] = {"down", "booting", "booted", "failed", "compacting"};

XrdMgmOfs* gOFS = 0;

/*----------------------------------------------------------------------------*/
extern "C"
XrdSfsFileSystem *
XrdSfsGetFileSystem (XrdSfsFileSystem *native_fs,
                     XrdSysLogger *lp,
                     const char *configfn)
/*----------------------------------------------------------------------------*/
/*
 * The Filesystem Plugin factory function
 *
 * @param native_fs (not used)
 * @param lp the logger object
 * @param configfn the configuration file name
 * It configures and returns our MgmOfs object
 */
/*----------------------------------------------------------------------------*/
{
  gMgmOfsEroute.SetPrefix("mgmofs_");
  gMgmOfsEroute.logger(lp);

  static XrdMgmOfs myFS(&gMgmOfsEroute);

  XrdOucString vs = "MgmOfs (meta data redirector) ";
  vs += VERSION;
  gMgmOfsEroute.Say("++++++ (c) 2015 CERN/IT-DSS ", vs.c_str());

  // ---------------------------------------------------------------------------
  // Initialize the subsystems
  // ---------------------------------------------------------------------------
  if (!myFS.Init(gMgmOfsEroute)) return 0;


  // ---------------------------------------------------------------------------
  // Disable XRootd log rotation
  // ---------------------------------------------------------------------------
  lp->setRotate(0);
  gOFS = &myFS;

  // by default enable stalling and redirection
  gOFS->IsStall = true;
  gOFS->IsRedirect = true;

  myFS.ConfigFN = (configfn && *configfn ? strdup(configfn) : 0);
  if (myFS.Configure(gMgmOfsEroute)) return 0;


  // Initialize authorization module ServerAcc
  gOFS->CapabilityEngine = (XrdCapability*) XrdAccAuthorizeObject(lp,
                                                                  configfn, 0);
  if (!gOFS->CapabilityEngine)
  {
    return 0;
  }

  return gOFS;
}

/******************************************************************************/
/******************************************************************************/
/* MGM Meta Data Interface                                                    */
/******************************************************************************/
/******************************************************************************/

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
XrdMgmOfs::XrdMgmOfs (XrdSysError *ep):
    deletion_tid(0), stats_tid(0), fsconfiglistener_tid(0),
    mFstGwHost(""), mFstGwPort(0), mSubmitterTid(0)
{
  eDest = ep;
  ConfigFN = 0;
  eos::common::LogId();
  eos::common::LogId::SetSingleShotLogId();
  mZmqContext = new zmq::context_t(1);
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
XrdMgmOfs::~XrdMgmOfs()
{
  StopArchiveSubmitter();
  delete mZmqContext;
}

//------------------------------------------------------------------------------
// This is just kept to be compatible with standard OFS plugins, but it is not
// used for the moment.
//------------------------------------------------------------------------------
bool
XrdMgmOfs::Init (XrdSysError &ep)
{
  return true;
}

/*----------------------------------------------------------------------------*/
XrdSfsDirectory *
XrdMgmOfs::newDir (char *user, int MonID)
/*----------------------------------------------------------------------------*/
/*
 * @brief return a MGM directory object
 * @param user user-name
 * @param MonID monitor ID
 */
/*----------------------------------------------------------------------------*/
{
  return (XrdSfsDirectory *)new XrdMgmOfsDirectory(user, MonID);
}

/*----------------------------------------------------------------------------*/
XrdSfsFile *
XrdMgmOfs::newFile (char *user, int MonID)
/*----------------------------------------------------------------------------*/
/*
 * @brief return a MGM file object
 * @param user user-name
 * @param MonID monitor ID
 */
/*----------------------------------------------------------------------------*/
{
  return (XrdSfsFile *)new XrdMgmOfsFile(user, MonID);
}


/*----------------------------------------------------------------------------*/
/*
 * Implementation Source Code Includes
 */
/*----------------------------------------------------------------------------*/
#include "XrdMgmOfs/Access.cc"
#include "XrdMgmOfs/Attr.cc"
#include "XrdMgmOfs/Auth.cc"
#include "XrdMgmOfs/Chksum.cc"
#include "XrdMgmOfs/Chmod.cc"
#include "XrdMgmOfs/Chown.cc"
#include "XrdMgmOfs/DeleteExternal.cc"
#include "XrdMgmOfs/Exists.cc"
#include "XrdMgmOfs/Find.cc"
#include "XrdMgmOfs/FsConfigListener.cc"
#include "XrdMgmOfs/Fsctl.cc"
#include "XrdMgmOfs/Link.cc"
#include "XrdMgmOfs/Merge.cc"
#include "XrdMgmOfs/Mkdir.cc"
#include "XrdMgmOfs/PathMap.cc"
#include "XrdMgmOfs/Remdir.cc"
#include "XrdMgmOfs/Rename.cc"
#include "XrdMgmOfs/Rm.cc"
#include "XrdMgmOfs/SendResync.cc"
#include "XrdMgmOfs/SharedPath.cc"
#include "XrdMgmOfs/ShouldRedirect.cc"
#include "XrdMgmOfs/ShouldStall.cc"
#include "XrdMgmOfs/Shutdown.cc"
#include "XrdMgmOfs/Stacktrace.cc"
#include "XrdMgmOfs/Stat.cc"
#include "XrdMgmOfs/Stripes.cc"
#include "XrdMgmOfs/Touch.cc"
#include "XrdMgmOfs/Utimes.cc"
#include "XrdMgmOfs/Version.cc"

/*----------------------------------------------------------------------------*/
bool
XrdMgmOfs::HasStall (const char* path,
                     const char* rule,
                     int &stalltime,
                     XrdOucString &stallmsg)
/*----------------------------------------------------------------------------*/
/* @brief Test if there is stall configured for the given rule
 *
 * @param path the path where the rule should be checked (currently unused)
 * @param rule the rule to check e.g. rule = "ENOENT:*" meaning we send a stall if an entry is missing
 * @param stalltime returns the configured time to stall
 * @param stallmsg returns the message displayed to the client during a stall
 * @return true if there is a stall configured otherwise false
 *
 * The interface is generic to check for individual paths, but currently we just
 * implemented global rules for any paths. See Access.cc for details.
 */
/*----------------------------------------------------------------------------*/
{
  if (!rule)
    return false;
  eos::common::RWMutexReadLock lock(Access::gAccessMutex);
  if (Access::gStallRules.count(std::string(rule)))
  {
    stalltime = atoi(Access::gStallRules[std::string(rule)].c_str());
    stallmsg = "Attention: you are currently hold in this instance and each request is stalled for ";
    stallmsg += (int) stalltime;
    stallmsg += " seconds after an errno of type: ";
    stallmsg += rule;
    eos_static_info("info=\"stalling\" path=\"%s\" errno=\"%s\"", path, rule);
    return true;
  }
  else
  {
    return false;
  }
}

/*----------------------------------------------------------------------------*/
bool
XrdMgmOfs::HasRedirect (const char* path,
                        const char* rule,
                        XrdOucString &host,
                        int &port)
/*----------------------------------------------------------------------------*/
/* @brief Test if there is redirect configured for a given rule
 *
 * @param path the path where the rule should be checked (currently unused)
 * @param rule the rule to check e.g. rule = "ENOENT:*" meaning we send a redirect if an entry is missing
 * @param host returns the redirection target host
 * @param port returns the redirection target port
 * @return true if there is a redirection configured otherwise false
 *
 * The interface is generic to check for individual paths, but currently we just
 * implemented global rules for any paths. See Access.cc for details.
 */
/*----------------------------------------------------------------------------*/
{
  if (!rule)
    return false;

  std::string srule = rule;
  eos::common::RWMutexReadLock lock(Access::gAccessMutex);
  if (Access::gRedirectionRules.count(srule))
  {
    std::string delimiter = ":";
    std::vector<std::string> tokens;
    eos::common::StringConversion::Tokenize(Access::gRedirectionRules[srule],
                                            tokens, delimiter);
    if (tokens.size() == 1)
    {
      host = tokens[0].c_str();
      port = 1094;
    }
    else
    {
      host = tokens[0].c_str();
      port = atoi(tokens[1].c_str());
      if (port == 0)
        port = 1094;
    }

    eos_static_info("info=\"redirect\" path=\"%s\" host=%s port=%d errno=%s",
                    path, host.c_str(), port, rule);

    if (srule == "ENONET")
    {
      gOFS->MgmStats.Add("RedirectENONET", 0, 0, 1);
    }
    if (srule == "ENOENT")
    {
      gOFS->MgmStats.Add("redirectENOENT", 0, 0, 1);
    }

    return true;
  }
  else
  {
    return false;
  }
}

/*----------------------------------------------------------------------------*/
const char *
XrdMgmOfs::getVersion ()
/*----------------------------------------------------------------------------*/
/*
 * Return the version of the MGM software
 *
 * @return return a version string
 */
/*----------------------------------------------------------------------------*/
{

  static XrdOucString FullVersion = XrdVERSION;
  FullVersion += " MgmOfs ";
  FullVersion += VERSION;
  return FullVersion.c_str();
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfs::prepare (XrdSfsPrep &pargs,
                    XrdOucErrInfo &error,
                    const XrdSecEntity * client)
/*----------------------------------------------------------------------------*/
/*
 * Prepare a file (EOS does nothing, only stall/redirect if configured)
 *
 * @return always SFS_OK
 */
/*----------------------------------------------------------------------------*/
{
  //  static const char *epname = "prepare";

  const char *tident = error.getErrUser();

  eos::common::Mapping::VirtualIdentity vid;

  EXEC_TIMING_BEGIN("IdMap");
  eos::common::Mapping::IdMap(client, 0, tident, vid);
  EXEC_TIMING_END("IdMap");

  gOFS->MgmStats.Add("IdMap", vid.uid, vid.gid, 1);

  ACCESSMODE_R;
  MAYSTALL;
  MAYREDIRECT;

  return SFS_OK;
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfs::truncate (const char*,
                     XrdSfsFileOffset,
                     XrdOucErrInfo& error,
                     const XrdSecEntity* client,
                     const char* path)
/*----------------------------------------------------------------------------*/
/*
 * @brief truncate a file ( not supported in EOS, only via the file interface )
 * @return SFS_ERROR and EOPNOTSUPP
 */
/*----------------------------------------------------------------------------*/
{

  static const char *epname = "truncate";
  const char *tident = error.getErrUser();
  // use a thread private vid
  eos::common::Mapping::VirtualIdentity vid;

  EXEC_TIMING_BEGIN("IdMap");
  eos::common::Mapping::IdMap(client, 0, tident, vid);
  EXEC_TIMING_END("IdMap");

  gOFS->MgmStats.Add("IdMap", vid.uid, vid.gid, 1);

  ACCESSMODE_W;
  MAYSTALL;
  MAYREDIRECT;

  gOFS->MgmStats.Add("Truncate", vid.uid, vid.gid, 1);
  return Emsg(epname, error, EOPNOTSUPP, "truncate", path);
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfs::Emsg (const char *pfx,
                 XrdOucErrInfo &einfo,
                 int ecode,
                 const char *op,
                 const char *target)
/*----------------------------------------------------------------------------*/
/*
 * @brief create an error message
 *
 * @param pfx message prefix value
 * @param einfo error text/code object
 * @param ecode error code
 * @param op name of the operation performed
 * @param target target of the operation e.g. file name etc.
 *
 * @return SFS_ERROR in all cases
 *
 * This routines prints also an error message into the EOS log if it was not
 * due to a stat call or the error codes EIDRM or ENODATA
 */
/*----------------------------------------------------------------------------*/
{
  char *etext, buffer[4096], unkbuff[64];

  // Get the reason for the error
  //
  if (ecode < 0) ecode = -ecode;
  if (!(etext = strerror(ecode)))
  {
    sprintf(unkbuff, "reason unknown (%d)", ecode);
    etext = unkbuff;
  }

  // Format the error message
  //


  snprintf(buffer, sizeof (buffer), "Unable to %s %s; %s", op, target, etext);

  if ((ecode == EIDRM) || (ecode == ENODATA))
  {
    eos_debug("Unable to %s %s; %s", op, target, etext);
  }
  else
  {
    if ((!strcmp(op, "stat")) || ( (!strcmp(pfx,"attr_get") || (!strcmp(pfx,"attr_ls")) ) && (ecode == ENOENT) ) )
    {
      eos_debug("Unable to %s %s; %s", op, target, etext);
    }
    else
    {
      eos_err("Unable to %s %s; %s", op, target, etext);
    }
  }

  // ---------------------------------------------------------------------------
  // Print it out if debugging is enabled
  // ---------------------------------------------------------------------------
#ifndef NODEBUG
  //   XrdMgmOfs::eDest->Emsg(pfx, buffer);
#endif

  // ---------------------------------------------------------------------------
  // Place the error message in the error object and return
  // ---------------------------------------------------------------------------
  einfo.setErrInfo(ecode, buffer);

  return SFS_ERROR;
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsDirectory::Emsg (const char *pfx,
                          XrdOucErrInfo &einfo,
                          int ecode,
                          const char *op,
                          const char *target)
/*----------------------------------------------------------------------------*/
/*
 * @brief create an error message for a directory object
 *
 * @param pfx message prefix value
 * @param einfo error text/code object
 * @param ecode error code
 * @param op name of the operation performed
 * @param target target of the operation e.g. file name etc.
 *
 * @return SFS_ERROR in all cases
 *
 * This routines prints also an error message into the EOS log.
 */
/*----------------------------------------------------------------------------*/
{
  char *etext, buffer[4096], unkbuff[64];

  // ---------------------------------------------------------------------------
  // Get the reason for the error
  // ---------------------------------------------------------------------------
  if (ecode < 0) ecode = -ecode;
  if (!(etext = strerror(ecode)))
  {

    sprintf(unkbuff, "reason unknown (%d)", ecode);
    etext = unkbuff;
  }

  // ---------------------------------------------------------------------------
  // Format the error message
  // ---------------------------------------------------------------------------
  snprintf(buffer, sizeof (buffer), "Unable to %s %s; %s", op, target, etext);

  if (ecode == ENOENT)
    eos_debug("Unable to %s %s; %s", op, target, etext);
  else
    eos_err("Unable to %s %s; %s", op, target, etext);
  // ---------------------------------------------------------------------------
  // Print it out if debugging is enabled
  // ---------------------------------------------------------------------------
#ifndef NODEBUG
  //   XrdMgmOfs::eDest->Emsg(pfx, buffer);
#endif

  // ---------------------------------------------------------------------------
  // Place the error message in the error object and return
  // ---------------------------------------------------------------------------
  einfo.setErrInfo(ecode, buffer);

  return SFS_ERROR;
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::Emsg (const char *pfx,
                     XrdOucErrInfo &einfo,
                     int ecode,
                     const char *op,
                     const char *target)
/*----------------------------------------------------------------------------*/
/*
 * @brief create an error message for a file object
 *
 * @param pfx message prefix value
 * @param einfo error text/code object
 * @param ecode error code
 * @param op name of the operation performed
 * @param target target of the operation e.g. file name etc.
 *
 * @return SFS_ERROR in all cases
 *
 * This routines prints also an error message into the EOS log.
 */
/*----------------------------------------------------------------------------*/
{
  char *etext, buffer[4096], unkbuff[64];

  // ---------------------------------------------------------------------------
  // Get the reason for the error
  // ---------------------------------------------------------------------------
  if (ecode < 0) ecode = -ecode;

  // we rewrite this errno, because xrd client translates this into IO error
  if (ecode == EDQUOT)
    ecode = ENOSPC;

  if (!(etext = strerror(ecode)))
  {

    sprintf(unkbuff, "reason unknown (%d)", ecode);
    etext = unkbuff;
  }

  // ---------------------------------------------------------------------------
  // Format the error message
  // ---------------------------------------------------------------------------
  snprintf(buffer, sizeof (buffer), "Unable to %s %s; %s", op, target, etext);

  eos_err("Unable to %s %s; %s", op, target, etext);

  // ---------------------------------------------------------------------------
  // Print it out if debugging is enabled
  // ---------------------------------------------------------------------------
#ifndef NODEBUG
  //   XrdMgmOfs::eDest->Emsg(pfx, buffer);
#endif

  // ---------------------------------------------------------------------------
  // Place the error message in the error object and return
  // ---------------------------------------------------------------------------
  einfo.setErrInfo(ecode, buffer);

  return SFS_ERROR;
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfs::Stall (XrdOucErrInfo &error,
                  int stime,
                  const char *msg)
/*----------------------------------------------------------------------------*/
/*
 * @brief send a stall response to the client
 *
 * @param error error object with text/code
 * @param stime seconds to stall
 * @param msg message for the client
 */
/*----------------------------------------------------------------------------*/
{
  XrdOucString smessage = msg;
  smessage += "; come back in ";
  smessage += stime;
  smessage += " seconds!";

  EPNAME("Stall");
  const char *tident = error.getErrUser();

  ZTRACE(delay, "Stall " << stime << ": " << smessage.c_str());

  // ---------------------------------------------------------------------------
  // Place the error message in the error object and return
  // ---------------------------------------------------------------------------
  error.setErrInfo(0, smessage.c_str());

  // ---------------------------------------------------------------------------
  // All done
  // ---------------------------------------------------------------------------
  return stime;
}


/*----------------------------------------------------------------------------*/
int
XrdMgmOfs::Redirect (XrdOucErrInfo &error,
                     const char* host,
                     int &port)
/*----------------------------------------------------------------------------*/
/*
 * @brief send a redirect response to the client
 *
 * @param error error object with text/code
 * @param host redirection target host
 * @param port redirection target port
 */
/*----------------------------------------------------------------------------*/
{

  EPNAME("Redirect");
  const char *tident = error.getErrUser();

  ZTRACE(delay, "Redirect " << host << ":" << port);

  // ---------------------------------------------------------------------------
  // Place the error message in the error object and return
  // ---------------------------------------------------------------------------
  error.setErrInfo(port, host);

  // ---------------------------------------------------------------------------
  // All done
  // ---------------------------------------------------------------------------
  return SFS_REDIRECT;
}


/*----------------------------------------------------------------------------*/
void*
XrdMgmOfs::StartMgmStats (void *pp)
{

  XrdMgmOfs* ofs = (XrdMgmOfs*) pp;
  ofs->MgmStats.Circulate();
  return 0;
}

/*----------------------------------------------------------------------------*/
void*
XrdMgmOfs::StartMgmFsConfigListener (void *pp)
{

  XrdMgmOfs* ofs = (XrdMgmOfs*) pp;
  ofs->FsConfigListener();
  return 0;
}

//------------------------------------------------------------------------------
// Static method to start a thread that will queue, build and submit backup
// operations to the archiver daemon.
//------------------------------------------------------------------------------
void*
XrdMgmOfs::StartArchiveSubmitter(void* arg)
{
  return reinterpret_cast<XrdMgmOfs*>(arg)->ArchiveSubmitter();
}

//------------------------------------------------------------------------------
// Method to stop the submitter thread
//------------------------------------------------------------------------------
void
XrdMgmOfs::StopArchiveSubmitter()
{
  XrdSysThread::Cancel(mSubmitterTid);
  XrdSysThread::Join(mSubmitterTid, NULL);
}

//------------------------------------------------------------------------------
// Implementation of the archive/backup sumitter thread
//------------------------------------------------------------------------------
void*
XrdMgmOfs::ArchiveSubmitter()
{
  ProcCommand pcmd;
  XrdSysTimer timer;
  std::string job_opaque;
  XrdOucString std_out, std_err;
  int max, running, pending;
  eos::common::Mapping::VirtualIdentity root_vid;
  eos::common::Mapping::Root(root_vid);
  eos_debug("msg=\"starting archive/backup submitter thread\"");
  std::ostringstream cmd_json;
  cmd_json << "{\"cmd\": \"stats\", "
           << "\"opt\": \"\", "
           << "\"uid\": \"0\", "
           << "\"gid\": \"0\" }";

  while (1)
  {
    XrdSysThread::SetCancelOff();
    {
      XrdSysMutexHelper lock(mJobsQMutex);

      if (!mPendingBkps.empty())
      {
        // Check if archiver has slots available
        if (!pcmd.ArchiveExecuteCmd(cmd_json.str()))
        {
          std_out.resize(0);
          std_err.resize(0);
          pcmd.AddOutput(std_out, std_err);

          if ((sscanf(std_out.c_str(), "max=%i running=%i pending=%i",
                      &max, &running, &pending) == 3))
          {
            while ((running + pending < max) && !mPendingBkps.empty())
            {
              running++;
              job_opaque = mPendingBkps.back();
              mPendingBkps.pop_back();
              job_opaque += "&mgm.backup.create=1";

              if (pcmd.open("/proc/admin", job_opaque.c_str(), root_vid, 0))
              {
                pcmd.AddOutput(std_out, std_err);
                eos_err("failed backup, msg=\"%s\"", std_err.c_str());
              }
            }
          }
        }
        else
        {
          eos_err("failed to send stats command to archive daemon");
        }
      }
    }

    XrdSysThread::SetCancelOn();
    timer.Wait(5000);
  }

  return 0;
}

//------------------------------------------------------------------------------
// Submit backup job
//------------------------------------------------------------------------------
bool
XrdMgmOfs::SubmitBackupJob(const std::string& job_opaque)
{
  XrdSysMutexHelper lock(mJobsQMutex);
  auto it = std::find(mPendingBkps.begin(), mPendingBkps.end(), job_opaque);

  if (it == mPendingBkps.end())
  {
    mPendingBkps.push_front(job_opaque);
    return true;
  }

  return false;
}

//------------------------------------------------------------------------------
// Get vector of pending backups
//------------------------------------------------------------------------------
std::vector<ProcCommand::ArchDirStatus>
XrdMgmOfs::GetPendingBkps()
{
  std::vector<ProcCommand::ArchDirStatus> bkps;
  XrdSysMutexHelper lock(mJobsQMutex);

  for (auto it = mPendingBkps.begin(); it != mPendingBkps.end(); ++it)
  {
    XrdOucEnv opaque(it->c_str());
    bkps.emplace_back("N/A", "N/A", opaque.Get("mgm.backup.dst"), "backup",
                      "pending at MGM");
  }

  return bkps;
}


//------------------------------------------------------------------------------                                               
// Cast a change message to all fusex clients                                                                                 
//------------------------------------------------------------------------------                                               
void 
XrdMgmOfs::FuseXCast(uint64_t inode)
{
  gOFS->zMQ->gFuseServer.Cap().BroadcastReleaseFromExternal(inode);
}
