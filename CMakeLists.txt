cmake_minimum_required(VERSION 3.10)
project(MemPoolV2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
add_executable(mempool_test UnitTest.cpp ${SOURCES} ${HEADERS})

# Find and link pthread
find_package(Threads REQUIRED)
target_link_libraries(mempool_test PRIVATE Threads::Threads)

# Set include directories
target_include_directories(mempool_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enable testing
enable_testing()
add_test(NAME mempool_test COMMAND mempool_test)
