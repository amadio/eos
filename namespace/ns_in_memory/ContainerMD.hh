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

//------------------------------------------------------------------------------
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Class representing the container metadata
//------------------------------------------------------------------------------

#ifndef __EOS_NS_CONTAINER_MD_HH__
#define __EOS_NS_CONTAINER_MD_HH__

#include "namespace/Namespace.hh"
#include "namespace/interface/IContainerMD.hh"
#include "namespace/interface/IFileMD.hh"
#include <stdint.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <google/sparse_hash_map>
#include <google/dense_hash_map>
#include <map>
#include <sys/time.h>

EOSNSNAMESPACE_BEGIN

//! Forward declaration
class IContainerMDSvc;
class IFileMDSvc;

//------------------------------------------------------------------------------
//! Class holding the metadata information concerning a single container
//------------------------------------------------------------------------------
class ContainerMD: public IContainerMD
{
public:
  //----------------------------------------------------------------------------
  // Type definitions
  //----------------------------------------------------------------------------
  typedef google::dense_hash_map< std::string, eos::IContainerMD::id_t >
  ContainerMap;
  typedef google::dense_hash_map< std::string, eos::IFileMD::id_t >
  FileMap;

  //----------------------------------------------------------------------------
  //! Constructor
  //!
  //! @param id container id
  //! @param file_svc file metadata service
  //! @param cont_svc container metadata service
  //----------------------------------------------------------------------------
  ContainerMD(id_t id, IFileMDSvc* file_svc, IContainerMDSvc* cont_svc);

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  ~ContainerMD();

  //----------------------------------------------------------------------------
  //! Virtual copy constructor
  //----------------------------------------------------------------------------
  virtual ContainerMD* clone() const;

  //----------------------------------------------------------------------------
  //! Copy constructor
  //----------------------------------------------------------------------------
  ContainerMD(const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Assignment operator
  //----------------------------------------------------------------------------
  ContainerMD& operator= (const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Children inheritance
  //!
  // @param other container from which to inherit children
  //----------------------------------------------------------------------------
  void InheritChildren(const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Add container
  //----------------------------------------------------------------------------
  virtual void addContainer(IContainerMD* container);

  //----------------------------------------------------------------------------
  //! Remove container
  //----------------------------------------------------------------------------
  void removeContainer(const std::string& name);

  //----------------------------------------------------------------------------
  //! Find sub container
  //----------------------------------------------------------------------------
  std::shared_ptr<IContainerMD> findContainer(const std::string& name);

  //----------------------------------------------------------------------------
  //! Get number of containers
  //----------------------------------------------------------------------------
  size_t getNumContainers()
  {
    return pSubContainers.size();
  }

  //----------------------------------------------------------------------------
  //! Add file
  //----------------------------------------------------------------------------
  virtual void addFile(IFileMD* file);

  //----------------------------------------------------------------------------
  //! Remove file
  //----------------------------------------------------------------------------
  void removeFile(const std::string& name);

  //----------------------------------------------------------------------------
  //! Find file
  //----------------------------------------------------------------------------
  std::shared_ptr<IFileMD> findFile(const std::string& name);

  //----------------------------------------------------------------------------
  //! Get number of files
  //----------------------------------------------------------------------------
  size_t getNumFiles()
  {
    return pFiles.size();
  }

  //----------------------------------------------------------------------------
  //! Get container id
  //----------------------------------------------------------------------------
  id_t getId() const
  {
    return pId;
  }

  //----------------------------------------------------------------------------
  //! Get parent id
  //----------------------------------------------------------------------------
  id_t getParentId() const
  {
    return pParentId;
  }

  //----------------------------------------------------------------------------
  //! Set parent id
  //----------------------------------------------------------------------------
  void setParentId(id_t parentId)
  {
    pParentId = parentId;
  }

  //----------------------------------------------------------------------------
  //! Get the flags
  //----------------------------------------------------------------------------
  uint16_t& getFlags()
  {
    return pFlags;
  }

  //----------------------------------------------------------------------------
  //! Get the flags
  //----------------------------------------------------------------------------
  uint16_t getFlags() const
  {
    return pFlags;
  }

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setCTime(ctime_t ctime)
  {
    pCTime.tv_sec = ctime.tv_sec;
    pCTime.tv_nsec = ctime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setCTimeNow()
  {
#ifdef __APPLE__
    struct timeval tv;
    gettimeofday(&tv, 0);
    pCTime.tv_sec = tv.tv_sec;
    pCTime.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &pCTime);
#endif
  }

  //----------------------------------------------------------------------------
  //! Get creation time
  //----------------------------------------------------------------------------
  void getCTime(ctime_t& ctime) const
  {
    ctime.tv_sec = pCTime.tv_sec;
    ctime.tv_nsec = pCTime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setMTime(mtime_t mtime);

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setMTimeNow();

  //----------------------------------------------------------------------------
  //! Trigger an mtime change event
  //----------------------------------------------------------------------------
  void notifyMTimeChange(IContainerMDSvc* containerMDSvc);

  //----------------------------------------------------------------------------
  //! Get modification time
  //----------------------------------------------------------------------------
  void getMTime(mtime_t& mtime) const
  {
    mtime.tv_sec = pMTime.tv_sec;
    mtime.tv_nsec = pMTime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Set propagated modification time (if newer)
  //----------------------------------------------------------------------------
  bool setTMTime(tmtime_t tmtime)
  {
    if ((tmtime.tv_sec > pTMTime.tv_sec) ||
        ((tmtime.tv_sec == pTMTime.tv_sec) &&
         (tmtime.tv_nsec > pTMTime.tv_nsec))) {
      pTMTime.tv_sec = tmtime.tv_sec;
      pTMTime.tv_nsec = tmtime.tv_nsec;
      return true;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  //! Set propagated modification time to now
  //----------------------------------------------------------------------------
  void setTMTimeNow()
  {
    tmtime_t tmtime;
#ifdef __APPLE__
    struct timeval tv;
    gettimeofday(&tv, 0);
    tmtime..tv_sec = tv.tv_sec;
    tmtime.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &tmtime);
#endif
    setTMTime(tmtime);
    return;
  }

  //----------------------------------------------------------------------------
  //! Get creation time
  //----------------------------------------------------------------------------
  void getTMTime(tmtime_t& tmtime) const
  {
    tmtime.tv_sec = pTMTime.tv_sec;
    tmtime.tv_nsec = pTMTime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Get tree size
  //----------------------------------------------------------------------------
  uint64_t getTreeSize() const
  {
    return pTreeSize;
  }

  //----------------------------------------------------------------------------
  //! Set tree size
  //----------------------------------------------------------------------------
  void setTreeSize(uint64_t treesize)
  {
    pTreeSize = treesize;
  }

  //----------------------------------------------------------------------------
  //! Add to tree size
  //----------------------------------------------------------------------------
  uint64_t addTreeSize(uint64_t addsize)
  {
    pTreeSize += addsize;
    return pTreeSize;
  }

  //----------------------------------------------------------------------------
  //! Remove from tree size
  //----------------------------------------------------------------------------
  uint64_t removeTreeSize(uint64_t removesize)
  {
    pTreeSize -= removesize;
    return pTreeSize;
  }

  //----------------------------------------------------------------------------
  //! Get name
  //----------------------------------------------------------------------------
  const std::string& getName() const
  {
    return pName;
  }

  //----------------------------------------------------------------------------
  //! Set name
  //----------------------------------------------------------------------------
  void setName(const std::string& name)
  {
    pName = name;
  }

  //----------------------------------------------------------------------------
  //! Get uid
  //----------------------------------------------------------------------------
  uid_t getCUid() const
  {
    return pCUid;
  }

  //----------------------------------------------------------------------------
  //! Set uid
  //----------------------------------------------------------------------------
  void setCUid(uid_t uid)
  {
    pCUid = uid;
  }

  //----------------------------------------------------------------------------
  //! Get gid
  //----------------------------------------------------------------------------
  gid_t getCGid() const
  {
    return pCGid;
  }

  //----------------------------------------------------------------------------
  //! Set gid
  //----------------------------------------------------------------------------
  void setCGid(gid_t gid)
  {
    pCGid = gid;
  }

  //----------------------------------------------------------------------------
  //! Get mode
  //----------------------------------------------------------------------------
  mode_t getMode() const
  {
    return pMode;
  }

  //----------------------------------------------------------------------------
  //! Set mode
  //----------------------------------------------------------------------------
  void setMode(mode_t mode)
  {
    pMode = mode;
  }

  //----------------------------------------------------------------------------
  //! Get ACL Id
  //----------------------------------------------------------------------------
  uint16_t getACLId() const
  {
    return pACLId;
  }

  //----------------------------------------------------------------------------
  //! Set ACL Id
  //----------------------------------------------------------------------------
  void setACLId(uint16_t ACLId)
  {
    pACLId = ACLId;
  }

  //----------------------------------------------------------------------------
  //! Add extended attribute
  //----------------------------------------------------------------------------
  void setAttribute(const std::string& name, const std::string& value)
  {
    pXAttrs[name] = value;
  }

  //----------------------------------------------------------------------------
  //! Remove attribute
  //----------------------------------------------------------------------------
  void removeAttribute(const std::string& name)
  {
    XAttrMap::iterator it = pXAttrs.find(name);

    if (it != pXAttrs.end()) {
      pXAttrs.erase(it);
    }
  }

  //----------------------------------------------------------------------------
  //! Check if the attribute exist
  //----------------------------------------------------------------------------
  bool hasAttribute(const std::string& name) const
  {
    return pXAttrs.find(name) != pXAttrs.end();
  }

  //----------------------------------------------------------------------------
  //! Return number of attributes
  //----------------------------------------------------------------------------
  size_t numAttributes() const
  {
    return pXAttrs.size();
  }

  //----------------------------------------------------------------------------
  // Get the attribute
  //----------------------------------------------------------------------------
  std::string getAttribute(const std::string& name) const
  {
    XAttrMap::const_iterator it = pXAttrs.find(name);

    if (it == pXAttrs.end()) {
      MDException e(ENOENT);
      e.getMessage() << "Attribute: " << name << " not found";
      throw e;
    }

    return it->second;
  }

  //----------------------------------------------------------------------------
  //! Get attribute begin iterator
  //----------------------------------------------------------------------------
  XAttrMap::iterator attributesBegin()
  {
    return pXAttrs.begin();
  }

  //----------------------------------------------------------------------------
  //! Get the attribute end iterator
  //----------------------------------------------------------------------------
  XAttrMap::iterator attributesEnd()
  {
    return pXAttrs.end();
  }

  //------------------------------------------------------------------------------
  //! Check the access permissions
  //!
  //! @return true if all the requested rights are granted, false otherwise
  //------------------------------------------------------------------------------
  bool access(uid_t uid, gid_t gid, int flags = 0);

  //----------------------------------------------------------------------------
  //! Clean up the entire contents for the container. Delete files and
  //! containers recurssively
  //----------------------------------------------------------------------------
  void cleanUp();

  //----------------------------------------------------------------------------
  //! Get set of file names contained in the current object
  //!
  //! @return set of file names
  //----------------------------------------------------------------------------
  std::set<std::string> getNameFiles() const;

  //----------------------------------------------------------------------------
  //! Get set of subcontainer names contained in the current object
  //!
  //! @return set of subcontainer names
  //----------------------------------------------------------------------------
  std::set<std::string> getNameContainers() const;

  //----------------------------------------------------------------------------
  //! Serialize the object to a buffer
  //----------------------------------------------------------------------------
  void serialize(Buffer& buffer);

  //----------------------------------------------------------------------------
  //! Deserialize the class to a buffer
  //----------------------------------------------------------------------------
  void deserialize(Buffer& buffer);

protected:
  id_t         pId;
  id_t         pParentId;
  uint16_t     pFlags;
  ctime_t      pCTime;
  std::string  pName;
  uid_t        pCUid;
  gid_t        pCGid;
  mode_t       pMode;
  uint16_t     pACLId;
  XAttrMap     pXAttrs;
  ContainerMap pSubContainers;
  FileMap      pFiles;

private:
  // Non-presistent data members
  mtime_t      pMTime;
  tmtime_t     pTMTime;
  uint64_t     pTreeSize;

  IFileMDSvc* pFileSvc; ///< File metadata service
  IContainerMDSvc* pContSvc; ///< Container metadata service
};

EOSNSNAMESPACE_END

#endif // __EOS_NS_CONTAINER_MD_HH__
