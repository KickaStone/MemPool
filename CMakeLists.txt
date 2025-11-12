cmake_minimum_required(VERSION 3.10)
project(MemPoolV2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set compiler flags based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g0 -O3 -march=native -mtune=native -flto")
endif()

# Add source files
set(SOURCES
    ThreadCache.cpp
    CentralCache.cpp
    PageCache.cpp
)

# Add header files
set(HEADERS
    ConcurrentAlloc.h
    ThreadCache.h
    CentralCache.h
    PageCache.h
    Common.h
    ObjectPool.h
    RadixTree.h
)

add_compile_options(-g)

add_executable(benchmark benchmark.cpp ${SOURCES} ${HEADERS})

# Create executable for testing
add_executable(unit_test UnitTest.cpp ${SOURCES} ${HEADERS})

add_executable(object_pool_test ObjectpoolUnitTest.cpp ${SOURCES} ${HEADERS})

# Find and link pthread
find_package(Threads REQUIRED)
target_link_libraries(unit_test PRIVATE Threads::Threads)
target_link_libraries(object_pool_test PRIVATE Threads::Threads)

# Set include directories
target_include_directories(unit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enable testing
enable_testing()
add_test(NAME mempool_test COMMAND mempool_test)
