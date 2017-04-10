# Try to find ZMQ
# Once done this will define
#
#  ZMQ_FOUND        - system has ZMQ
#  ZMQ_INCLUDE_DIRS - the ZMQ include directories
#  ZMQ_LIBRARIES    - ZMQ library directories

if (ZMQ_LIBRARIES AND ZMQ_INCLUDE_DIRS)
  # Already in cache
  set(ZMQ_FIND_QUIETLY TRUE)
else (ZMQ_LIBRARIES AND ZMQ_INCLUDE_DIRS)

  find_path(ZMQ_INCLUDE_DIR NAMES zmq.h
    HINTS
    /usr
    /usr/local
    /opt/local
    PATH_SUFFIXES include
  )

  find_library(ZMQ_LIBRARY NAMES zmq
    HINTS
    /usr
    /usr/local
    /opt/local
    PATH_SUFFIXES lib
  )

  find_path(
    ZMQ_CPP_INCLUDE_DIR
    NAMES zmq.hpp
    HINTS ${ZMQ_ROOT_DIR}
    PATHS ${CMAKE_SOURCE_DIR}/utils
    PATH_SUFFIXES include)

  # Set variable in case we are using our own ZMQ C++ bindings
  if(NOT "${ZMQ_CPP_INCLUDE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}/utils")
    add_definitions(-DHAVE_DEFAULT_ZMQ)
  endif()

  set(ZMQ_LIBRARIES ${ZMQ_LIBRARY})
  set(ZMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIR} ${ZMQ_CPP_INCLUDE_DIR})

  # handle the QUIETLY and REQUIRED arguments and set ZMQ_FOUND to TRUE if
  # all listed variables are TRUE
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(ZMQ DEFAULT_MSG ZMQ_LIBRARY ZMQ_INCLUDE_DIR)

  mark_as_advanced(ZMQ_INCLUDE_DIR ZMQ_LIBRARY)

endif (ZMQ_LIBRARIES AND ZMQ_INCLUDE_DIRS)
