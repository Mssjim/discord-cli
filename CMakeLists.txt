cmake_minimum_required(VERSION 3.15)
project(discord-cli C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(SOURCES
    src/main.c
    src/config.c
    src/discord/state.c
    src/discord/api.c
    src/discord/gateway.c
    src/ui/terminal.c
    src/ui/views.c
    lib/cjson/cJSON.c
)

add_executable(discord-cli ${SOURCES})

set_target_properties(discord-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)

target_include_directories(discord-cli PRIVATE
    src
    lib/cjson
)

if(WIN32)
    target_compile_definitions(discord-cli PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(discord-cli PRIVATE winhttp)
    if(MSVC)
        target_compile_options(discord-cli PRIVATE /W3 /D_CRT_SECURE_NO_WARNINGS)
    elseif(MINGW)
        target_compile_definitions(discord-cli PRIVATE MINGW_HAS_SECURE_API=1)
        target_compile_options(discord-cli PRIVATE -Wall -Wno-unknown-pragmas)
        target_link_options(discord-cli PRIVATE -static-libgcc -s)
    endif()
else()
    target_link_libraries(discord-cli PRIVATE pthread)
endif()
