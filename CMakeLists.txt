cmake_minimum_required(VERSION 3.16)
project(DeterministicLockstep CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for strict warnings and warnings-as-errors
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
endif()

# Sanitizers (Address and Undefined Behavior) for Debug builds -- host builds only.
# Android APKs do not package the ASan runtime (libclang_rt.asan-*-android.so),
# so a sanitized libuhcraft.so fails System.loadLibrary with UnsatisfiedLinkError
# and the app crashes instantly on launch.
if(NOT ANDROID AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()

if(ANDROID)
    add_library(uhcraft SHARED 
        android/app/src/main/cpp/android_main.cpp
        src/LoopbackPeer.cpp
        src/NetworkSimulator.cpp
        src/UdpPeer.cpp
    )
    target_include_directories(uhcraft PRIVATE include)
    target_link_libraries(uhcraft log)
    # 16 KB page-size devices (recent Pixel/Galaxy flagships on Android 15+)
    # refuse to load 4 KB-aligned native libraries: dlopen fails and the app
    # dies on launch. Align LOAD segments to 16 KB; 16 KB-aligned libraries
    # still load fine on 4 KB devices.
    target_link_options(uhcraft PRIVATE "-Wl,-z,max-page-size=16384")
endif()

# We will add executable targets later as we build Phase 0.
add_executable(sim_demo src/main.cpp)
target_include_directories(sim_demo PRIVATE include)

add_executable(test_input_system src/test_input_system.cpp)
target_include_directories(test_input_system PRIVATE include)

add_executable(test_replay src/test_replay.cpp)
target_include_directories(test_replay PRIVATE include)

add_executable(test_fuzz_replay src/test_fuzz_replay.cpp)
target_include_directories(test_fuzz_replay PRIVATE include)

add_executable(test_loopback_peer tests/test_loopback_peer.cpp src/LoopbackPeer.cpp)
target_include_directories(test_loopback_peer PRIVATE include)

add_executable(test_lockstep_barrier tests/test_lockstep_barrier.cpp src/LoopbackPeer.cpp)
target_include_directories(test_lockstep_barrier PRIVATE include)

add_executable(test_desync_detection tests/test_desync_detection.cpp src/LoopbackPeer.cpp)
target_include_directories(test_desync_detection PRIVATE include)

add_executable(test_desync_dump tests/test_desync_dump.cpp)
target_include_directories(test_desync_dump PRIVATE include)

add_executable(test_local_lockstep tests/test_local_lockstep.cpp src/LoopbackPeer.cpp)
target_include_directories(test_local_lockstep PRIVATE include)

add_executable(test_udp_peer tests/test_udp_peer.cpp src/UdpPeer.cpp)
target_include_directories(test_udp_peer PRIVATE include)

add_executable(test_network_simulator tests/test_network_simulator.cpp src/NetworkSimulator.cpp src/LoopbackPeer.cpp)
target_include_directories(test_network_simulator PRIVATE include)

add_executable(test_adaptive_input_delay tests/test_adaptive_input_delay.cpp src/NetworkSimulator.cpp src/LoopbackPeer.cpp)
target_include_directories(test_adaptive_input_delay PRIVATE include)
