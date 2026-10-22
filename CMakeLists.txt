cmake_minimum_required(VERSION 3.31)

project(simcore LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SIM_COMPILE_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wstrict-prototypes
    -Wmissing-prototypes
)

option(ENABLE_WERROR "Treat compiler warnings as errors" OFF)
if(ENABLE_WERROR)
  add_compile_options(-Werror)
endif()

option(
  ENABLE_SANITIZERS
  "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
  OFF
)
if(ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

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
  # --- Integration Tests ---

  # Simulation lifecycle
  add_executable(intg_test_sim_lifecycle tests/test_sim_lifecycle.c)
  target_link_libraries(intg_test_sim_lifecycle PRIVATE simcore)
  target_compile_options(intg_test_sim_lifecycle PRIVATE ${SIM_COMPILE_OPTIONS})
  add_test(NAME intg_test_sim_lifecycle COMMAND intg_test_sim_lifecycle)

  # Simulation schedule
  add_executable(intg_test_sim_schedule tests/test_sim_schedule.c)
  target_link_libraries(intg_test_sim_schedule PRIVATE simcore)
  target_compile_options(intg_test_sim_schedule PRIVATE ${SIM_COMPILE_OPTIONS})
  add_test(NAME intg_test_sim_schedule COMMAND intg_test_sim_schedule)

  # Simulation run
  add_executable(intg_test_sim_run tests/test_sim_run.c)
  target_link_libraries(intg_test_sim_run PRIVATE simcore)
  target_compile_options(intg_test_sim_run PRIVATE ${SIM_COMPILE_OPTIONS})
  add_test(NAME intg_test_sim_run COMMAND intg_test_sim_run)

  # --- Unit Tests ---

  # Event queue
  add_executable(unit_test_event_queue
      tests/unit/test_event_queue.c
      src/event_queue.c
  )
  target_include_directories(unit_test_event_queue
      PRIVATE
          ${PROJECT_SOURCE_DIR}/include
          ${PROJECT_SOURCE_DIR}/src/internal
          ${PROJECT_SOURCE_DIR}/tests
  )
  target_compile_options(unit_test_event_queue PRIVATE ${SIM_COMPILE_OPTIONS})
  add_test(NAME unit_test_event_queue COMMAND unit_test_event_queue)
endif()
