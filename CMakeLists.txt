cmake_minimum_required(VERSION 3.11)


set(PROJECT_NAME UWPOpenIGTLink)
project(${PROJECT_NAME})

# Version
set(VERSION_MAJOR 1)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

#-----------------------------------------------------------------------------
add_subdirectory(OpenIGTLink)

#-----------------------------------------------------------------------------
add_subdirectory(${PROJECT_NAME})

#-----------------------------------------------------------------------------
option(BUILD_SAMPLE_UI "Build UWPOpenIGTLinkUI sample." ON)
if(BUILD_SAMPLE_UI)
  add_subdirectory(${PROJECT_NAME}UI)
endif()


