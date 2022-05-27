// ----------------------------------------------------------------------
// File: RestApiManager.cc
// Author: Cedric Caffy - CERN
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

#include "RestApiManager.hh"
#include "mgm/http/rest-api/handler/factory/WellKnownRestHandlerFactory.hh"
#include "mgm/http/rest-api/utils/URLParser.hh"

EOSMGMRESTNAMESPACE_BEGIN

RestApiManager::RestApiManager()
{
  mTapeRestApiConfig = std::make_unique<TapeRestApiConfig>();
  mMapAccessURLRestHandlerFactory[mTapeRestApiConfig->getAccessURL()] =
    std::make_unique<TapeRestHandlerFactory>(mTapeRestApiConfig.get());
  mMapAccessURLRestHandlerFactory[getWellKnownAccessURL()] =
    std::make_unique<WellKnownRestHandlerFactory>(this);
}

bool RestApiManager::isRestRequest(const std::string& requestURL) const
{
  const auto& restHandler = getRestHandler(requestURL);
  //We do not need an error message in the caller of this method.
  //If we need this one day, one may need to add this parameter to RestApiManager::isRestRequest()
  std::string errorMsg;
  return (restHandler != nullptr &&
          restHandler->isRestRequest(requestURL, errorMsg));
}

TapeRestApiConfig* RestApiManager::getTapeRestApiConfig() const
{
  return mTapeRestApiConfig.get();
}

std::unique_ptr<rest::RestHandler> RestApiManager::getRestHandler(
  const std::string& requestURL) const
{
  const auto& restHandlerFactory = std::find_if(
                                     mMapAccessURLRestHandlerFactory.begin(),
  mMapAccessURLRestHandlerFactory.end(), [&requestURL](const auto & kv) {
    URLParser parser(requestURL);
    return parser.startsBy(kv.first);
  });

  if (restHandlerFactory != mMapAccessURLRestHandlerFactory.end()) {
    return restHandlerFactory->second->createRestHandler();
  }

  return nullptr;
}

const std::string RestApiManager::getWellKnownAccessURL() const
{
  return "/.well-known/";
}

EOSMGMRESTNAMESPACE_END
