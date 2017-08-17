
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
// Author: Lukasz Janyst <ljanyst@cern.ch>
// Date:   07.07.2010
// File:   Descriptor.hh
//------------------------------------------------------------------------------

#ifndef EOS_NS_DESCRIPTOR_HH
#define EOS_NS_DESCRIPTOR_HH

#include <exception>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace eos
{
  //----------------------------------------------------------------------------
  //! Socket exception
  //----------------------------------------------------------------------------
  class DescriptorException
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      DescriptorException()
      {
      }

      //------------------------------------------------------------------------
      //! Copy constructor
      //------------------------------------------------------------------------
      DescriptorException( const DescriptorException& ex )
      {
        pMsg << ex.pMsg.str();
      }

      //------------------------------------------------------------------------
      //! Get the associated stream
      //------------------------------------------------------------------------
      std::ostringstream &getMessage()
      {
        return pMsg;
      }

    private:
      std::ostringstream pMsg;
  };

  //----------------------------------------------------------------------------
  //! A Descriptor
  //----------------------------------------------------------------------------
  class Descriptor
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Descriptor():
        pFD( -1 ) {}

      //------------------------------------------------------------------------
      //! Create a Descriptor from a file descriptor
      //------------------------------------------------------------------------
      explicit Descriptor( int fd ):
        pFD( fd ) {}

      //------------------------------------------------------------------------
      //! Set descriptor
      //------------------------------------------------------------------------
      void setDescriptor( int fd )
      {
        pFD = fd;
      }

      //------------------------------------------------------------------------
      //! Get descriptor
      //------------------------------------------------------------------------
      int getDescriptor()
      {
        return pFD;
      }

      //------------------------------------------------------------------------
      //! Seek
      //------------------------------------------------------------------------
      off_t seek( off_t offset, int whence )
      {
        return ::lseek( pFD, offset, whence );
      }

      //------------------------------------------------------------------------
      //! Close the descriptor
      //------------------------------------------------------------------------
      void close();

      //------------------------------------------------------------------------
      //! Read the buffer from the blocking descriptor (socket, pipe), it won't
      //! return until all the requested data is read, or fail if it cannot
      //! be read
      //!
      //! @param buffer buffer pointer
      //! @param len    length of the buffer
      //------------------------------------------------------------------------
      void readBlocking( char *buffer, unsigned len );

      //------------------------------------------------------------------------
      //! Read the buffer from the non-blocking descriptor (file),
      //! it won't return until all the requested data is read, when it reaches
      //! the end it will wait for new data to be appended
      //!
      //! @param buffer buffer pointer
      //! @param len    length of the buffer
      //! @param poll   poll the descriptor every poll microseconds if the
      //!               sufficient amount of data is unavailable
      //!               an exception is thrown it poll is set to 0 and there
      //!               is no data anymore
      //------------------------------------------------------------------------
      void readNonBlocking( char *buffer, unsigned len, unsigned poll = 0 );

      //------------------------------------------------------------------------
      //! Read the buffer from the non-blocking descriptor (file)
      //! at given offset, it won't return until all the requested data is read,
      //! when it reaches the end it will wait for new data to be appended
      //!
      //! @param buffer buffer pointer
      //! @param len    length of the buffer
      //! @param offset offset
      //! @param poll   poll the descriptor every poll microseconds if the
      //!               sufficient amount of data is unavailable
      //!               an exception is thrown it poll is set to 0 and there
      //!               is no data anymore
      //------------------------------------------------------------------------
      void offsetReadNonBlocking( char *buffer, unsigned len, off_t offset,
                                  unsigned poll = 0 );

      //------------------------------------------------------------------------
      //! Try to read len bytes at offset
      //!
      //! @param  buffer buffer pointer
      //! @param  len    length of the buffer
      //! @param  offset offset
      //! @return number of available bytes
      //------------------------------------------------------------------------
       unsigned tryRead( char *buffer, unsigned len, off_t offset );

      //------------------------------------------------------------------------
      //! Write data to the descriptor
      //!
      //! @param buffer buffer pointer
      //! @param len    length of the buffer
      //------------------------------------------------------------------------
      void write( const char *buffer, unsigned len );

    protected:
      int pFD;
  };

  //----------------------------------------------------------------------------
  //! A network socket
  //----------------------------------------------------------------------------
  class Socket: public Descriptor
  {
    public:
      //------------------------------------------------------------------------
      //! Protocol
      //------------------------------------------------------------------------
      enum Protocol
        {
          TCP = 0,
          UDP
        };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Socket() {}

      //------------------------------------------------------------------------
      //! Create a socket from a descriptor
      //------------------------------------------------------------------------
      explicit Socket( int socket ):
        Descriptor( socket ) {}

      //------------------------------------------------------------------------
      //! Initialize the socket
      //!
      //! @param proto   protocol type
      //------------------------------------------------------------------------
      void init( Protocol proto );

      //------------------------------------------------------------------------
      //! Connect the socket
      //!
      //! @param address hostname or ip address of a server
      //! @param port    port number
      //------------------------------------------------------------------------
      void connect( const char *address, unsigned port );

      //------------------------------------------------------------------------
      //! Bind to the port
      //------------------------------------------------------------------------
      void bind( const char *address, unsigned port );

      //------------------------------------------------------------------------
      //! Listen to the incomming connections
      //!
      //! @param queue backlog queue size
      //------------------------------------------------------------------------
      void listen( unsigned queue = 20 );

      //------------------------------------------------------------------------
      //! Accept connections, allocates memory, the user takes ownership over
      //! The socket object
      //------------------------------------------------------------------------
      Socket *accept();

      //------------------------------------------------------------------------
      //! Close the socket
      //------------------------------------------------------------------------
      void close();

      //------------------------------------------------------------------------
      //! The same as the ones in the manual
      //------------------------------------------------------------------------
      void setsockopt( int level, int name, void *value, socklen_t len );

      //------------------------------------------------------------------------
      //! The same as the ones in the manual
      //------------------------------------------------------------------------
      void getsockopt( int level, int name, void *value, socklen_t &len );
  };
};

#endif // EOS_NS_DESCRIPTOR_HH
