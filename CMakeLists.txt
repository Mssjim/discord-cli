cmake_minimum_required(VERSION 3.15)
project(discord-cli C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

target_include_directories(discord-cli PRIVATE
    src
    lib/cjson
)

if(WIN32)
    target_link_libraries(discord-cli PRIVATE winhttp)
    if(MSVC)
        target_compile_options(discord-cli PRIVATE /W3 /D_CRT_SECURE_NO_WARNINGS)
    endif()
else()
    target_link_libraries(discord-cli PRIVATE pthread)
endif()

