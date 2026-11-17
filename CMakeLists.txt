cmake_minimum_required(VERSION 3.16)
project(FrontMissionPC C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# Sanitizer flags for host engine code
set(FM_SANITIZER_FLAGS "")
if(ENABLE_ASAN)
    list(APPEND FM_SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()
if(ENABLE_UBSAN)
    list(APPEND FM_SANITIZER_FLAGS -fsanitize=undefined -fno-omit-frame-pointer)
endif()

# LakeSnes hardware emulation core
set(LAKESNES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/snesrecomp/ext/LakeSnes/snes)
add_library(lakesnes STATIC
    ${LAKESNES_DIR}/apu.c
    ${LAKESNES_DIR}/cart.c
    ${LAKESNES_DIR}/cpu.c
    ${LAKESNES_DIR}/dma.c
    ${LAKESNES_DIR}/dsp.c
    ${LAKESNES_DIR}/dsp1.c
    ${LAKESNES_DIR}/gsu.c
    ${LAKESNES_DIR}/input.c
    ${LAKESNES_DIR}/ppu.c
    ${LAKESNES_DIR}/snes.c
    ${LAKESNES_DIR}/snes_other.c
    ${LAKESNES_DIR}/spc.c
    ${LAKESNES_DIR}/statehandler.c
)
target_include_directories(lakesnes PUBLIC ${LAKESNES_DIR})
if(NOT MSVC)
    target_compile_options(lakesnes PRIVATE -O2 -w)
endif()

# SNES static recompilation runtime
set(SNESRECOMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/snesrecomp)
add_library(snesrecomp STATIC
    ${SNESRECOMP_DIR}/src/bus.c
    ${SNESRECOMP_DIR}/src/cpu.c
    ${SNESRECOMP_DIR}/src/func_table.c
    ${SNESRECOMP_DIR}/src/recomp_interp.c
)
target_include_directories(snesrecomp PUBLIC
    ${SNESRECOMP_DIR}/include
    ${LAKESNES_DIR}
)
target_link_libraries(snesrecomp PUBLIC lakesnes)
if(NOT MSVC)
    target_compile_options(snesrecomp PRIVATE -O2 -w)
endif()

# Host engine library (strict -Wall -Wextra -Werror)
add_library(fm_engine STATIC
    src/engine/boot_loader.c
    src/engine/config.c
    src/engine/gui_menu.c
    src/engine/replay.c
    src/engine/snes_ui_rasterizer.c
)
target_include_directories(fm_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
)
if(MSVC)
    target_compile_options(fm_engine PRIVATE /W4 /WX /O2)
else()
    target_compile_options(fm_engine PRIVATE -Wall -Wextra -Werror -O2 ${FM_SANITIZER_FLAGS})
    if(FM_SANITIZER_FLAGS)
        target_link_options(fm_engine PUBLIC ${FM_SANITIZER_FLAGS})
    endif()
endif()

# Headless Gate 0 boot & audio verifier
add_executable(fm_boot_verify
    src/boot_verify.c
)
target_link_libraries(fm_boot_verify PRIVATE fm_engine lakesnes m)
if(NOT MSVC)
    target_compile_options(fm_boot_verify PRIVATE -Wall -Wextra -Werror -O2 ${FM_SANITIZER_FLAGS})
endif()

# Headless Phase 1 trace recorder
add_executable(fm_record_traces
    tools/record_traces.c
)
target_link_libraries(fm_record_traces PRIVATE fm_engine lakesnes m)
if(NOT MSVC)
    target_compile_options(fm_record_traces PRIVATE -Wall -Wextra -Werror -O2 ${FM_SANITIZER_FLAGS})
endif()

# Headless Phase 1 CDL collector & manifest generator
add_executable(fm_cdl_collector
    tools/cdl_collector.c
)
target_include_directories(fm_cdl_collector PRIVATE ${LAKESNES_DIR})
target_link_libraries(fm_cdl_collector PRIVATE fm_engine lakesnes m)
if(NOT MSVC)
    target_compile_options(fm_cdl_collector PRIVATE -Wall -Wextra -Werror -O2 ${FM_SANITIZER_FLAGS})
endif()

# Optional SDL2 desktop frontend
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    add_executable(front_mission_pc
        src/main.c
    )
    target_include_directories(front_mission_pc PRIVATE ${SDL2_INCLUDE_DIRS} ${LAKESNES_DIR})
    if(WIN32)
        target_compile_definitions(front_mission_pc PRIVATE SDL_MAIN_HANDLED)
    endif()
    if(TARGET SDL2::SDL2)
        target_link_libraries(front_mission_pc PRIVATE fm_engine lakesnes SDL2::SDL2 m)
    else()
        target_link_libraries(front_mission_pc PRIVATE fm_engine lakesnes ${SDL2_LIBRARIES} m)
    endif()
    if(NOT MSVC)
        target_compile_options(front_mission_pc PRIVATE -Wall -Wextra -Werror -O2 ${FM_SANITIZER_FLAGS})
    endif()
endif()

# Unit tests
enable_testing()
add_executable(test_boot_loader
    tests/test_boot_loader.c
)
target_link_libraries(test_boot_loader PRIVATE fm_engine)
if(NOT MSVC)
    target_compile_options(test_boot_loader PRIVATE -Wall -Wextra -Werror -O2 ${FM_SANITIZER_FLAGS})
endif()
add_test(NAME test_boot_loader COMMAND test_boot_loader)
