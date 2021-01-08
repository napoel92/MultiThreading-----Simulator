cmake_minimum_required(VERSION 3.0.0)
project(Simulator VERSION 0.1.0)

include(CTest)
enable_testing()


#set(MTM_FLAGS_DEBUG "-std=c++11 -Wall -Werror -pedantic-errors")
#set(MTM_FLAGS_RELEASE "${MTM_FLAGS_DEBUG} -DNDEBUG")
#set(CMAKE_CXX_FLAGS ${MTM_FLAGS_DEBUG})


add_executable(Simulator core_api.cpp core_api.h main.c sim_api.c sim_api.h)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
