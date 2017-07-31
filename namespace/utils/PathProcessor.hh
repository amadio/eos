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
// desc:   ChangeLog like store
//------------------------------------------------------------------------------

#ifndef EOS_NS_PATH_PROCESSOR_HH
#define EOS_NS_PATH_PROCESSOR_HH

#include <cstring>
#include <vector>
#include <string>

namespace eos
{
  //----------------------------------------------------------------------------
  //! Helper class responsible for spliting the path
  //----------------------------------------------------------------------------
  class PathProcessor
  {
    public:
      //------------------------------------------------------------------------
      //! Split the path and put its elements in a vector, the tokens are
      //! copied, the buffer is not overwritten
      //------------------------------------------------------------------------
      static void splitPath( std::vector<std::string> &elements,
                             const std::string &path )
      {
        elements.clear();
        std::vector<char*> elems;
        char buffer[path.length()+1];
        strcpy( buffer, path.c_str() );
        splitPath( elems, buffer );
        for( size_t i = 0; i < elems.size(); ++i )
          elements.push_back( elems[i] );
      }

      //------------------------------------------------------------------------
      //! Split the path and put its element in a vector, the split is done
      //! in-place and the buffer is overwritten
      //------------------------------------------------------------------------
      static void splitPath( std::vector<char*> &elements, char *buffer )
      {
        elements.clear();
        elements.reserve( 10 );

        char *cursor = buffer;
        char *beg    = buffer;

        //----------------------------------------------------------------------
        // Go by the characters one by one
        //----------------------------------------------------------------------
        while( *cursor )
        {
          if( *cursor == '/' )
          {
            *cursor = 0;
            if( beg != cursor )
              elements.push_back( beg );
            beg = cursor+1;
          }
          ++cursor;
        }

        if( beg != cursor )
          elements.push_back( beg );
      }

      //------------------------------------------------------------------------
      //! Absolute Path sanitizing all '/../' and '/./' entries
      //------------------------------------------------------------------------
      static void absPath(std::string& mypath)
      {
        std::string path = mypath;
        std::string abspath;
        size_t rpos = 4096;
        size_t bppos;

        // remove /../ from front
        while ((bppos = path.find("/../")) != std::string::npos) {
          size_t spos = path.rfind("/", bppos - 1);

          if (spos != std::string::npos) {
            path.erase(bppos, 4);
            path.erase(spos + 1, bppos - spos - 1);
          } else {
            path = "/";
            break;
          }
        }

        while ((rpos = path.rfind("/", rpos)) != std::string::npos) {
          rpos--;
          std::string tp = path.substr(rpos + 1);
          path.erase(rpos + 1);

          if (tp == "/") {
            continue;
          }

          if (tp == "/.") {
            continue;
          }

          abspath.insert(0, tp);

          if (rpos <= 0) {
            break;
          }
        }

        mypath = abspath;
      }
  };
}

#endif // EOS_NS_PATH_PROCESSOR_HH
