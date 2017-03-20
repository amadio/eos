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

#include "namespace/ns_quarkdb/NsOnRedisPlugin.hh"
#include "namespace/interface/IContainerMDSvc.hh"
#include "namespace/ns_quarkdb/accounting/ContainerAccounting.hh"
#include "namespace/ns_quarkdb/accounting/FileSystemView.hh"
#include "namespace/ns_quarkdb/accounting/SyncTimeAccounting.hh"
#include "namespace/ns_quarkdb/persistency/ContainerMDSvc.hh"
#include "namespace/ns_quarkdb/persistency/FileMDSvc.hh"
#include "namespace/ns_quarkdb/views/HierarchicalView.hh"
#include <iostream>

//------------------------------------------------------------------------------
// Plugin exit function called by the PluginManager when doing cleanup
//------------------------------------------------------------------------------
int32_t
ExitFunc()
{
  return 0;
}

//------------------------------------------------------------------------------
// Plugin registration entry point called by the PluginManager
//------------------------------------------------------------------------------
PF_ExitFunc
PF_initPlugin(const PF_PlatformServices* services)
{
  std::cout << "Register objects provided by NsOnRedisPlugin ..." << std::endl;
  // Register container metadata service
  PF_RegisterParams param_cmdsvc = {};
  param_cmdsvc.version.major = 0;
  param_cmdsvc.version.minor = 1;
  param_cmdsvc.CreateFunc = eos::NsOnRedisPlugin::CreateContainerMDSvc;
  param_cmdsvc.DestroyFunc = eos::NsOnRedisPlugin::DestroyContainerMDSvc;
  // Register file metadata service
  PF_RegisterParams param_fmdsvc = {};
  param_fmdsvc.version.major = 0;
  param_fmdsvc.version.minor = 1;
  param_fmdsvc.CreateFunc = eos::NsOnRedisPlugin::CreateFileMDSvc;
  param_fmdsvc.DestroyFunc = eos::NsOnRedisPlugin::DestroyFileMDSvc;
  // Register hierarchical view
  PF_RegisterParams param_hview = {};
  param_hview.version.major = 0;
  param_hview.version.minor = 1;
  param_hview.CreateFunc = eos::NsOnRedisPlugin::CreateHierarchicalView;
  param_hview.DestroyFunc = eos::NsOnRedisPlugin::DestroyHierarchicalView;
  // Register file system view
  PF_RegisterParams param_fsview = {};
  param_fsview.version.major = 0;
  param_fsview.version.minor = 1;
  param_fsview.CreateFunc = eos::NsOnRedisPlugin::CreateFsView;
  param_fsview.DestroyFunc = eos::NsOnRedisPlugin::DestroyFsView;
  // Register recursive container accounting view
  PF_RegisterParams param_contacc = {};
  param_contacc.version.major = 0;
  param_contacc.version.minor = 1;
  param_contacc.CreateFunc = eos::NsOnRedisPlugin::CreateContAcc;
  param_contacc.DestroyFunc = eos::NsOnRedisPlugin::DestroyContAcc;
  // Register recursive container accounting view
  PF_RegisterParams param_syncacc = {};
  param_syncacc.version.major = 0;
  param_syncacc.version.minor = 1;
  param_syncacc.CreateFunc = eos::NsOnRedisPlugin::CreateSyncTimeAcc;
  param_syncacc.DestroyFunc = eos::NsOnRedisPlugin::DestroySyncTimeAcc;
  // TODO(esindril): define the necessary objects to be provided by the
  // namespace in a common header
  std::map<std::string, PF_RegisterParams> map_obj = {
    {"ContainerMDSvc", param_cmdsvc},
    {"FileMDSvc", param_fmdsvc},
    {"HierarchicalView", param_hview},
    {"FileSystemView", param_fsview},
    {"ContainerAccounting", param_contacc},
    {"SyncTimeAccounting", param_syncacc}
  };

  // Register all the provided object with the Plugin Manager
  for (const auto& elem : map_obj) {
    if (services->registerObject(elem.first.c_str(), &elem.second) != 0) {
      std::cerr << "Failed registering object " << elem.first << std::endl;
      return nullptr;
    }
  }

  return ExitFunc;
}

EOSNSNAMESPACE_BEGIN

// Static variables
eos::IContainerMDSvc* NsOnRedisPlugin::pContMDSvc = nullptr;

//------------------------------------------------------------------------------
// Create container metadata service
//------------------------------------------------------------------------------
void*
NsOnRedisPlugin::CreateContainerMDSvc(PF_PlatformServices* /*services*/)
{
  if (pContMDSvc == nullptr) {
    pContMDSvc = new ContainerMDSvc();
  }

  return pContMDSvc;
}

//------------------------------------------------------------------------------
// Destroy container metadata service
//------------------------------------------------------------------------------
int32_t
NsOnRedisPlugin::DestroyContainerMDSvc(void* obj)
{
  if (obj == nullptr) {
    return -1;
  }

  delete static_cast<ContainerMDSvc*>(obj);
  return 0;
}

//------------------------------------------------------------------------------
// Create file metadata service
//------------------------------------------------------------------------------
void*
NsOnRedisPlugin::CreateFileMDSvc(PF_PlatformServices* /*services*/)
{
  return new FileMDSvc();
}

//------------------------------------------------------------------------------
// Destroy file metadata service
//------------------------------------------------------------------------------
int32_t
NsOnRedisPlugin::DestroyFileMDSvc(void* obj)
{
  if (obj == nullptr) {
    return -1;
  }

  delete static_cast<FileMDSvc*>(obj);
  return 0;
}

//------------------------------------------------------------------------------
// Create hierarchical view
//------------------------------------------------------------------------------
void*
NsOnRedisPlugin::CreateHierarchicalView(PF_PlatformServices* /*services*/)
{
  return new HierarchicalView();
}

//------------------------------------------------------------------------------
// Destroy hierarchical view
//------------------------------------------------------------------------------
int32_t
NsOnRedisPlugin::DestroyHierarchicalView(void* obj)
{
  if (obj == nullptr) {
    return -1;
  }

  delete static_cast<HierarchicalView*>(obj);
  return 0;
}

//------------------------------------------------------------------------------
// Create file system view
//------------------------------------------------------------------------------
void*
NsOnRedisPlugin::CreateFsView(PF_PlatformServices* /*services*/)
{
  return new FileSystemView();
}

//------------------------------------------------------------------------------
// Destroy file system view
//------------------------------------------------------------------------------
int32_t
NsOnRedisPlugin::DestroyFsView(void* obj)
{
  if (obj == nullptr) {
    return -1;
  }

  delete static_cast<FileSystemView*>(obj);
  return 0;
}

//------------------------------------------------------------------------------
// Create recursive container accounting listener
//------------------------------------------------------------------------------
void*
NsOnRedisPlugin::CreateContAcc(PF_PlatformServices* /*services*/)
{
  if (pContMDSvc == nullptr) {
    return nullptr;
  }

  return new ContainerAccounting(pContMDSvc);
}

//------------------------------------------------------------------------------
// Destroy recursive container accounting listener
//------------------------------------------------------------------------------
int32_t
NsOnRedisPlugin::DestroyContAcc(void* obj)
{
  if (obj == nullptr) {
    return -1;
  }

  delete static_cast<ContainerAccounting*>(obj);
  return 0;
}

//------------------------------------------------------------------------------
// Create sync time propagation listener
//------------------------------------------------------------------------------
void*
NsOnRedisPlugin::CreateSyncTimeAcc(PF_PlatformServices* /*services*/)
{
  if (pContMDSvc == nullptr) {
    return nullptr;
  }

  return new SyncTimeAccounting(pContMDSvc);
}

//------------------------------------------------------------------------------
// Destroy sync time propagation listener
//------------------------------------------------------------------------------
int32_t
NsOnRedisPlugin::DestroySyncTimeAcc(void* obj)
{
  if (obj == nullptr) {
    return -1;
  }

  delete static_cast<FileSystemView*>(obj);
  return 0;
}

EOSNSNAMESPACE_END
