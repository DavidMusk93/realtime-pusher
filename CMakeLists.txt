cmake_minimum_required(VERSION 3.10)

project(realtime-pusher)

set(CMAKE_CXX_STANDARD 14)

include_directories(common)
include_directories(3rdparty/websocketpp)

aux_source_directory(common COMMON_SRCS)
aux_source_directory(notifier NOTIFIER_SRCS)

find_package(Boost 1.65.0 REQUIRED COMPONENTS system)
#if(NOT Boost_FOUND)
#    message(FATAL_ERROR "boost libraries is required for websocketpp")
#endif()
include_directories(${Boost_INCLUDE_DIRS})

#message(INFO ${Boost_LIBRARIES})
add_executable(notifier ${COMMON_SRCS} ${NOTIFIER_SRCS})
target_link_libraries(notifier pthread ${Boost_LIBRARIES})

aux_source_directory(collector COLLECTOR_SRCS)

add_executable(collector ${COMMON_SRCS} ${COLLECTOR_SRCS})
target_link_libraries(collector pthread mysqlcppconn)