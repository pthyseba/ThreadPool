cmake_minimum_required(VERSION 3.10)

# set the project name
project(ThreadPool)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# add the executable
add_executable(ThreadPool ThreadPool.cc)

target_include_directories(ThreadPool PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(ThreadPool PRIVATE Threads::Threads rt)
