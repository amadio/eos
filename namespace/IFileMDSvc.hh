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
// desc:   FileMD interface
//------------------------------------------------------------------------------

#ifndef EOS_NS_I_FILE_MD_SVC_HH
#define EOS_NS_I_FILE_MD_SVC_HH

#include "namespace/FileMD.hh"
#include "namespace/ContainerMD.hh"
#include "namespace/MDException.hh"

#include <map>
#include <string>

namespace eos
{
  class IContainerMDSvc;

  //----------------------------------------------------------------------------
  //! Interface for a listener that is notified about all of the
  //! actions performed in a IFileMDSvc
  //----------------------------------------------------------------------------
  class IFileMDChangeListener
  {
    public:
      enum Action
        {
          Updated = 0,
          Deleted,
          Created,
          LocationAdded,
          LocationUnlinked,
          LocationRemoved,
          LocationReplaced,
	  SizeChange
        };

      //------------------------------------------------------------------------
      //! Event sent to the listener
      //------------------------------------------------------------------------
      struct Event
      {
        Event( FileMD *_file, Action _action,
               FileMD::location_t _location = 0,
               FileMD::location_t _oldLocation = 0,
	       int64_t _changed_size = 0):
          file( _file ),
          fileId( 0 ),
          action( _action ),
          location( _location ),
          oldLocation( _oldLocation ),
	  sizeChange( _changed_size) {}

        Event( FileMD::id_t _fileId, Action _action,
               FileMD::location_t _location = 0,
               FileMD::location_t _oldLocation = 0,
	       int64_t _changed_size = 0):
          file( 0 ),
          fileId( _fileId ),
          action( _action ),
          location( _location ),
          oldLocation( _oldLocation ),
	  sizeChange( _changed_size) {}

        FileMD             *file;
        FileMD::id_t        fileId;
        Action              action;
        FileMD::location_t  location;
        FileMD::location_t  oldLocation;
	int64_t sizeChange;
      };

      virtual void fileMDChanged( Event *event ) = 0;
      virtual void fileMDRead( FileMD *obj ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Interface for a file visitor
  //----------------------------------------------------------------------------
  class IFileVisitor
  {
    public:
      virtual void visitFile( FileMD *file ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Interface for class responsible for managing the metadata information
  //! concerning files. It is responsible for assigning file IDs and managing
  //! storage of the metadata. Could be implemented as a change log or db based
  //! store or as an interface to memcached or some other caching system or
  //! key value store
  //----------------------------------------------------------------------------
  class IFileMDSvc
  {
    public:
      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~IFileMDSvc() {}

      //------------------------------------------------------------------------
      //! Initialize the file service
      //------------------------------------------------------------------------
      virtual void initialize() throw( MDException ) = 0;

      //------------------------------------------------------------------------
      //! Configure the file service
      //------------------------------------------------------------------------
      virtual void configure( std::map<std::string, std::string> &config )
        throw( MDException ) = 0;

      //------------------------------------------------------------------------
      //! Finalize the file service
      //------------------------------------------------------------------------
      virtual void finalize() throw( MDException ) = 0;

      //------------------------------------------------------------------------
      //! Get the file metadata information for the given file ID
      //------------------------------------------------------------------------
      virtual FileMD *getFileMD( FileMD::id_t id, uint64_t* clock) throw( MDException ) = 0;
      virtual FileMD *getFileMD( FileMD::id_t id) throw( MDException ) = 0;

      //------------------------------------------------------------------------
      //! Create new file metadata object with an assigned id, the user has
      //! to fill all the remaining fields
      //------------------------------------------------------------------------
      virtual FileMD *createFile() throw( MDException ) = 0;

      //------------------------------------------------------------------------
      //! Update the file metadata in the backing store after the FileMD object
      //! has been changed
      //------------------------------------------------------------------------
      virtual void updateStore( FileMD *obj ) throw( MDException ) = 0;

      //------------------------------------------------------------------------
      //! Remove object from the store
      //------------------------------------------------------------------------
      virtual void removeFile( FileMD *obj ) throw( MDException ) = 0;

      //------------------------------------------------------------------------
      //! Remove object from the store
      //------------------------------------------------------------------------
      virtual void removeFile( FileMD::id_t fileId ) throw( MDException ) = 0;

      //------------------------------------------------------------------------
      //! Get number of files
      //------------------------------------------------------------------------
      virtual uint64_t getNumFiles() const = 0;

      //------------------------------------------------------------------------
      //! Visit all the files
      //------------------------------------------------------------------------
      virtual void visit( IFileVisitor *visitor ) = 0;

      //------------------------------------------------------------------------
      //! Add file listener that will be notified about all of the changes in
      //! the store
      //------------------------------------------------------------------------
      virtual void addChangeListener( IFileMDChangeListener *listener ) = 0;

      //------------------------------------------------------------------------
      //! Notify the listeners about the change
      //------------------------------------------------------------------------
      virtual void notifyListeners( IFileMDChangeListener::Event *event ) = 0;
  };
}

#endif // EOS_NS_I_FILE_MD_SVC_HH
