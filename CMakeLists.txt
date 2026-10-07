cmake_minimum_required(VERSION 3.31)

project(simcore LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(simcore src/sim.c)

target_include_directories(simcore PUBLIC ${PROJECT_SOURCE_DIR}/include)

target_compile_options(simcore PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
)

include(CTest)

if(BUILD_TESTING)
  add_executable(sim_tests tests/test_sim.c)

  target_link_libraries(sim_tests PRIVATE simcore)

  add_test(NAME sim_tests COMMAND sim_tests)
endif()
