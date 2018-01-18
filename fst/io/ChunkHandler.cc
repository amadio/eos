//------------------------------------------------------------------------------
// File: ChunkHandler.cc
// Author: Elvin-Alin Sindrilaru <esindril@cern.ch>
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

#include "fst/io/AsyncMetaHandler.hh"
#include "fst/io/ChunkHandler.hh"

EOSFSTNAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
ChunkHandler::ChunkHandler(AsyncMetaHandler* metaHandler, uint64_t offset,
                           uint32_t length, char* buff, bool isWrite) :
  XrdCl::ResponseHandler(),
  mBuffer(buff),
  mMetaHandler(metaHandler),
  mOffset(offset),
  mLength(length),
  mCapacity(0),
  mRespLength(0),
  mIsWrite(isWrite)
{
  if (mIsWrite) {
    mCapacity = length;
    mBuffer = static_cast<char*>(calloc(mCapacity, sizeof(char)));

    if (mBuffer) {
      mBuffer = static_cast<char*>(memcpy(mBuffer, buff, length));
    }
  }
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
ChunkHandler::~ChunkHandler()
{
  if (mIsWrite && mBuffer) {
    free(mBuffer);
  }
}

//------------------------------------------------------------------------------
// Update function
//------------------------------------------------------------------------------
void
ChunkHandler::Update(AsyncMetaHandler* metaHandler,  uint64_t offset,
                     uint32_t length, char* buff, bool isWrite)
{
  mMetaHandler = metaHandler;
  mOffset = offset;
  mLength = length;
  mRespLength = 0;

  if (mIsWrite && !isWrite) {
    // write -> read
    free(mBuffer);
    mBuffer = buff;
    mCapacity = 0;
  } else if (!mIsWrite && !isWrite) {
    // read -> read
    mBuffer = buff;
  } else if (mIsWrite && isWrite) {
    // write -> write
    if (length > mCapacity) {
      mCapacity = length;
      mBuffer = static_cast<char*>(realloc(mBuffer, mCapacity));
    }

    mBuffer = static_cast<char*>(memcpy(mBuffer, buff, length));
  } else {
    // read -> write
    mCapacity = length;
    mBuffer = static_cast<char*>(calloc(mCapacity, sizeof(char)));
    mBuffer = static_cast<char*>(memcpy(mBuffer, buff, length));
  }

  mIsWrite = isWrite;
}

//------------------------------------------------------------------------------
// Handle response
//------------------------------------------------------------------------------
void
ChunkHandler::HandleResponse(XrdCl::XRootDStatus* pStatus,
                             XrdCl::AnyObject* pResponse)
{
  // Do some extra check for the read case
  if ((mIsWrite == false) && (pResponse)) {
    XrdCl::ChunkInfo* chunk = 0;
    pResponse->Get(chunk);
    mRespLength = chunk->length;

    // Notice if we received less then we initially requested - usually this means
    // we reached the end of the file, but we will treat it as an error
    if (mLength != mRespLength) {
      pStatus->status = XrdCl::stError;
      pStatus->code = XrdCl::errErrorResponse;
    }
  }

  if (pResponse) {
    delete pResponse;
  }

  mMetaHandler->HandleResponse(pStatus, this);
  delete pStatus;
}

EOSFSTNAMESPACE_END
