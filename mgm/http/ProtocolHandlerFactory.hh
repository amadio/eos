// ----------------------------------------------------------------------
// File: ProtocolHandlerFactory.hh
// Author: Justin Lewis Salmon - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2013 CERN/Switzerland                                  *
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

/**
 * @file   ProtocolHandlerFactory.hh
 *
 * @brief  Factory class to create an appropriate protocol handler for the
 *         MGM.
 */

#ifndef __EOSMGM_PROTOCOLHANDLERFACTORY__HH__
#define __EOSMGM_PROTOCOLHANDLERFACTORY__HH__

/*----------------------------------------------------------------------------*/
#include "mgm/Namespace.hh"
#include "mgm/http/HttpHandler.hh"
#include "mgm/http/s3/S3Handler.hh"
#include "mgm/http/webdav/WebDAVHandler.hh"
#include "common/http/ProtocolHandlerFactory.hh"
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
#include <map>
#include <string>
/*----------------------------------------------------------------------------*/

EOSMGMNAMESPACE_BEGIN

class ProtocolHandlerFactory : public eos::common::ProtocolHandlerFactory
{
public:

  ProtocolHandlerFactory () {};
  virtual ~ProtocolHandlerFactory () {};

  /**
   * Factory function to create an appropriate object which will handle this
   * request based on the method and headers.
   *
   * @param method  the request verb used by the client (GET, PUT, etc)
   * @param headers the map of request headers
   * @param vid     the mapped virtual identity of this client
   *
   * @return a concrete ProtocolHandler, or NULL if no matching protocol found
   */
  eos::common::ProtocolHandler*
  CreateProtocolHandler (const std::string                     &method,
                         std::map<std::string, std::string>    &headers,
                         eos::common::VirtualIdentity          *vid)
  {
    if (S3Handler::Matches(method, headers))
    {
      return new S3Handler(vid);
    }
    else if (WebDAVHandler::Matches(method, headers))
    {
      return new WebDAVHandler(vid);
    }
    else if (HttpHandler::Matches(method, headers))
    {
      return new HttpHandler(vid);
    }

    else return NULL;
  };
};

/*----------------------------------------------------------------------------*/
EOSMGMNAMESPACE_END

#endif /* __EOSMGM_PROTOCOLHANDLERFACTORY__HH__ */
