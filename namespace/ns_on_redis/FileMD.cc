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

#include "namespace/ns_on_redis/FileMD.hh"
#include "namespace/interface/IContainerMD.hh"
#include "namespace/interface/IFileMDSvc.hh"
#include <sstream>

EOSNSNAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
FileMD::FileMD(id_t id, IFileMDSvc* fileMDSvc)
  : IFileMD(), pId(id), pCTime{0}, pMTime{0}, pSize(0), pContainerId(0), pCUid(0),
    pCGid(0), pLayoutId(0), pFlags(0), pChecksum(0),
    pFileMDSvc(dynamic_cast<FileMDSvc*>(fileMDSvc)), mMutex(), mAsyncCv(),
    mNumAsyncReq(0)
{
  pCTime.tv_sec = pCTime.tv_nsec = 0;
  pMTime.tv_sec = pMTime.tv_nsec = 0;
  // Notification callback used by Redox client
  mNotificationCb = [&](redox::Command<int>& c) {
    if (!c.ok()) {
      std::unique_lock<std::mutex> lock(mMutex);
      std::ostringstream oss;
      oss << "Failed command: " << c.cmd() << " error: " << c.lastError();
      mErrors.emplace(mErrors.end(), oss.str());
    }

    if (--mNumAsyncReq == 0) {
      mAsyncCv.notify_one();
    }
  };
  // Wrapper callback accounts for the number of requests in-flight
  mWrapperCb = [&]() -> decltype(mNotificationCb) {
    ++mNumAsyncReq;
    return mNotificationCb;
  };
}

//------------------------------------------------------------------------------
// Virtual copy constructor
//------------------------------------------------------------------------------
FileMD*
FileMD::clone() const
{
  return new FileMD(*this);
}

//------------------------------------------------------------------------------
// Copy constructor
//------------------------------------------------------------------------------
FileMD::FileMD(const FileMD& other)
{
  *this = other;
}

//------------------------------------------------------------------------------
// Asignment operator
//------------------------------------------------------------------------------
FileMD&
FileMD::operator = (const FileMD& other)
{
  pName        = other.pName;
  pId          = other.pId;
  pSize        = other.pSize;
  pContainerId = other.pContainerId;
  pCUid        = other.pCUid;
  pCGid        = other.pCGid;
  pLayoutId    = other.pLayoutId;
  pFlags       = other.pFlags;
  pLinkName    = other.pLinkName;
  pLocation    = other.pLocation;
  pUnlinkedLocation = other.pUnlinkedLocation;
  pCTime       = other.pCTime;
  pMTime       = other.pMTime;
  pChecksum    = other.pChecksum;
  pFileMDSvc   = 0;
  return *this;
}

//------------------------------------------------------------------------------
// Add location
//------------------------------------------------------------------------------
void
FileMD::addLocation(location_t location)
{
  if (hasLocation(location)) {
    return;
  }

  pLocation.push_back(location);
  IFileMDChangeListener::Event e(this, IFileMDChangeListener::LocationAdded,
                                 location);
  pFileMDSvc->notifyListeners(&e);
}

//------------------------------------------------------------------------------
// Set name
//------------------------------------------------------------------------------
void
FileMD::setName(const std::string& name)
{
  pName = name;
}

//------------------------------------------------------------------------------
// Get vector with all the locations
//------------------------------------------------------------------------------
IFileMD::LocationVector
FileMD::getLocations() const
{
  return pLocation;
}

//------------------------------------------------------------------------------
// Get vector with all unlinked locations
//------------------------------------------------------------------------------
IFileMD::LocationVector
FileMD::getUnlinkedLocations() const
{
  return pUnlinkedLocation;
}

//------------------------------------------------------------------------------
// Replace location by index
//------------------------------------------------------------------------------
void
FileMD::replaceLocation(unsigned int index, location_t newlocation)
{
  location_t oldLocation = pLocation[index];
  pLocation[index] = newlocation;
  IFileMDChangeListener::Event e(this, IFileMDChangeListener::LocationReplaced,
                                 newlocation, oldLocation);
  pFileMDSvc->notifyListeners(&e);
}

//------------------------------------------------------------------------------
// Remove location
//------------------------------------------------------------------------------
void
FileMD::removeLocation(location_t location)
{
  for (auto it = pUnlinkedLocation.begin(); it < pUnlinkedLocation.end(); ++it) {
    if (*it == location) {
      pUnlinkedLocation.erase(it);
      IFileMDChangeListener::Event e(
        this, IFileMDChangeListener::LocationRemoved, location);
      pFileMDSvc->notifyListeners(&e);
      return;
    }
  }
}

//------------------------------------------------------------------------------
// Remove all locations that were previously unlinked
//------------------------------------------------------------------------------
void
FileMD::removeAllLocations()
{
  std::vector<location_t>::reverse_iterator it;

  while ((it = pUnlinkedLocation.rbegin()) != pUnlinkedLocation.rend()) {
    pUnlinkedLocation.pop_back();
    IFileMDChangeListener::Event e(this, IFileMDChangeListener::LocationRemoved,
                                   *it);
    pFileMDSvc->notifyListeners(&e);
  }
}

//------------------------------------------------------------------------------
// Unlink location
//------------------------------------------------------------------------------
void
FileMD::unlinkLocation(location_t location)
{
  for (auto it = pLocation.begin(); it < pLocation.end(); it++) {
    if (*it == location) {
      pUnlinkedLocation.push_back(*it);
      pLocation.erase(it);
      IFileMDChangeListener::Event e(
        this, IFileMDChangeListener::LocationUnlinked, location);
      pFileMDSvc->notifyListeners(&e);
      return;
    }
  }
}

//------------------------------------------------------------------------------
// Unlink all locations
//------------------------------------------------------------------------------
void
FileMD::unlinkAllLocations()
{
  std::vector<location_t>::reverse_iterator it;

  while ((it = pLocation.rbegin()) != pLocation.rend()) {
    location_t loc = *it;
    pUnlinkedLocation.push_back(loc);
    pLocation.pop_back();
    IFileMDChangeListener::Event e(
      this, IFileMDChangeListener::LocationUnlinked, loc);
    pFileMDSvc->notifyListeners(&e);
  }
}

//------------------------------------------------------------------------
//  Env Representation
//------------------------------------------------------------------------
void
FileMD::getEnv(std::string& env, bool escapeAnd)
{
  env = "";
  std::ostringstream o;
  std::string saveName = pName;

  if (escapeAnd) {
    if (!saveName.empty()) {
      std::string from = "&";
      std::string to = "#AND#";
      size_t start_pos = 0;

      while ((start_pos = saveName.find(from, start_pos)) !=
             std::string::npos) {
        saveName.replace(start_pos, from.length(), to);
        start_pos += to.length();
      }
    }
  }

  o << "name=" << saveName << "&id=" << pId << "&ctime=" << pCTime.tv_sec;
  o << "&ctime_ns=" << pCTime.tv_nsec << "&mtime=" << pMTime.tv_sec;
  o << "&mtime_ns=" << pMTime.tv_nsec << "&size=" << pSize;
  o << "&cid=" << pContainerId << "&uid=" << pCUid << "&gid=" << pCGid;
  o << "&lid=" << pLayoutId;
  env += o.str();
  env += "&location=";
  char locs[16];

  for (auto && elem : pLocation) {
    snprintf(static_cast<char*>(locs), sizeof(locs), "%u", elem);
    env += static_cast<char*>(locs);
    env += ",";
  }

  for (auto && elem : pUnlinkedLocation) {
    snprintf(static_cast<char*>(locs), sizeof(locs), "!%u", elem);
    env += static_cast<char*>(locs);
    env += ",";
  }

  env += "&checksum=";
  uint8_t size = pChecksum.getSize();

  for (uint8_t i = 0; i < size; i++) {
    char hx[3];
    hx[0] = 0;
    snprintf(static_cast<char*>(hx), sizeof(hx), "%02x",
             *reinterpret_cast<unsigned char*>(pChecksum.getDataPtr() + i));
    env += static_cast<char*>(hx);
  }
}

//------------------------------------------------------------------------------
// Serialize the object to a std::string buffer
//------------------------------------------------------------------------------
void
FileMD::serialize(eos::Buffer& buffer)
{
  if (pFileMDSvc == nullptr) {
    MDException ex(ENOTSUP);
    ex.getMessage() << "This was supposed to be a read only copy!";
    throw ex;
  }

  buffer.putData(&pId, sizeof(pId));
  buffer.putData(&pCTime, sizeof(pCTime));
  buffer.putData(&pMTime, sizeof(pMTime));
  uint64_t tmp = pFlags;
  tmp <<= 48;
  tmp |= (pSize & 0x0000ffffffffffff);
  buffer.putData(&tmp, sizeof(tmp));
  buffer.putData(&pContainerId, sizeof(pContainerId));
  // Symbolic links are serialized as <name>//<link>
  std::string nameAndLink = pName;

  if (pLinkName.length() != 0u) {
    nameAndLink += "//";
    nameAndLink += pLinkName;
  }

  uint16_t len = nameAndLink.length() + 1;
  buffer.putData(&len, sizeof(len));
  buffer.putData(nameAndLink.c_str(), len);
  len = pLocation.size();
  buffer.putData(&len, sizeof(len));
  LocationVector::iterator it;

  for (it = pLocation.begin(); it != pLocation.end(); ++it) {
    location_t location = *it;
    buffer.putData(&location, sizeof(location_t));
  }

  len = pUnlinkedLocation.size();
  buffer.putData(&len, sizeof(len));

  for (it = pUnlinkedLocation.begin(); it != pUnlinkedLocation.end(); ++it) {
    location_t location = *it;
    buffer.putData(&location, sizeof(location_t));
  }

  buffer.putData(&pCUid, sizeof(pCUid));
  buffer.putData(&pCGid, sizeof(pCGid));
  buffer.putData(&pLayoutId, sizeof(pLayoutId));
  uint8_t size = pChecksum.getSize();
  buffer.putData(&size, sizeof(size));
  buffer.putData(pChecksum.getDataPtr(), size);

  // May store xattr
  if (!pXAttrs.empty()) {
    uint16_t len = pXAttrs.size();
    buffer.putData(&len, sizeof(len));
    XAttrMap::iterator it;

    for (it = pXAttrs.begin(); it != pXAttrs.end(); ++it) {
      uint16_t strLen = it->first.length() + 1;
      buffer.putData(&strLen, sizeof(strLen));
      buffer.putData(it->first.c_str(), strLen);
      strLen = it->second.length() + 1;
      buffer.putData(&strLen, sizeof(strLen));
      buffer.putData(it->second.c_str(), strLen);
    }
  }

  if (!waitAsyncReplies()) {
    MDException ex(EIO);
    ex.getMessage() << "KV-store asynchronous reuqests failed: ";

    // Append all the error messages to the exception description
    for (auto && err : mErrors) {
      ex.getMessage() << "(" << err << ") ";
    }

    throw ex;
  }
}

//------------------------------------------------------------------------------
// Deserialize the class to a buffer
//------------------------------------------------------------------------------
void
FileMD::deserialize(const eos::Buffer& buffer)
{
  uint16_t offset = 0;
  offset = buffer.grabData(offset, &pId, sizeof(pId));
  offset = buffer.grabData(offset, &pCTime, sizeof(pCTime));
  offset = buffer.grabData(offset, &pMTime, sizeof(pMTime));
  uint64_t tmp;
  offset = buffer.grabData(offset, &tmp, sizeof(tmp));
  pSize = tmp & 0x0000ffffffffffff;
  tmp >>= 48;
  pFlags = tmp & 0x000000000000ffff;
  offset = buffer.grabData(offset, &pContainerId, sizeof(pContainerId));
  uint16_t len = 0;
  offset = buffer.grabData(offset, &len, 2);
  char strBuffer[len];
  offset = buffer.grabData(offset, static_cast<char*>(strBuffer), len);
  pName = static_cast<char*>(strBuffer);
  // Possibly extract symbolic link
  size_t link_pos = pName.find("//");

  if (link_pos != std::string::npos) {
    pLinkName = pName.substr(link_pos + 2);
    pName.erase(link_pos);
  }

  offset = buffer.grabData(offset, &len, 2);

  for (uint16_t i = 0; i < len; ++i) {
    location_t location;
    offset = buffer.grabData(offset, &location, sizeof(location_t));
    pLocation.push_back(location);
  }

  offset = buffer.grabData(offset, &len, 2);

  for (uint16_t i = 0; i < len; ++i) {
    location_t location;
    offset = buffer.grabData(offset, &location, sizeof(location_t));
    pUnlinkedLocation.push_back(location);
  }

  offset = buffer.grabData(offset, &pCUid, sizeof(pCUid));
  offset = buffer.grabData(offset, &pCGid, sizeof(pCGid));
  offset = buffer.grabData(offset, &pLayoutId, sizeof(pLayoutId));
  uint8_t size = 0;
  offset = buffer.grabData(offset, &size, sizeof(size));
  pChecksum.resize(size);
  offset = buffer.grabData(offset, pChecksum.getDataPtr(), size);

  if ((buffer.size() - offset) >= 4) {
    // XAttr are optional
    uint16_t len1 = 0;
    uint16_t len2 = 0;
    uint16_t len = 0;
    offset = buffer.grabData(offset, &len, sizeof(len));

    for (uint16_t i = 0; i < len; ++i) {
      offset = buffer.grabData(offset, &len1, sizeof(len1));
      char strBuffer1[len1];
      offset = buffer.grabData(offset, static_cast<char*>(strBuffer1), len1);
      offset = buffer.grabData(offset, &len2, sizeof(len2));
      char strBuffer2[len2];
      offset = buffer.grabData(offset, static_cast<char*>(strBuffer2), len2);
      pXAttrs.insert(std::pair<std::string, std::string>
                     (static_cast<char*>(strBuffer1),
                      static_cast<char*>(strBuffer2)));
    }
  }
}

//------------------------------------------------------------------------------
// Set size - 48 bytes will be used
//------------------------------------------------------------------------------
void
FileMD::setSize(uint64_t size)
{
  int64_t sizeChange = (size & 0x0000ffffffffffff) - pSize;
  pSize = size & 0x0000ffffffffffff;
  IFileMDChangeListener::Event e(this, IFileMDChangeListener::SizeChange, 0, 0,
                                 sizeChange);
  pFileMDSvc->notifyListeners(&e);
}

//------------------------------------------------------------------------------
// Get creation time
//------------------------------------------------------------------------------
void
FileMD::getCTime(ctime_t& ctime) const
{
  ctime.tv_sec = pCTime.tv_sec;
  ctime.tv_nsec = pCTime.tv_nsec;
}

//------------------------------------------------------------------------------
// Set creation time
//------------------------------------------------------------------------------
void
FileMD::setCTime(ctime_t ctime)
{
  pCTime.tv_sec = ctime.tv_sec;
  pCTime.tv_nsec = ctime.tv_nsec;
}

//----------------------------------------------------------------------------
// Set creation time to now
//----------------------------------------------------------------------------
void
FileMD::setCTimeNow()
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

//------------------------------------------------------------------------------
// Get modification time
//------------------------------------------------------------------------------
void
FileMD::getMTime(ctime_t& mtime) const
{
  mtime.tv_sec = pMTime.tv_sec;
  mtime.tv_nsec = pMTime.tv_nsec;
}

//------------------------------------------------------------------------------
// Set modification time
//------------------------------------------------------------------------------
void
FileMD::setMTime(ctime_t mtime)
{
  pMTime.tv_sec = mtime.tv_sec;
  pMTime.tv_nsec = mtime.tv_nsec;
}

//------------------------------------------------------------------------------
// Set modification time to now
//------------------------------------------------------------------------------
void
FileMD::setMTimeNow()
{
#ifdef __APPLE__
  struct timeval tv;
  gettimeofday(&tv, 0);
  pMTime.tv_sec = tv.tv_sec;
  pMTime.tv_nsec = tv.tv_usec * 1000;
#else
  clock_gettime(CLOCK_REALTIME, &pMTime);
#endif
}

//------------------------------------------------------------------------------
// Wait for replies to asynchronous requests
//------------------------------------------------------------------------------
bool
FileMD::waitAsyncReplies()
{
  std::unique_lock<std::mutex> lock(mMutex);

  while (mNumAsyncReq != 0u) {
    mAsyncCv.wait(lock);
  }

  if (!mErrors.empty()) {
    return false;
  }

  return true;
}

EOSNSNAMESPACE_END
