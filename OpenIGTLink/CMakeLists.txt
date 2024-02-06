cmake_minimum_required(VERSION 3.11)

project(OpenIGTLink)

# Configuration
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/install" CACHE PATH "Install path prefix" FORCE)
endif()


# Dependencies 
include(FetchContent)
set(BUILD_SHARED_LIBS OFF)
set(OpenIGTLink_BUILD_TESTING OFF)
set(BUILD_EXAMPLES OFF)

# Target
FetchContent_Declare(OpenIGTLink
  GIT_REPOSITORY https://github.com/openigtlink/OpenIGTLink.git
  GIT_TAG master
  INSTALL_DIR ${CMAKE_BINARY_DIR}
)

FetchContent_MakeAvailable(OpenIGTLink)