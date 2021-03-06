# Copyright © 2013 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authored by: Thomas Voss <thomas.voss@canonical.com>

cmake_minimum_required(VERSION 2.8)

project(dbus-cpp)

# We haven't received version information via the packaging setup.
# For that, we try to determine sensible values on our own, ensuring
# plain old invocations to cmake still work as expected.
if (NOT DEFINED DBUS_CPP_VERSION_MAJOR)
  find_program(LSB_RELEASE lsb_release)
  execute_process(
    COMMAND ${LSB_RELEASE} -c -s
    OUTPUT_VARIABLE DISTRO_CODENAME
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # We explicitly ignore errors and only check if we are building for vivid.
  # For all other cases:
  #   - releases other than vivid
  #   - other distros
  #   - errors
  # we define the version to be 2.0.0
  if (${DISTRO_CODENAME} STREQUAL "vivid")
    set(DBUS_CPP_VERSION_MAJOR 4)
    set(DBUS_CPP_VERSION_MINOR 3)
    set(DBUS_CPP_VERSION_PATCH 0)
  else ()
    set(DBUS_CPP_VERSION_MAJOR 5)
    set(DBUS_CPP_VERSION_MINOR 0)
    set(DBUS_CPP_VERSION_PATCH 0)
  endif()
endif()

message(STATUS "${CMAKE_PROJECT_NAME} ${DBUS_CPP_VERSION_MAJOR}.${DBUS_CPP_VERSION_MINOR}.${DBUS_CPP_VERSION_PATCH}")

find_package(Boost COMPONENTS filesystem program_options system REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(DBUS dbus-1)
pkg_check_modules(PROCESS_CPP process-cpp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(cmake/EnableCoverageReport.cmake)

include(GNUInstallDirs)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall -Wextra -fvisibility=hidden -fPIC -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Werror -Wall -fno-strict-aliasing -fvisibility=hidden -fvisibility-inlines-hidden -Wextra -fPIC -pthread")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

# We leverage GCC's sanitize functionality
set(
  DBUS_CPP_SANITIZE None
  CACHE STRING "Enables sanitizer functionality if set to a value in {address, leak}")

if((${DBUS_CPP_SANITIZE} STREQUAL "address") OR (${DBUS_CPP_SANITIZE} STREQUAL "leak"))
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${DBUS_CPP_SANITIZE}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${DBUS_CPP_SANITIZE}")
endif()

#####################################################################
# Enable code coverage calculation with gcov/gcovr/lcov
# Usage:
#  * Switch build type to coverage (use ccmake or cmake-gui)
#  * Invoke make, make test, make coverage
#  * Find html report in subdir coveragereport
#  * Find xml report feasible for jenkins in coverage.xml
#####################################################################
IF(CMAKE_BUILD_TYPE MATCHES [cC][oO][vV][eE][rR][aA][gG][eE])
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage -fprofile-arcs" )
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftest-coverage -fprofile-arcs" )
  SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -ftest-coverage -fprofile-arcs" )
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -ftest-coverage -fprofile-arcs" )
ENDIF(CMAKE_BUILD_TYPE MATCHES [cC][oO][vV][eE][rR][aA][gG][eE])

include(CTest)

include_directories(
  include
  ${LIBXML2_INCLUDE_DIR}
)

add_subdirectory(data)
add_subdirectory(doc)
add_subdirectory(include)
add_subdirectory(src)

enable_coverage_report(benchmark)
