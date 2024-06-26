# ----------------------------------------------------------------------
# File: CMakeLists.txt
# Author: Elvin-Alin Sindrilaru - CERN
# ----------------------------------------------------------------------

# ************************************************************************
# * EOS - the CERN Disk Storage System                                   *
# * Copyright (C) 2011 CERN/Switzerland                                  *
# *                                                                      *
# * This program is free software: you can redistribute it and/or modify *
# * it under the terms of the GNU General Public License as published by *
# * the Free Software Foundation, either version 3 of the License, or    *
# * (at your option) any later version.                                  *
# *                                                                      *
# * This program is distributed in the hope that it will be useful,      *
# * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
# * GNU General Public License for more details.                         *
# *                                                                      *
# * You should have received a copy of the GNU General Public License    *
# * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
# ************************************************************************

include_directories(${CMAKE_SOURCE_DIR})

#-------------------------------------------------------------------------------
# Generate protocol buffer files
#-------------------------------------------------------------------------------
PROTOBUF_GENERATE_CPP(CONFIG_SRCS CONFIG_HDRS ConfigProto.proto)
PROTOBUF_GENERATE_CPP(RESULT_SRCS RESULT_HDRS ResultProto.proto)
set_source_files_properties(
  ${CONFIG_SRCS} ${CONFIG_HDRS} ${RESULT_SRCS} ${RESULT_HDRS}
  PROPERTIES GENERATED 1)

#-------------------------------------------------------------------------------
# eos-io-benchmark executable
#-------------------------------------------------------------------------------
add_executable(eos-io-benchmark
  eosbenchmark.cc     eosbenchmark.hh
  Configuration.cc    Configuration.hh
  DirEos.cc           DirEos.hh
  FileEos.cc          FileEos.hh
  Result.cc           Result.hh
  ProtoIo.cc          ProtoIo.hh
  ${CONFIG_SRCS}      ${CONFIG_HDRS}
  ${RESULT_SRCS}      ${RESULT_HDRS}
  ${CMAKE_SOURCE_DIR}/common/StringConversion.cc
  ${CMAKE_SOURCE_DIR}/common/StringConversion.hh
  ${CMAKE_SOURCE_DIR}/fst/io/FileIoPlugin.cc)

target_link_libraries(eos-io-benchmark PRIVATE
  EosFstIo
  XROOTD::SERVER
  ${CMAKE_THREAD_LIBS_INIT})

target_compile_definitions(eos-io-benchmark PUBLIC
  -D_LARGE_FILE_SOURCE -D_LARGE64_FILE_SOURCE -D_FILE_OFFSET_BITS=64)

install(TARGETS eos-io-benchmark
  RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_SBINDIR})
