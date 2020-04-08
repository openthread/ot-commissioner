# * Find Libevent (a cross event library) This module defines
#   LIBEVENT_INCLUDE_DIR, where to find Libevent headers LIBEVENT_LIB, Libevent
#   libraries Libevent_FOUND, If false, do not try to use libevent

set(Libevent_EXTRA_PREFIXES /usr/local /opt/local "$ENV{HOME}")
foreach(prefix ${Libevent_EXTRA_PREFIXES})
  list(APPEND Libevent_INCLUDE_PATHS "${prefix}/include")
  list(APPEND Libevent_LIB_PATHS "${prefix}/lib")
endforeach()

find_path(LIBEVENT_INCLUDE_DIR event.h PATHS ${Libevent_INCLUDE_PATHS})
find_library(
  LIBEVENT_LIB
  NAMES event
  PATHS ${Libevent_LIB_PATHS})
find_library(
  LIBEVENT_PTHREAD_LIB
  NAMES event_pthreads
  PATHS ${Libevent_LIB_PATHS})

if(LIBEVENT_LIB
   AND LIBEVENT_INCLUDE_DIR
   AND LIBEVENT_PTHREAD_LIB)
  set(Libevent_FOUND TRUE)
  set(LIBEVENT_LIB ${LIBEVENT_LIB} ${LIBEVENT_PTHREAD_LIB})
else()
  set(Libevent_FOUND FALSE)
endif()

if(Libevent_FOUND)
  if(NOT Libevent_FIND_QUIETLY)
    message(STATUS "Found libevent: ${LIBEVENT_LIB}")
  endif()
else()
  if(Libevent_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find libevent and libevent_pthread.")
  endif()
  message(STATUS "libevent and libevent_pthread NOT found.")
endif()

mark_as_advanced(LIBEVENT_LIB LIBEVENT_INCLUDE_DIR)

add_library(event_pthreads IMPORTED UNKNOWN)
set_target_properties(event_pthreads PROPERTIES IMPORTED_LOCATION
                                                ${LIBEVENT_INCLUDE_DIR})
set_target_properties(event_pthreads PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                "${LIBEVENT_PTHREAD_LIB}")

add_library(event_core IMPORTED UNKNOWN)
set_target_properties(event_core PROPERTIES IMPORTED_LOCATION
                                            ${LIBEVENT_INCLUDE_DIR})
set_target_properties(event_core PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                            "${LIBEVENT_LIB}")
