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

#include "namespace/ns_quarkdb/accounting/SyncTimeAccounting.hh"
#include <iostream>

EOSNSNAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
SyncTimeAccounting::SyncTimeAccounting(IContainerMDSvc* svc)
  : pContainerMDSvc(svc)
{
}

//------------------------------------------------------------------------------
// Notify the me about the changes in the main view
//------------------------------------------------------------------------------
void
SyncTimeAccounting::containerMDChanged(IContainerMD* obj, Action type)
{
  switch (type) {
  // MTime change
  case IContainerMDChangeListener::MTimeChange:
    Propagate(obj->getId());
    break;

  default:
    break;
  }
}

//------------------------------------------------------------------------------
// Propagate the sync time
//------------------------------------------------------------------------------
void
SyncTimeAccounting::Propagate(IContainerMD::id_t id)
{
  size_t deepness = 0;

  if (id == 0u) {
    return;
  }

  IContainerMD::ctime_t mTime = {0};
  mTime.tv_sec = mTime.tv_nsec = 0;
  IContainerMD::id_t iId = id;

  while ((iId > 1) && (deepness < 255)) {
    std::shared_ptr<IContainerMD> iCont;

    try {
      iCont = pContainerMDSvc->getContainerMD(iId);

      // Only traverse if there there is an attribute saying so
      if (!iCont->hasAttribute("sys.mtime.propagation")) {
        return;
      }

      if (deepness == 0u) {
        iCont->getMTime(mTime);
      }

      if (!iCont->setTMTime(mTime) && (deepness != 0u)) {
        return;
      }
    } catch (MDException& e) {
      iCont = nullptr;
    }

    if (iCont == nullptr) {
      return;
    }

    iId = iCont->getParentId();
    deepness++;
  }
}

EOSNSNAMESPACE_END
