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
//! @brief Class representing the file metadata
//------------------------------------------------------------------------------

#ifndef __EOS_NS_FILE_MD_HH__
#define __EOS_NS_FILE_MD_HH__

#include "namespace/interface/IFileMD.hh"
#include "namespace/ns_on_redis/RedisClient.hh"
#include "namespace/ns_on_redis/persistency/FileMDSvc.hh"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <list>
#include <mutex>
#include <stdint.h>
#include <string>
#include <sys/time.h>

EOSNSNAMESPACE_BEGIN

//! Forward declaration
class IFileMDSvc;
class IContainerMD;

//------------------------------------------------------------------------------
//! Class holding the metadata information concerning a single file
//------------------------------------------------------------------------------
class FileMD : public IFileMD
{
  friend class FileSystemView;

public:
  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  FileMD(id_t id, IFileMDSvc* fileMDSvc);

  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  virtual ~FileMD() {};

  //----------------------------------------------------------------------------
  //! Copy constructor
  //----------------------------------------------------------------------------
  FileMD(const FileMD& other);

  //----------------------------------------------------------------------------
  //! Virtual copy constructor
  //----------------------------------------------------------------------------
  virtual FileMD* clone() const;

  //----------------------------------------------------------------------------
  //! Asignment operator
  //----------------------------------------------------------------------------
  FileMD& operator=(const FileMD& other);

  //----------------------------------------------------------------------------
  //! Get creation time
  //----------------------------------------------------------------------------
  void getCTime(ctime_t& ctime) const;

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setCTime(ctime_t ctime);

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setCTimeNow();

  //----------------------------------------------------------------------------
  //! Get modification time
  //----------------------------------------------------------------------------
  void getMTime(ctime_t& mtime) const;

  //----------------------------------------------------------------------------
  //! Set modification time
  //----------------------------------------------------------------------------
  void setMTime(ctime_t mtime);

  //----------------------------------------------------------------------------
  //! Set modification time to now
  //----------------------------------------------------------------------------
  void setMTimeNow();

  //----------------------------------------------------------------------------
  //! Get file id
  //----------------------------------------------------------------------------
  inline id_t
  getId() const
  {
    return pId;
  }

  //----------------------------------------------------------------------------
  //! Get size
  //----------------------------------------------------------------------------
  inline uint64_t
  getSize() const
  {
    return pSize;
  }

  //----------------------------------------------------------------------------
  //! Set size - 48 bytes will be used
  //----------------------------------------------------------------------------
  void setSize(uint64_t size);

  //----------------------------------------------------------------------------
  //! Get tag
  //----------------------------------------------------------------------------
  inline IContainerMD::id_t
  getContainerId() const
  {
    return pContainerId;
  }

  //----------------------------------------------------------------------------
  //! Set tag
  //----------------------------------------------------------------------------
  void
  setContainerId(IContainerMD::id_t containerId)
  {
    pContainerId = containerId;
  }

  //----------------------------------------------------------------------------
  //! Get checksum
  //----------------------------------------------------------------------------
  inline const Buffer&
  getChecksum() const
  {
    return pChecksum;
  }

  //----------------------------------------------------------------------------
  //! Compare checksums
  //!
  //! @param checksum checksum value to compare with
  //! @warning You need to supply enough bytes to compare with the checksum
  //!          stored in the object
  //----------------------------------------------------------------------------
  bool
  checksumMatch(const void* checksum) const
  {
    return !memcmp(checksum, pChecksum.getDataPtr(), pChecksum.getSize());
  }

  //----------------------------------------------------------------------------
  //! Set checksum
  //----------------------------------------------------------------------------
  void
  setChecksum(const Buffer& checksum)
  {
    pChecksum = checksum;
  }

  //----------------------------------------------------------------------------
  //! Clear checksum
  //----------------------------------------------------------------------------
  void
  clearChecksum(uint8_t size = 20)
  {
    char zero = 0;

    for (uint8_t i = 0; i < size; i++) {
      pChecksum.putData(&zero, 1);
    }
  }

  //----------------------------------------------------------------------------
  //! Set checksum
  //!
  //! @param checksum address of a memory location string the checksum
  //! @param size     size of the checksum in bytes
  //----------------------------------------------------------------------------
  void
  setChecksum(const void* checksum, uint8_t size)
  {
    pChecksum.clear();
    pChecksum.putData(checksum, size);
  }

  //----------------------------------------------------------------------------
  //! Get name
  //----------------------------------------------------------------------------
  inline const std::string
  getName() const
  {
    return pName;
  }

  //----------------------------------------------------------------------------
  //! Set name
  //----------------------------------------------------------------------------
  void setName(const std::string& name);

  //----------------------------------------------------------------------------
  //! Add location
  //----------------------------------------------------------------------------
  void addLocation(location_t location);

  //----------------------------------------------------------------------------
  //! Get vector with all the locations
  //----------------------------------------------------------------------------
  LocationVector getLocations() const;

  //----------------------------------------------------------------------------
  //! Get location
  //----------------------------------------------------------------------------
  location_t
  getLocation(unsigned int index)
  {
    if (index < pLocation.size()) {
      return pLocation[index];
    }

    return 0;
  }

  //----------------------------------------------------------------------------
  //! Replace location by index
  //----------------------------------------------------------------------------
  void replaceLocation(unsigned int index, location_t newlocation);

  //----------------------------------------------------------------------------
  //! Remove location that was previously unlinked
  //----------------------------------------------------------------------------
  void removeLocation(location_t location);

  //----------------------------------------------------------------------------
  //! Remove all locations that were previously unlinked
  //----------------------------------------------------------------------------
  void removeAllLocations();

  //----------------------------------------------------------------------------
  //! Clear locations without notifying the listeners
  //----------------------------------------------------------------------------
  void
  clearLocations()
  {
    pLocation.clear();
  }

  //----------------------------------------------------------------------------
  //! Test if location exists
  //----------------------------------------------------------------------------
  bool
  hasLocation(location_t location)
  {
    for (location_t i = 0; i < pLocation.size(); i++) {
      if (pLocation[i] == location) {
        return true;
      }
    }

    return false;
  }

  //----------------------------------------------------------------------------
  //! Get number of locations
  //----------------------------------------------------------------------------
  inline size_t
  getNumLocation() const
  {
    return pLocation.size();
  }

  //----------------------------------------------------------------------------
  //! Get vector with all unlinked locations
  //----------------------------------------------------------------------------
  LocationVector getUnlinkedLocations() const;

  //----------------------------------------------------------------------------
  //! Unlink location
  //----------------------------------------------------------------------------
  void unlinkLocation(location_t location);

  //----------------------------------------------------------------------------
  //! Unlink all locations
  //----------------------------------------------------------------------------
  void unlinkAllLocations();

  //----------------------------------------------------------------------------
  //! Clear unlinked locations without notifying the listeners
  //----------------------------------------------------------------------------
  inline void
  clearUnlinkedLocations()
  {
    pUnlinkedLocation.clear();
  }

  //----------------------------------------------------------------------------
  //! Test the unlinkedlocation
  //----------------------------------------------------------------------------
  bool
  hasUnlinkedLocation(location_t location)
  {
    for (location_t i = 0; i < pUnlinkedLocation.size(); i++) {
      if (pUnlinkedLocation[i] == location) {
        return true;
      }
    }

    return false;
  }

  //----------------------------------------------------------------------------
  //! Get number of unlinked locations
  //----------------------------------------------------------------------------
  inline size_t
  getNumUnlinkedLocation() const
  {
    return pUnlinkedLocation.size();
  }

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
  //! Get layout
  //----------------------------------------------------------------------------
  inline layoutId_t
  getLayoutId() const
  {
    return pLayoutId;
  }

  //----------------------------------------------------------------------------
  //! Set layout
  //----------------------------------------------------------------------------
  inline void
  setLayoutId(layoutId_t layoutId)
  {
    pLayoutId = layoutId;
  }

  //----------------------------------------------------------------------------
  //! Get flags
  //----------------------------------------------------------------------------
  inline uint16_t
  getFlags() const
  {
    return pFlags;
  }

  //----------------------------------------------------------------------------
  //! Get the n-th flag
  //----------------------------------------------------------------------------
  inline bool
  getFlag(uint8_t n)
  {
    return pFlags & (0x0001 << n);
  }

  //----------------------------------------------------------------------------
  //! Set flags
  //----------------------------------------------------------------------------
  inline void
  setFlags(uint16_t flags)
  {
    pFlags = flags;
  }

  //----------------------------------------------------------------------------
  //! Set the n-th flag
  //----------------------------------------------------------------------------
  void
  setFlag(uint8_t n, bool flag)
  {
    if (flag) {
      pFlags |= (1 << n);
    } else {
      pFlags &= !(1 << n);
    }
  }

  //----------------------------------------------------------------------------
  //! Env Representation
  //----------------------------------------------------------------------------
  void getEnv(std::string& env, bool escapeAnd = false);

  //----------------------------------------------------------------------------
  //! Set the FileMDSvc object
  //----------------------------------------------------------------------------
  inline void
  setFileMDSvc(IFileMDSvc* fileMDSvc)
  {
    pFileMDSvc = static_cast<FileMDSvc*>(fileMDSvc);
  }

  //----------------------------------------------------------------------------
  //! Get the FileMDSvc object
  //----------------------------------------------------------------------------
  inline virtual IFileMDSvc*
  getFileMDSvc()
  {
    return pFileMDSvc;
  }

  //----------------------------------------------------------------------------
  //! Get symbolic link
  //----------------------------------------------------------------------------
  inline std::string
  getLink() const
  {
    return pLinkName;
  }

  //----------------------------------------------------------------------------
  //! Set symbolic link
  //----------------------------------------------------------------------------
  inline void
  setLink(std::string link_name)
  {
    pLinkName = link_name;
  }

  //----------------------------------------------------------------------------
  //! Check if symbolic link
  //----------------------------------------------------------------------------
  bool
  isLink() const
  {
    return (pLinkName.length() ? true : false);
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
  void
  removeAttribute(const std::string& name)
  {
    XAttrMap::iterator it = pXAttrs.find(name);

    if (it != pXAttrs.end()) {
      pXAttrs.erase(it);
    }
  }

  //----------------------------------------------------------------------------
  //! Check if the attribute exist
  //----------------------------------------------------------------------------
  bool
  hasAttribute(const std::string& name) const
  {
    return (pXAttrs.find(name) != pXAttrs.end());
  }

  //----------------------------------------------------------------------------
  //! Return number of attributes
  //----------------------------------------------------------------------------
  inline size_t
  numAttributes() const
  {
    return pXAttrs.size();
  }

  //----------------------------------------------------------------------------
  //! Get the attribute
  //----------------------------------------------------------------------------
  std::string
  getAttribute(const std::string& name) const
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

  //----------------------------------------------------------------------------
  //! Serialize the object to a buffer
  //----------------------------------------------------------------------------
  void serialize(Buffer& buffer);

  //----------------------------------------------------------------------------
  //! Deserialize the class to a buffer
  //----------------------------------------------------------------------------
  void deserialize(const Buffer& buffer);

  //----------------------------------------------------------------------------
  //! Wait for replies to asynchronous requests
  //!
  //! @return true if all replies successful, otherwise false
  //----------------------------------------------------------------------------
  bool waitAsyncReplies();

protected:
  id_t pId;
  ctime_t pCTime;
  ctime_t pMTime;
  uint64_t pSize;
  IContainerMD::id_t pContainerId;
  uid_t pCUid;
  gid_t pCGid;
  layoutId_t pLayoutId;
  uint16_t pFlags;
  std::string pName;
  std::string pLinkName;
  LocationVector pLocation;
  LocationVector pUnlinkedLocation;
  Buffer pChecksum;
  XAttrMap pXAttrs;
  FileMDSvc* pFileMDSvc;

private:
  std::list<std::string> mErrors; ///< Error messages from the callbacks
  std::mutex mMutex; ///< Mutex for condition variable and access to the errors
  std::condition_variable mAsyncCv; ///< Condition variable for async requests
  //! Number of in-flight async requests
  std::atomic<std::uint32_t> mNumAsyncReq;
  //! Redox callback for notifications sent to listeners
  std::function<void(redox::Command<int>&)> mNotificationCb;
  //! Wrapper callback which returns a callback used by the Redox client
  std::function<decltype(mNotificationCb)(void)> mWrapperCb;
};

EOSNSNAMESPACE_END

#endif // __EOS_NS_FILE_MD_HH__
