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
// desc:   Checksumming, data conversion and other stuff
//------------------------------------------------------------------------------

#include "namespace/utils/DataHelper.hh"
#include "namespace/MDException.hh"
#include "common/crc32c/crc32c.h"
#include <zlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cerrno>

namespace eos
{

//------------------------------------------------------------------------------
// Compute crc32 checksum out of a buffer
//------------------------------------------------------------------------------
uint32_t DataHelper::computeCRC32(void* buffer, uint32_t len)
{
  return crc32(crc32(0L, Z_NULL, 0), (const Bytef*)buffer, len);
}

//------------------------------------------------------------------------------
// Update a crc32 checksum
//------------------------------------------------------------------------------
uint32_t DataHelper::updateCRC32(uint32_t crc, void* buffer, uint32_t len)
{
  return crc32(crc, (const Bytef*)buffer, len);
}

//------------------------------------------------------------------------------
// Compute crc32c checksum out of a buffer
//------------------------------------------------------------------------------
uint32_t DataHelper::computeCRC32C(void* buffer, uint32_t len)
{
  return checksum::crc32c(checksum::crc32cInit(), (const Bytef*)buffer, len);
}

//------------------------------------------------------------------------------
// Update a crc32c checksum
//------------------------------------------------------------------------------
uint32_t DataHelper::updateCRC32C(uint32_t crc, void* buffer, uint32_t len)
{
  return checksum::crc32c(crc, (const Bytef*)buffer, len);
}

//------------------------------------------------------------------------------
// Finalize crc32c checksum
//------------------------------------------------------------------------------
uint32_t DataHelper::finalizeCRC32C(uint32_t crc)
{
  return checksum::crc32cFinish(crc);
}

//----------------------------------------------------------------------------
// Copy file ownership information
//----------------------------------------------------------------------------
void DataHelper::copyOwnership(const std::string& target,
                               const std::string& source,
                               bool ignoreNoPerm)
{
  //--------------------------------------------------------------------------
  // Check the root-ness
  //--------------------------------------------------------------------------
  uid_t uid = getuid();

  if (uid != 0 && ignoreNoPerm) {
    return;
  }

  if (uid != 0) {
    MDException e(EFAULT);
    e.getMessage() << "Only root can change ownership";
    throw e;
  }

  //--------------------------------------------------------------------------
  // Get the thing done
  //--------------------------------------------------------------------------
  struct stat st;

  if (stat(source.c_str(), &st) != 0) {
    MDException e(errno);
    e.getMessage() << "Unable to stat source: " << source;
    throw e;
  }

  if (chown(target.c_str(), st.st_uid, st.st_gid) != 0) {
    MDException e(errno);
    e.getMessage() << "Unable to change the ownership of the target: ";
    e.getMessage() << target;
    throw e;
  }
}
}

