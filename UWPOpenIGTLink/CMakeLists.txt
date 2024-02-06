#-----------------------------------------------------------------------------
# Set the project name
project(${PROJECT_NAME} LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable precompiled headers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Yu\"pch.h\"")

#-----------------------------------------------------------------------------
file(GLOB_RECURSE SRC "*.cxx" "*.cpp" "Content/*.cxx" "Content/Data/*.cpp")
file(GLOB_RECURSE HEADERS "*.h" "Content/*.h" "Content/Data/*.h")

add_library(${PROJECT_NAME} SHARED ${SRC})

#-----------------------------------------------------------------------------
# Set the precompiled header file
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

#-----------------------------------------------------------------------------
# Include the headers directory
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}" VS_WINRT_COMPONENT TRUE VS_GLOBAL_ROOTNAMESPACE "${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/Content" "${CMAKE_CURRENT_SOURCE_DIR}/Content/Data")
target_link_libraries(${PROJECT_NAME} OpenIGTLink)


