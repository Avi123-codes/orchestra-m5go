# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)
FILE(GLOB_RECURSE app_headers ${CMAKE_SOURCE_DIR}/include/*.h)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include
    REQUIRES driver esp_wifi nvs_flash
)
