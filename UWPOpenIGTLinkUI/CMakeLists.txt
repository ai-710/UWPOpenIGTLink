# UWPOpenIGTLinkUI sample app
set(PROJ_NAME "UWPOpenIGTLinkUI")
project(${PROJ_NAME})

# Source files
set(SOURCE_FILES
  App.xaml.cpp
  IGTLConnectorPage.xaml.cpp
  pch.cpp)

# Header files
set(HEADER_FILES
  App.xaml.h
  IGTLConnectorPage.xaml.h
  pch.h)

# Content files
set(CONTENT_FILES Package.appxmanifest) 

# Asset files
set(ASSET_FILES
  Assets/glossy-green-button-2400px.png
  Assets/glossy-red-button-2400px.png
  Assets/glossy-yellow-button-2400px.png
	Assets/LockScreenLogo.scale-200.png
	Assets/SplashScreen.scale-200.png
	Assets/Square44x44Logo.scale-200.png
	Assets/Square44x44Logo.targetsize-24_altform-unplated.png
	Assets/Square150x150Logo.scale-200.png
	Assets/StoreLogo.png
	Assets/Wide310x150Logo.scale-200.png
)

# Resource files
set(RESOURCE_FILES
  ${CONTENT_FILES} ${ASSET_FILES})

# XAML files
set(XAML_FILES
  App.xaml
  IGTLConnectorPage.xaml)

set_source_files_properties(App.xaml PROPERTIES VS_XAML_TYPE "ApplicationDefinition")

set_source_files_properties(App.xaml.h PROPERTIES OBJECT_DEPENDS "App.xaml")
set_source_files_properties(IGTLConnectorPage.xaml.h PROPERTIES OBJECT_DEPENDS "IGTLConnectorPage.xaml")

set_property(SOURCE ${CONTENT_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
set_property(SOURCE ${ASSET_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
set_property(SOURCE ${ASSET_FILES} PROPERTY VS_DEPLOYMENT_LOCATION "Assets")

# UWP executable
add_executable(${PROJ_NAME} WIN32 ${SOURCE_FILES} ${HEADER_FILES} ${RESOURCE_FILES} ${XAML_FILES})

set_source_files_properties(pch.cpp PROPERTIES COMPILE_FLAGS "/Ycpch.h")
set_target_properties(${PROJ_NAME} PROPERTIES COMPILE_FLAGS "/Yupch.h")

set_target_properties(${PROJ_NAME} PROPERTIES VS_WINRT_COMPONENT TRUE FOLDER samples)
set_target_properties(${PROJ_NAME} PROPERTIES VS_GLOBAL_ROOTNAMESPACE ${PROJ_NAME})

target_include_directories(${PROJ_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(${PROJ_NAME} OpenIGTLink UWPOpenIGTLink)

# Output Dirs
# set_target_properties(${PROJ_NAME} PROPERTIES "ARCHIVE_OUTPUT_DIRECTORY" "${CMAKE_CURRENT_BINARY_DIR}/")
# set_target_properties(${PROJ_NAME} PROPERTIES "LIBRARY_OUTPUT_DIRECTORY" "${CMAKE_CURRENT_BINARY_DIR}/")
# set_target_properties(${PROJ_NAME} PROPERTIES "RUNTIME_OUTPUT_DIRECTORY" "${CMAKE_CURRENT_BINARY_DIR}/")


