cmake_minimum_required(VERSION 3.10)

# set the project name
project(ThreadPool)

		   
#set(CMAKE_BINARY_DIR ${PROJECT_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}${CMAKE_CXX_COMPILER_VERSION}) 
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}${CMAKE_CXX_COMPILER_VERSION})
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}${CMAKE_CXX_COMPILER_VERSION})
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}${CMAKE_CXX_COMPILER_VERSION}) 
message("-- CMAKE_SYSTEM_INFO_FILE: ${CMAKE_SYSTEM_INFO_FILE}")
message("-- CMAKE_SYSTEM_NAME:      ${CMAKE_SYSTEM_NAME}")
message("-- CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message("-- CMAKE_SYSTEM:           ${CMAKE_SYSTEM}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
find_package(Threads REQUIRED)

# add library
add_library(ThreadPool STATIC src/ThreadPool.cc)
target_include_directories(ThreadPool PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/spdlog/include)
target_link_libraries(ThreadPool PUBLIC Threads::Threads)

if (UNIX)
target_link_libraries(ThreadPool PUBLIC rt)
endif()

# add the executable
add_executable(ThreadPoolTest ThreadPoolTest.cc)
target_include_directories(ThreadPoolTest PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ThreadPoolTest PRIVATE ThreadPool)

if (WIN32)
option(MINGW_STDTHREADS_GENERATE_STDHEADERS "" ON)
add_subdirectory(mingw-std-threads)
target_link_libraries(ThreadPool PRIVATE mingw_stdthreads)
target_link_libraries(ThreadPoolTest PRIVATE mingw_stdthreads)
endif()
