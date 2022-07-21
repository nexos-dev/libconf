#[[
    CMakeLists.txt - contains confparse build system
    Copyright 2021, 2022 The NexNix Project

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
]]

cmake_minimum_required(VERSION 3.00)
project(libconf VERSION 0.0.1)
enable_language(C)

# Options
option(LIBCONF_ENABLE_TESTS "Specifies if the test suite should be built" OFF)
option(BUILD_SHARED_LIBS "Specifies if shared libraries should be built" OFF)
OPTION(LIBCONF_LINK_DEPS "Specifies if dependencies should automatically be linked into libconf")

# Enable testing
if(${LIBCONF_ENABLE_TESTS} AND NOT ${CMAKE_CROSSCOMPILING})
    enable_testing()
endif()

# Check if we should use the SDK or not
if(CMAKE_SYSTEM_NAME STREQUAL "NexNix")
    find_package(NexNixSdk REQUIRED)
else()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    include_directories(cmake)
endif()

if(LIBCONF_LINK_DEPS)
    find_package(LibNex REQUIRED)
    find_package(LibChardet REQUIRED)
endif()

include(GNUInstallDirs)
include(NexTest)
include(SdkCompilerTest)
include(CMakePackageConfigHelpers)

if(${LIBCONF_ENABLE_TESTS})
    nextest_enable_tests()
endif()

# Configure system dependent stuff
check_library_visibility(HAVE_DECLSPEC_EXPORT HAVE_VISIBILITY)
configure_file(src/libconf_config.in.h ${CMAKE_BINARY_DIR}/libconf/libconf_config.h)
include_directories(${CMAKE_BINARY_DIR})

list(APPEND CONF_SOURCES src/conf.c src/lex.c src/parse.c)

# Create the library
add_library(conf ${CONF_SOURCES})
target_include_directories(conf PRIVATE src)
target_include_directories(conf PUBLIC include)

# Set SOName
set_target_properties(conf PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

if (LIBCONF_LINK_DEPS)
    # Link with libnex and libchardet
    target_link_libraries(conf PUBLIC LibNex::nex LibChardet::chardet)
else()
    target_link_libraries(conf PUBLIC nex chardet)
endif()

# Install it
install(TARGETS conf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/libconf.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libconf)
install(FILES ${CMAKE_BINARY_DIR}/libconf/libconf_config.h 
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libconf)

# Figure out library type
if(BUILD_SHARED_LIBS)
    set(LIBCONF_LIBTYPE "SHARED")
    set(LIBCONF_LIBRARY_FILE 
        "${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}conf${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(LIBCONF_HAVE_SONAME TRUE)
    set(LIBCONF_SONAME "${CMAKE_SHARED_LIBRARY_PREFIX}conf${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJECT_VERSION_MAJOR}")
else()
    set(LIBCONF_LIBTYPE "STATIC")
    set(LIBCONF_HAVE_SONAME FALSE)
    set(LIBCONF_LIBRARY_FILE 
        "${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}conf${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

# Configure package configuration file
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/data/LibConfConfig.cmake.in 
                              ${CMAKE_BINARY_DIR}/LibConfConfig.cmake
                              INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake
                              PATH_VARS CMAKE_INSTALL_INCLUDEDIR)

write_basic_package_version_file(${CMAKE_BINARY_DIR}/LibConfConfigVersion.cmake
                                 COMPATIBILITY AnyNewerVersion)

# Install cmake package files
install(FILES ${CMAKE_BINARY_DIR}/LibConfConfig.cmake 
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake)
install(FILES ${CMAKE_BINARY_DIR}/LibConfConfigVersion.cmake 
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake)

# Setup test cases
list(APPEND CONF_TESTS lex parse)

foreach(test ${CONF_TESTS})
    nextest_add_library_test(NAME ${test}
                             SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/${test}.c
                             LIBS conf
                             INCLUDES ${CMAKE_BINARY_DIR} 
                                      ${CMAKE_CURRENT_SOURCE_DIR}/src
                                      ${CMAKE_CURRENT_SOURCE_DIR}/include
                             WORKDIR ${CMAKE_CURRENT_SOURCE_DIR}/src/tests
                             LINK_LANG CXX)
endforeach()
