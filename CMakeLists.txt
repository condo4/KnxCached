cmake_minimum_required (VERSION 3.0)
project (knxcached)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories("lib/include")
link_directories("lib")


file(GLOB LIB_SOURCES_FILES
    *.cpp
)

file(GLOB LIB_HEADERS_FILES
    *.h
)

find_package(OpenSSL)
if( OpenSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIRS})
    link_directories(${OPENSSL_LIBRARIES})
    add_definitions("-DWITH_SSL_SOCKET")
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
endif( OpenSSL_FOUND )

file(GLOB_RECURSE PublicHeader "include/*.h*")
add_custom_target(private_headers SOURCES ${LIB_HEADERS_FILES})
add_custom_target(public_headers SOURCES ${PublicHeader})

add_executable (knxcached ${LIB_SOURCES_FILES})
target_link_libraries(knxcached eibclient)
target_link_libraries(knxcached ${OPENSSL_LIBRARIES})

install(TARGETS knxcached RUNTIME DESTINATION /usr/bin)
install(PROGRAMS knxcache.py DESTINATION /usr/bin RENAME knxcache)
