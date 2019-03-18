# - Try to find JACK
# Once done, this will define
#
#  JACK_FOUND - system has JACK
#  JACK_INCLUDE_DIRS - the JACK include directories
#  JACK_LIBRARIES - link these to use JACK
#
# See documentation on how to write CMake scripts at
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries

include(LibFindMacros)

libfind_pkg_check_modules(JACK jackd2)

find_path(JACK_INCLUDE_DIR
  NAMES jack/jack.h
  PATHS ${JACK_INCLUDE_DIRS}
)

find_library(JACK_LIBRARY
  NAMES jack
  PATHS ${JACK_LIBRARY_DIRS}
)

set(JACK_PROCESS_INCLUDES JACK_INCLUDE_DIR)
set(JACK_PROCESS_LIBS JACK_LIBRARY)
libfind_process(JACK)

