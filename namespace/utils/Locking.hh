//------------------------------------------------------------------------------
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Locking utilities
//------------------------------------------------------------------------------
// EOS - the CERN Disk Storage System
// Copyright (C) 2011 CERN/Switzerland
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------


#ifndef EOS_NS_LOCKING_HH
#define EOS_NS_LOCKING_HH

namespace eos
{
  class LockHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Virtual destructor
      //------------------------------------------------------------------------
      virtual ~LockHandler() {}

      //------------------------------------------------------------------------
      //! Take a read lock
      //------------------------------------------------------------------------
      virtual void readLock() = 0;

      //------------------------------------------------------------------------
      //! Take a write lock
      //------------------------------------------------------------------------
      virtual void writeLock() = 0;

      //------------------------------------------------------------------------
      //! Release read lock 
      //------------------------------------------------------------------------
      virtual void readUnLock() = 0;

      //------------------------------------------------------------------------
      //! Release write lock 
      //------------------------------------------------------------------------
      virtual void writeUnLock() = 0;
  };
}

#endif // EOS_NS_LOCKING_HH
