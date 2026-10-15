cmake_minimum_required(VERSION 3.20)
project(raylib_feature_lab C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The official raylib 6.0 MSVC library imports the dynamic Universal CRT.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

set(RAYLIB_ROOT "${CMAKE_SOURCE_DIR}/third_party/raylib-6.0_win64_msvc16")

add_library(raylib STATIC IMPORTED)
set_target_properties(raylib PROPERTIES
    IMPORTED_LOCATION "${RAYLIB_ROOT}/lib/raylib.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_ROOT}/include"
)

file(GLOB_RECURSE LIB_SOURCES "lib/*.c")
file(GLOB_RECURSE LIB_HEADERS "lib/*.h")

add_executable(feature_lab src/main.c ${LIB_SOURCES} ${LIB_HEADERS})
target_include_directories(feature_lab PRIVATE "${CMAKE_SOURCE_DIR}/lib" "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(feature_lab PRIVATE raylib winmm gdi32 opengl32 user32 shell32)

add_executable(platformer src/platformer.c ${LIB_SOURCES} ${LIB_HEADERS})
target_include_directories(platformer PRIVATE "${CMAKE_SOURCE_DIR}/lib" "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(platformer PRIVATE raylib winmm gdi32 opengl32 user32 shell32)

add_executable(cardpress
    src/cards/cards.c
    src/cards/card_doc.c
    src/cards/card_draw.c
    src/cards/card_export.c
    src/cards/card_dialog.c
)
target_include_directories(cardpress PRIVATE "${CMAKE_SOURCE_DIR}/src/cards" "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(cardpress PRIVATE raylib winmm gdi32 opengl32 user32 shell32 comdlg32)

if(MSVC)
    target_compile_options(feature_lab PRIVATE /W3)
    target_compile_options(platformer PRIVATE /W3)
    target_compile_options(cardpress PRIVATE /W3)
    target_compile_definitions(cardpress PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
