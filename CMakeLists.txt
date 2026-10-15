cmake_minimum_required(VERSION 3.31)

project(simcore LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SIM_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_library(simcore src/sim.c src/event_queue.c)

target_include_directories(simcore
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/internal
)

target_compile_options(simcore PRIVATE ${SIM_COMPILE_OPTIONS})

include(CTest)

if(BUILD_TESTING)
  # Public API / integration tests
  file(GLOB SIM_TEST_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/tests/test_sim_*.c"
    )

  add_executable(sim_tests ${SIM_TEST_SOURCES})

  target_link_libraries(sim_tests PRIVATE simcore)

  target_compile_options(sim_tests PRIVATE ${SIM_COMPILE_OPTIONS})

  add_test(NAME sim_tests COMMAND sim_tests)


  # Event queue unit tests
  file(GLOB EVENT_QUEUE_TEST_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/tests/unit/test_event_queue.c"
    )

  add_executable(event_queue_tests
        ${EVENT_QUEUE_TEST_SOURCES}
        src/event_queue.c
    )

  target_include_directories(event_queue_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src/internal
            ${PROJECT_SOURCE_DIR}/tests
    )

  target_compile_options(event_queue_tests PRIVATE ${SIM_COMPILE_OPTIONS})

  add_test(NAME event_queue_tests COMMAND event_queue_tests)
endif()
