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
//! @author Elvin Sindrilaru <esindril@cern.ch>
//! @brief  Namespace on Redis plugin interface implementation
//------------------------------------------------------------------------------

#ifndef __EOS_NS_ON_REDIS_PLUGIN_HH__
#define __EOS_NS_ON_REDIS_PLUGIN_HH__

#include "common/plugin_manager/Plugin.hh"
#include "namespace/Namespace.hh"

//------------------------------------------------------------------------------
//! Plugin exit function called by the PluginManager when doing cleanup
//------------------------------------------------------------------------------
extern "C" int32_t ExitFunc();

//------------------------------------------------------------------------------
//! Plugin registration entry point called by the PluginManager
//------------------------------------------------------------------------------
extern "C" PF_ExitFunc PF_initPlugin(const PF_PlatformServices* services);

EOSNSNAMESPACE_BEGIN

//! Forward declaration
class IContainerMDSvc;

//------------------------------------------------------------------------------
//! Class NsOnRedisPlugin
//------------------------------------------------------------------------------
class NsOnRedisPlugin
{
public:
  //----------------------------------------------------------------------------
  //! Create container metadata service
  //!
  //! @param services pointer to other services that the plugin manager might
  //!         provide
  //!
  //! @return pointer to container metadata service
  //----------------------------------------------------------------------------
  static void* CreateContainerMDSvc(PF_PlatformServices* services);

  //----------------------------------------------------------------------------
  //! Destroy container metadata service
  //!
  //! @return 0 if successful, otherwise errno
  //----------------------------------------------------------------------------
  static int32_t DestroyContainerMDSvc(void*);

  //----------------------------------------------------------------------------
  //! Create file metadata service
  //!
  //! @param services pointer to other services that the plugin manager might
  //!         provide
  //!
  //! @return pointer to file metadata service
  //----------------------------------------------------------------------------
  static void* CreateFileMDSvc(PF_PlatformServices* services);

  //----------------------------------------------------------------------------
  //! Destroy file metadata service
  //!
  //! @return 0 if successful, otherwise errno
  //----------------------------------------------------------------------------
  static int32_t DestroyFileMDSvc(void*);

  //----------------------------------------------------------------------------
  //! Create hierarchical view
  //!
  //! @param services pointer to other services that the plugin manager might
  //!         provide
  //!
  //! @return pointer to hierarchical view
  //----------------------------------------------------------------------------
  static void* CreateHierarchicalView(PF_PlatformServices* services);

  //----------------------------------------------------------------------------
  //! Destroy hierarchical view
  //!
  //! @return 0 if successful, otherwise errno
  //----------------------------------------------------------------------------
  static int32_t DestroyHierarchicalView(void*);

  //----------------------------------------------------------------------------
  //! Create file system view
  //!
  //! @param services pointer to other services that the plugin manager might
  //!         provide
  //!
  //! @return pointer to file system view
  //----------------------------------------------------------------------------
  static void* CreateFsView(PF_PlatformServices* services);

  //----------------------------------------------------------------------------
  //! Destroy file system view
  //!
  //! @return 0 if successful, otherwise errno
  //----------------------------------------------------------------------------
  static int32_t DestroyFsView(void*);

  //----------------------------------------------------------------------------
  //! Create recursive container accounting listener
  //!
  //! @param services pointer to other services that the plugin manager might
  //!         provide
  //!
  //! @return pointer to container accounting listener
  //----------------------------------------------------------------------------
  static void* CreateContAcc(PF_PlatformServices* services);

  //----------------------------------------------------------------------------
  //! Destroy recursive container accounting listener
  //!
  //! @return 0 if successful, otherwise errno
  //----------------------------------------------------------------------------
  static int32_t DestroyContAcc(void*);

  //----------------------------------------------------------------------------
  //! Create sync time propagation listener
  //!
  //! @param services pointer to other services that the plugin manager might
  //!         provide
  //!
  //! @return pointer to sync time propagation listener
  //----------------------------------------------------------------------------
  static void* CreateSyncTimeAcc(PF_PlatformServices* services);

  //----------------------------------------------------------------------------
  //! Destroy sync time propagation listener
  //!
  //! @return 0 if successful, otherwise errno
  //----------------------------------------------------------------------------
  static int32_t DestroySyncTimeAcc(void*);

private:
  static IContainerMDSvc* pContMDSvc; ///< Pointer to container MD service
};

EOSNSNAMESPACE_END

#endif // __EOS_NS_ON_REDIS_PLUGIN_HH__
