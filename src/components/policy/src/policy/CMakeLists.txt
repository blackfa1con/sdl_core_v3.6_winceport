# Copyright (c) 2013, Ford Motor Company
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following
# disclaimer in the documentation and/or other materials provided with the
# distribution.
#
# Neither the name of the Ford Motor Company nor the names of its contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

set(target Policy)
set(install_destination bin)
set(copy_destination ${CMAKE_BINARY_DIR}/src/appMain)
set(library_name ${CMAKE_SHARED_LIBRARY_PREFIX}${target}${CMAKE_SHARED_LIBRARY_SUFFIX})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if (EXTENDED_POLICY_FLAG)
  add_definitions(-DEXTENDED_POLICY)
endif()

#Generation of policy table interface...
#include(${CMAKE_SOURCE_DIR}/tools/intergen/GenerateInterfaceLibrary.cmake)

include_directories (
  ./include
  ./usage_statistics/include
  ${CMAKE_SOURCE_DIR}/src/components/rpc_base/include
  ${CMAKE_SOURCE_DIR}/src/thirdPartyLibs/jsoncpp/include
  #${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src/components/utils/include/
  ${CMAKE_SOURCE_DIR}/src/components/config_profile/include
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "WindowsCE")
include_directories(
  ${CMAKE_SOURCE_DIR}/src/thirdPartyLibs/sqlite3
)
endif()

set(SOURCES
  ./src/policy_manager_impl.cc
  ./src/policy_helper.cc
  ./src/policy_table.cc
  ./src/sql_pt_queries.cc
  ./src/sql_pt_representation.cc
)

if (EXTENDED_POLICY_FLAG)
  list(APPEND SOURCES
    ./src/sql_pt_ext_queries.cc
    ./src/sql_pt_ext_representation.cc
  )
endif ()

add_subdirectory(usage_statistics)

if (EXTENDED_POLICY_FLAG)
add_subdirectory(policy_table/table_struct_ext)
include_directories(./policy_table/table_struct_ext)
else ()
include_directories(./policy_table/table_struct)
add_subdirectory(policy_table/table_struct)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
set(LIBRARIES ConfigProfile policy_struct jsoncpp Utils)
else()
set(LIBRARIES ConfigProfile policy_struct dbms jsoncpp Utils)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "QNX")
  # --- QDB Wrapper
  include_directories (qdb_wrapper/include)
  add_subdirectory(qdb_wrapper)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  include_directories (sqlite_wrapper/include)
  add_subdirectory(sqlite_wrapper)
  list(APPEND LIBRARIES sqlite3)
  list(APPEND LIBRARIES pthreadVC2)
elseif (CMAKE_SYSTEM_NAME STREQUAL "WindowsCE")
  include_directories (sqlite_wrapper/include)
  add_subdirectory(sqlite_wrapper)
  list(APPEND LIBRARIES sqlite3)
  list(APPEND LIBRARIES pthread)
  list(APPEND LIBRARIES Utils)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
  include_directories (sqlite_wrapper/include)
else ()
  # --- SQLite Wrapper
  include_directories (sqlite_wrapper/include)
  add_subdirectory(sqlite_wrapper)
  list(APPEND LIBRARIES sqlite3)
endif ()

if(MODIFY_FUNCTION_FLAGS)
if(ENABLE_LOG)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "QNX" OR CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Android" OR CMAKE_SYSTEM_NAME STREQUAL "WindowsCE")
  list(APPEND LIBRARIES log4cxx)
endif()
endif()
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "WindowsCE")
add_library(${target} ${SOURCES})
else()
add_library(${target} SHARED ${SOURCES})
endif()

target_link_libraries(${target} ${LIBRARIES} )

if(ENABLE_LOG)
if(MODIFY_FUNCTION_FLAGS)
else()
  add_dependencies(${target} liblog4cxx)
  target_link_libraries(${target} log4cxx -L${LOG4CXX_LIBS_DIRECTORY})
endif()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsCE")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
add_custom_target(copy_library_${target} ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${library_name}
    ${copy_destination}
    DEPENDS ${target}
    COMMENT "Copying library ${library_name}")
else()
add_custom_target(copy_library_${target} ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/${library_name}
    ${copy_destination}
    DEPENDS ${target}
    COMMENT "Copying library ${library_name}")
endif()


if (BUILD_TESTS_WITH_HMI)
  add_custom_target(test_copy_library_${target} ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_BINARY_DIR}/${library_name}
      ${CMAKE_BINARY_DIR}/test/
      DEPENDS ${target}
      COMMENT "Copying library ${library_name}")
endif(BUILD_TESTS_WITH_HMI)

install(TARGETS ${target}
  DESTINATION ${install_destination}
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)
endif()
