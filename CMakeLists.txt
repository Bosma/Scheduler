cmake_minimum_required(VERSION 3.7)
project(BosmaScheduler)

set(CMAKE_CXX_STANDARD 11)

include_directories(ccronexpr)

# threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(example example.cpp ccronexpr/ccronexpr.c)
target_link_libraries(example Threads::Threads)