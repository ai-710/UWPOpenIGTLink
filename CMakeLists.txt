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

# find_package(Git QUIET)
# message(${PROJECT_SOURCE_DIR})
# if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
  # # Update submodules as needed
    # option(GIT_SUBMODULE "Check submodules during build" ON)
    # if(GIT_SUBMODULE)
        # message(STATUS "Submodule update")
        # execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        # WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        # RESULT_VARIABLE GIT_SUBMOD_RESULT)
        # if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            # message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        # endif()
    # endif()
# endif()

#-----------------------------------------------------------------------------
add_subdirectory(${PROJECT_NAME})

#-----------------------------------------------------------------------------
if(BUILD_SAMPLE_UI)
  add_subdirectory(${PROJECT_NAME}UI)
endif()


