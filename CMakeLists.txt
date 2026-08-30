cmake_minimum_required(VERSION 3.15)
project(Servers CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

set(CORE_SOURCES
    src/models.cpp
    src/result.cpp
    src/sha256.cpp
    src/persistence.cpp
    src/admin.cpp
    src/user.cpp
    src/command_handlers.cpp
    src/session_manager.cpp
)

add_executable(admin_server src/admin_server.cpp ${CORE_SOURCES})
target_link_libraries(admin_server)
if (WIN32)
    target_link_libraries(admin_server ws2_32)
elseif (UNIX)
    target_link_libraries(admin_server pthread dl)
endif()

add_executable(user_server src/user_server.cpp ${CORE_SOURCES})
target_link_libraries(user_server)
if (WIN32)
    target_link_libraries(user_server ws2_32)
elseif (UNIX)
    target_link_libraries(user_server pthread dl)
endif()