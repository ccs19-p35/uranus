cmake_minimum_required(VERSION 3.1)
project(hotspot)

set(CMAKE_CXX_STANDARD 98)

add_definitions(-DLINUX -D_GNU_SOURCE -DAMD64 -DPRODUCT -DHOTSPOT_RELEASE_VERSION="25.71-b00"
        -DHOTSPOT_BUILD_TARGET="product" -DHOTSPOT_BUILD_USER="jianyu" -DHOTSPOT_LIB_ARCH="amd64"
        -DHOTSPOT_VM_DISTRO="OpenJDK"  -DTARGET_OS_FAMILY_linux -DTARGET_ARCH_x86 -DTARGET_ARCH_MODEL_x86_64
        -DTARGET_OS_ARCH_linux_x86 -DTARGET_OS_ARCH_MODEL_linux_x86_64
        -DTARGET_COMPILER_gcc -D_REENTRANT -DVM_LITTLE_ENDIAN -D_LP64=1
        -DJRE_RELEASE_VERSION="1.8.0-internal-jianyu_2018_04_23_00_03-b00"
        -DCOMPILER2 -DCOMPILER1)

file(GLOB Panoply_SRC
        "share/PanoplyEnclave/cpp/*.cpp"
        "share/PanoplyEnclave/EnclaveUtil/*.cpp"
        "share/PanoplyEnclave/Thread/*.cpp"
        )

file(GLOB Assembly_SRC
        "${CMAKE_SOURCE_DIR}/src/os_cpu/linux_x86/vm/*.s")

file(GLOB SRC
        share/vm/adlc/*pp
        share/vm/asm/*pp
        share/vm/c1/*pp
        share/vm/ci/*pp
        share/vm/classfile/*pp
        share/vm/code/*pp
        share/vm/compiler/*pp
        share/vm/em/*pp
        share/vm/gc_implementation/*pp
        share/vm/gc_implementation/shared/*pp
        share/vm/gc_implementation/concurrentMarkSweep/*pp
        share/vm/gc_implementation/parallelScavenge/*pp
        share/vm/gc_interface/*pp
        share/vm/interpreter/*pp
        share/vm/memory/*pp
        share/vm/oops/*pp
        share/vm/opto/*pp
        share/vm/precompiled/precompiled.hpp
        share/vm/prims/*pp
        share/vm/runtime/*pp
        share/vm/services/*pp
        share/vm/shark/*pp
        share/vm/trace/*pp
        share/vm/utilities/*pp
        os/linux/vm/*pp
        os_cpu/linux_x86/vm/*pp
        cpu/x86/vm/*pp
        )

set_property(SOURCE ${CMAKE_SOURCE_DIR}/src/os_cpu/linux_x86/vm/linux_x86_64.s PROPERTY LANGUAGE C)

add_library(jvm SHARED ${SRC} ${Panoply_SRC} ${Assembly_SRC})

include_directories("share/vm/precompiled")
include_directories("cpu/x86/vm")
include_directories("os/linux/vm")
include_directories("os/posix/vm")
include_directories("os_cpu/linux_x86/vm")
include_directories("share/vm/")
include_directories("$ENV{SGX_SDK}/include")
include_directories("$ENV{SGX_SDK}/include/stlport")
include_directories("$ENV{SGX_SDK}/include/tlibc")
include_directories("${CMAKE_BINARY_DIR}")
include_directories("../../build/linux-x86_64-normal-server-release/hotspot/linux_amd64_compiler2/generated")
