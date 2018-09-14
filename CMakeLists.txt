cmake_minimum_required (VERSION 3.0)
project (knxcached)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories("lib/include")
link_directories("lib")

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

find_package (Threads)

file(GLOB LIB_SOURCES_FILES
    src/*.cpp
)

file(GLOB LIB_HEADERS_FILES
    src/*.h
)

file(GLOB_RECURSE PublicHeader "include/*.h*")
add_custom_target(private_headers SOURCES ${LIB_HEADERS_FILES})
add_custom_target(public_headers SOURCES ${PublicHeader})

add_executable (knxcached ${LIB_SOURCES_FILES})
target_link_libraries(knxcached ${LIBXML2_LIBRARIES} eibclient zmq ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS knxcached RUNTIME DESTINATION /usr/bin)
