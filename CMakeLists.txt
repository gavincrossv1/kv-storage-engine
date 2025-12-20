cmake_minimum_required(VERSION 3.15)
project(leveldb)

include_directories(${PROJECT_SOURCE_DIR}/common)
include_directories(${PROJECT_SOURCE_DIR}/db)
include_directories(${PROJECT_SOURCE_DIR}/disk)
include_directories(${PROJECT_SOURCE_DIR}/memory)
include_directories(${PROJECT_SOURCE_DIR}/network)
include_directories(${PROJECT_SOURCE_DIR}/type)
include_directories(${PROJECT_SOURCE_DIR}/util)

add_subdirectory(common)
add_subdirectory(db)
add_subdirectory(disk)
add_subdirectory(memory)
add_subdirectory(network)
add_subdirectory(type)
add_subdirectory(util)


link_directories(${PROJECT_SOURCE_DIR}/lib)
link_libraries(common db disk memory network type util pthread)


add_executable(server main.cpp)
add_executable(db_bench db_bench.cpp)