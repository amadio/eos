/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2016 CERN/Switzerland                                  *
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
//! @author Elvin-Alin Sindrilaru <esindril@cern.ch>
//! @brief Class representing the container metadata
//------------------------------------------------------------------------------

#ifndef __EOS_NS_CONTAINER_MD_HH__
#define __EOS_NS_CONTAINER_MD_HH__

#include "namespace/interface/IContainerMD.hh"
#include "namespace/interface/IFileMD.hh"
#include "namespace/ns_quarkdb/BackendClient.hh"
#include <string>
#include <sys/time.h>

EOSNSNAMESPACE_BEGIN

class IContainerMDSvc;
class IFileMDSvc;

//------------------------------------------------------------------------------
//! Class holding the metadata information concerning a single container
//------------------------------------------------------------------------------
class ContainerMD : public IContainerMD
{
public:
  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  ContainerMD(id_t id, IFileMDSvc* file_svc, IContainerMDSvc* cont_svc);

  //----------------------------------------------------------------------------
  //! Desstructor
  //----------------------------------------------------------------------------
  virtual ~ContainerMD();

  //----------------------------------------------------------------------------
  //! Copy constructor
  //----------------------------------------------------------------------------
  ContainerMD(const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Virtual copy constructor
  //----------------------------------------------------------------------------
  virtual ContainerMD* clone() const;

  //----------------------------------------------------------------------------
  //! Assignment operator
  //----------------------------------------------------------------------------
  ContainerMD& operator=(const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Add container
  //----------------------------------------------------------------------------
  void addContainer(IContainerMD* container);

  //----------------------------------------------------------------------------
  //! Remove container
  //----------------------------------------------------------------------------
  void removeContainer(const std::string& name);

  //----------------------------------------------------------------------------
  //! Find subcontainer
  //----------------------------------------------------------------------------
  std::shared_ptr<IContainerMD> findContainer(const std::string& name);

  //----------------------------------------------------------------------------
  //! Get number of containers
  //----------------------------------------------------------------------------
  size_t getNumContainers();

  //----------------------------------------------------------------------------
  //! Add file
  //----------------------------------------------------------------------------
  void addFile(IFileMD* file);

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
  size_t getNumFiles();

  //----------------------------------------------------------------------------
  //! Get container id
  //----------------------------------------------------------------------------
  inline id_t
  getId() const
  {
    return pId;
  }

  //----------------------------------------------------------------------------
  //! Get parent id
  //----------------------------------------------------------------------------
  inline id_t
  getParentId() const
  {
    return pParentId;
  }

  //----------------------------------------------------------------------------
  //! Set parent id
  //----------------------------------------------------------------------------
  void
  setParentId(id_t parentId)
  {
    pParentId = parentId;
  }

  //----------------------------------------------------------------------------
  //! Get the flags
  //----------------------------------------------------------------------------
  uint16_t&
  getFlags()
  {
    return pFlags;
  }

  //----------------------------------------------------------------------------
  //! Get the flags
  //----------------------------------------------------------------------------
  inline uint16_t
  getFlags() const
  {
    return pFlags;
  }

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setCTime(ctime_t ctime);

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setCTimeNow();

  //----------------------------------------------------------------------------
  //! Get creation time
  //----------------------------------------------------------------------------
  void getCTime(ctime_t& ctime) const;

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setMTime(mtime_t mtime);

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setMTimeNow();

  //----------------------------------------------------------------------------
  //! Get modification time
  //----------------------------------------------------------------------------
  void getMTime(mtime_t& mtime) const;

  //----------------------------------------------------------------------------
  //! Set propagated modification time (if newer)
  //----------------------------------------------------------------------------
  bool setTMTime(tmtime_t tmtime);

  //----------------------------------------------------------------------------
  //! Set propagated modification time to now
  //----------------------------------------------------------------------------
  void setTMTimeNow();

  //----------------------------------------------------------------------------
  //! Get propagated modification time
  //----------------------------------------------------------------------------
  void getTMTime(tmtime_t& tmtime) const;

  //----------------------------------------------------------------------------
  //! Trigger an mtime change event
  //----------------------------------------------------------------------------
  void notifyMTimeChange(IContainerMDSvc* containerMDSvc);

  //----------------------------------------------------------------------------
  //! Get tree size
  //----------------------------------------------------------------------------
  inline uint64_t
  getTreeSize() const
  {
    return pTreeSize;
  }

  //----------------------------------------------------------------------------
  //! Set tree size
  //----------------------------------------------------------------------------
  inline void
  setTreeSize(uint64_t treesize)
  {
    pTreeSize = treesize;
  }

  //----------------------------------------------------------------------------
  //! Add to tree size
  //----------------------------------------------------------------------------
  uint64_t addTreeSize(uint64_t addsize);

  //----------------------------------------------------------------------------
  //! Remove from tree size
  //----------------------------------------------------------------------------
  uint64_t removeTreeSize(uint64_t removesize);

  //----------------------------------------------------------------------------
  //! Get name
  //----------------------------------------------------------------------------
  inline const std::string&
  getName() const
  {
    return pName;
  }

  //----------------------------------------------------------------------------
  //! Set name
  //----------------------------------------------------------------------------
  void setName(const std::string& name);

  //----------------------------------------------------------------------------
  //! Get uid
  //----------------------------------------------------------------------------
  inline uid_t
  getCUid() const
  {
    return pCUid;
  }

  //----------------------------------------------------------------------------
  //! Set uid
  //----------------------------------------------------------------------------
  inline void
  setCUid(uid_t uid)
  {
    pCUid = uid;
  }

  //----------------------------------------------------------------------------
  //! Get gid
  //----------------------------------------------------------------------------
  inline gid_t
  getCGid() const
  {
    return pCGid;
  }

  //----------------------------------------------------------------------------
  //! Set gid
  //----------------------------------------------------------------------------
  inline void
  setCGid(gid_t gid)
  {
    pCGid = gid;
  }

  //----------------------------------------------------------------------------
  //! Get mode
  //----------------------------------------------------------------------------
  inline mode_t
  getMode() const
  {
    return pMode;
  }

  //----------------------------------------------------------------------------
  //! Set mode
  //----------------------------------------------------------------------------
  inline void
  setMode(mode_t mode)
  {
    pMode = mode;
  }

  //----------------------------------------------------------------------------
  //! Get ACL Id
  //----------------------------------------------------------------------------
  inline uint16_t
  getACLId() const
  {
    return pACLId;
  }

  //----------------------------------------------------------------------------
  //! Set ACL Id
  //----------------------------------------------------------------------------
  inline void
  setACLId(uint16_t ACLId)
  {
    pACLId = ACLId;
  }

  //----------------------------------------------------------------------------
  //! Add extended attribute
  //----------------------------------------------------------------------------
  void
  setAttribute(const std::string& name, const std::string& value)
  {
    pXAttrs[name] = value;
  }

  //----------------------------------------------------------------------------
  //! Remove attribute
  //----------------------------------------------------------------------------
  void removeAttribute(const std::string& name);

  //----------------------------------------------------------------------------
  //! Check if the attribute exist
  //----------------------------------------------------------------------------
  bool
  hasAttribute(const std::string& name) const
  {
    return pXAttrs.find(name) != pXAttrs.end();
  }

  //----------------------------------------------------------------------------
  //! Return number of attributes
  //----------------------------------------------------------------------------
  size_t
  numAttributes() const
  {
    return pXAttrs.size();
  }

  //----------------------------------------------------------------------------
  // Get the attribute
  //----------------------------------------------------------------------------
  std::string getAttribute(const std::string& name) const;

  //----------------------------------------------------------------------------
  //! Get attribute begin iterator
  //----------------------------------------------------------------------------
  XAttrMap::iterator
  attributesBegin()
  {
    return pXAttrs.begin();
  }

  //----------------------------------------------------------------------------
  //! Get the attribute end iterator
  //----------------------------------------------------------------------------
  XAttrMap::iterator
  attributesEnd()
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
  id_t pId;
  id_t pParentId;
  uint16_t pFlags;
  ctime_t pCTime;
  std::string pName;
  uid_t pCUid;
  gid_t pCGid;
  mode_t pMode;
  uint16_t pACLId;
  XAttrMap pXAttrs;

private:
  // Non-presistent data members
  mtime_t pMTime;
  tmtime_t pTMTime;
  uint64_t pTreeSize;

  IContainerMDSvc* pContSvc;  ///< Container metadata service
  IFileMDSvc* pFileSvc;       ///< File metadata service
  qclient::QClient* pQcl;     ///< QClient object
  std::string pFilesKey;      ///< Map files key
  std::string pDirsKey;       ///< Map dir key
  qclient::QHash pFilesMap;   ///< Map holding info about files
  qclient::QHash pDirsMap;    ///< Map holding info about subcontainers
  //! Dir name to id map
  std::map<std::string, eos::IContainerMD::id_t> mDirsMap;
  std::map<std::string, eos::IFileMD::id_t> mFilesMap; ///< File name to id map
};

EOSNSNAMESPACE_END

#endif // __EOS_NS_CONTAINER_MD_HH__
