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

#include "namespace/ns_in_memory/accounting/ContainerAccounting.hh"
#include <iostream>

EOSNSNAMESPACE_BEGIN

//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------
ContainerAccounting::ContainerAccounting(IContainerMDSvc* svc) :
  pContainerMDSvc(svc)
{
}

//----------------------------------------------------------------------------
// Notify the me about the changes in the main view
//----------------------------------------------------------------------------
void ContainerAccounting::fileMDChanged(IFileMDChangeListener::Event* e)
{
  switch (e->action) {
  case IFileMDChangeListener::SizeChange:
    Account(e->file, e->sizeChange);
    break;

  default:
    break;
  }
}

//------------------------------------------------------------------------------
// Account a file in the respective container
//------------------------------------------------------------------------------
void ContainerAccounting::Account(IFileMD* obj , int64_t dsize)
{
  size_t deepness = 0;

  if (!obj) {
    return;
  }

  ContainerMD::id_t iId = obj->getContainerId();

  while ((iId > 1) && (deepness < 255)) {
    std::shared_ptr<IContainerMD> iCont;

    try {
      iCont = pContainerMDSvc->getContainerMD(iId);
    } catch (MDException& e) {
    }

    if (!iCont) {
      return;
    }

    iCont->addTreeSize(dsize);
    iId = iCont->getParentId();
    deepness++;
  }
}

//------------------------------------------------------------------------------
// Add tree
//------------------------------------------------------------------------------
void ContainerAccounting::AddTree(IContainerMD* obj , int64_t dsize)
{
  size_t deepness = 0;

  if (!obj) {
    return;
  }

  ContainerMD::id_t iId = obj->getId();

  while ((iId > 1) && (deepness < 255)) {
    std::shared_ptr<IContainerMD> iCont;

    try {
      iCont = pContainerMDSvc->getContainerMD(iId);
    } catch (MDException& e) {}

    if (!iCont) {
      return;
    }

    iCont->addTreeSize(dsize);
    iId = iCont->getParentId();
    deepness++;
  }
}

//------------------------------------------------------------------------------
//! Remove tree
//------------------------------------------------------------------------------
void ContainerAccounting::RemoveTree(IContainerMD* obj , int64_t dsize)
{
  AddTree(obj, -dsize);
}

EOSNSNAMESPACE_END
