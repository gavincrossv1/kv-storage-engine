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

add_executable(core_stress_test EXCLUDE_FROM_ALL
    tests/core_stress_test.cpp
    common/option.cpp
    common/status.cpp
    db/db_impl.cpp
    disk/block_builder.cpp
    disk/block_iterator.cpp
    disk/builder.cpp
    disk/filterblock_builder.cpp
    disk/filterblock_reader.cpp
    disk/random_access_file.cpp
    disk/sstable.cpp
    disk/sstable_iterator.cpp
    disk/table_builder.cpp
    disk/wal.cpp
    disk/writable_file.cpp
    memory/hash.cpp
    memory/memtable.cpp
    memory/write_batch.cpp
    type/key.cpp
    type/slice.cpp
    util/bloom_filter.cpp
    util/encode.cpp
    util/util.cpp
)
target_compile_features(core_stress_test PRIVATE cxx_std_17)
target_link_libraries(core_stress_test PRIVATE pthread)

add_executable(wal_recovery_test EXCLUDE_FROM_ALL
    tests/wal_recovery_test.cpp
    common/option.cpp
    common/status.cpp
    db/db_impl.cpp
    disk/block_builder.cpp
    disk/block_iterator.cpp
    disk/builder.cpp
    disk/filterblock_builder.cpp
    disk/filterblock_reader.cpp
    disk/random_access_file.cpp
    disk/sstable.cpp
    disk/sstable_iterator.cpp
    disk/table_builder.cpp
    disk/wal.cpp
    disk/writable_file.cpp
    memory/hash.cpp
    memory/memtable.cpp
    memory/write_batch.cpp
    type/key.cpp
    type/slice.cpp
    util/bloom_filter.cpp
    util/encode.cpp
    util/util.cpp
)
target_compile_features(wal_recovery_test PRIVATE cxx_std_17)
target_link_libraries(wal_recovery_test PRIVATE pthread)


link_directories(${PROJECT_SOURCE_DIR}/lib)
link_libraries(common db disk memory network type util pthread)


add_executable(server main.cpp)
add_executable(db_bench db_bench.cpp)