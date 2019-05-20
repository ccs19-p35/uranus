cmake_minimum_required(VERSION 3.1)
project(hotspot)

set(CMAKE_CXX_STANDARD 98)

if(NOT DEFINED ENV{SGX_SDK})
    set(ENV{SGX_SDK} /opt/intel/sgxsdk/)
endif()

if(NOT DEFINED ENV{SGX_SDK})
    message(FATAL_ERROR "$SGX_SDK environment variable must be set.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH} $ENV{SGX_SDK}/lib")
    set(SGX_ENCLAVE_SIGNER "$ENV{SGX_SDK}/bin/x86/sgx_sign")
    set(SGX_EDGER8R "$ENV{SGX_SDK}/bin/x86/sgx_edger8r")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "$ENV{SGX_SDK}/lib64")
    set(SGX_ENCLAVE_SIGNER "$ENV{SGX_SDK}/bin/x64/sgx_sign")
    set(SGX_EDGER8R "$ENV{SGX_SDK}/bin/x64/sgx_edger8r")
endif()

find_library(TRTS_LIB sgx_trts)
find_library(TRTS_SIM_LIB sgx_trts_sim)
find_library(TSTDC_LIB sgx_tstdc)
find_library(TSTDCXX_LIB sgx_tcxx)
find_library(TKEY_EXCHANGE_LIB sgx_tkey_exchange)
find_library(TCRYPTO_LIB sgx_tcrypto)
find_library(SERVICE_LIB sgx_tservice)
find_library(SERVICE_SIM_LIB sgx_tservice_sim)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENCLAVE -O0 -g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENCLAVE -O2")
set(CMAKE_CXX_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_PROFILE} -DENCLAVE -O2 -DPERF")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc -fvisibility=hidden -fpie -fstack-protector")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -nostdinc++")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -fPIC -fno-rtti -fno-exceptions -fcheck-new -fvisibility=hidden -m64 -pipe -fno-strict-aliasing  -fno-omit-frame-pointer -std=gnu++98 -Wno-write-strings -fpermissive")
set(ENCLAVE_LINK_FLAGS "-Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined -Wl,-pie,-eenclave_entry -Wl,--export-dynamic -Wl,--defsym,__ImageBase=0")

file(GLOB Panoply_SRC
        "enclave_src/share/PanoplyEnclave/cpp/*.cpp"
        "enclave_src/share/PanoplyEnclave/EnclaveUtil/*.cpp"
        "enclave_src/share/PanoplyEnclave/Thread/*.cpp"
        )

file(GLOB Assembly_SRC
        "${CMAKE_SOURCE_DIR}/enclave_src/os_cpu/linux_x86/vm/*.s")

file(GLOB enclave_SRC
        enclave_src/share/vm/asm/*pp
        enclave_src/share/vm/udis/*.c
        enclave_src/share/vm/c0/*pp
        enclave_src/share/vm/classfile/*pp
        enclave_src/share/vm/code/*pp
        enclave_src/share/vm/enclave/sc/*pp
        enclave_src/share/vm/interpreter/*pp
        enclave_src/share/vm/memory/*pp
        enclave_src/share/vm/oops/*pp
        enclave_src/share/vm/precompiled/precompiled.hpp
        enclave_src/share/vm/prims/*pp
        enclave_src/share/vm/runtime/*pp
        enclave_src/share/vm/utilities/*pp
        enclave_src/os/linux/vm/*pp
        enclave_src/os_cpu/linux_x86/vm/*pp
        enclave_src/cpu/x86/vm/*pp
        enclave_src/share/vm/enclave/sc/native_libs/*c
        enclave_src/share/vm/enclave/sc/native_libs/fdlibm/*h
        enclave_src/share/vm/enclave/sc/native_libs/fdlibm/*c
        enclave_src/share/vm/libmpx/*h
        enclave_src/share/vm/libmpx/*c
        )

set_property(SOURCE ${CMAKE_SOURCE_DIR}/enclave_src/os_cpu/linux_x86/vm/linux_x86_64.s PROPERTY LANGUAGE C)

add_library(jvm_enclave SHARED ${CMAKE_BINARY_DIR}/securecompiler_t.c ${enclave_SRC} enclave_src/share/vm/enclave/sc/native_libs/java_lang_StrictMath.h ${Panoply_SRC} ${Assembly_SRC})

set_target_properties(jvm_enclave PROPERTIES LINK_FLAGS ${ENCLAVE_LINK_FLAGS})
if(NOT DEFINED ENV{SGX_HW})
    set(Trts_Library_Path "${TRTS_SIM_LIB}")
    set(Service_Library_Path "${SERVICE_SIM_LIB}")
else()
    set(Trts_Library_Path "${TRTS_LIB}")
    set(Service_Library_Path "${SERVICE_LIB}")
endif()

target_link_libraries(jvm_enclave -Wl,--whole-archive "${Trts_Library_Path}" -Wl,--no-whole-archive -Wl,--start-group "${TSTDC_LIB}" "${TSTDCXX_LIB}" -Wl,--hash-style=both
        "${TKEY_EXCHANGE_LIB}" "${TCRYPTO_LIB}" "${Service_Library_Path}" -Wl,--end-group)

add_definitions(-DLINUX -D_GNU_SOURCE -DAMD64 -DPRODUCT -DHOTSPOT_RELEASE_VERSION="25.71-b00"
        -DHOTSPOT_BUILD_TARGET="product" -DHOTSPOT_BUILD_USER="jianyu" -DHOTSPOT_LIB_ARCH="amd64"
        -DHOTSPOT_VM_DISTRO="OpenJDK"  -DTARGET_OS_FAMILY_linux -DTARGET_ARCH_x86 -DTARGET_ARCH_MODEL_x86_64
        -DTARGET_OS_ARCH_linux_x86 -DTARGET_OS_ARCH_MODEL_linux_x86_64
        -DTARGET_COMPILER_gcc -D_REENTRANT -DVM_LITTLE_ENDIAN -D_LP64=1
        -DJRE_RELEASE_VERSION="1.8.0-internal-jianyu_2018_04_23_00_03-b00"
        -DENCLAVE_UNIX)

if (DEFINED ENV{BOUND_MODE})
    if ($ENV{BOUND_MODE} MATCHES "mpx")
        add_definitions("-DENCLAVE_BOUND_CHECK -DENCLAVE_MPX")
        message("bound check mode mpx enable")
    elseif($ENV{BOUND_MODE} MATCHES "soft")
        message("bound check mode soft enable")
        add_definitions("-DENCLAVE_BOUND_CHECK")
    else()
        message("bound check mode $ENV{BOUND_MODE} not valid")
    endif()
endif()

if(ENCLAVE)
    add_definitions(-DENCLAVE_UNIX)
endif()

include_directories("enclave_src/share/vm/precompiled")
include_directories("enclave_src/cpu/x86/vm")
include_directories("enclave_src/os/linux/vm")
include_directories("enclave_src/os/posix/vm")
include_directories("enclave_src/os_cpu/linux_x86/vm")
include_directories("enclave_src/share/vm/")
include_directories("enclave_src/share/PanoplyEnclave/include")
include_directories("$ENV{SGX_SDK}/include")
include_directories("$ENV{SGX_SDK}/include/stlport")
include_directories("$ENV{SGX_SDK}/include/tlibc")
include_directories("enclave_src/share/PanoplyEnclave/syscall_include/")
include_directories("enclave_src/share/PanoplyEnclave/")
include_directories("${CMAKE_BINARY_DIR}")

add_custom_command(
        COMMAND ${SGX_EDGER8R} --trusted ${CMAKE_SOURCE_DIR}/make/linux/makefiles/enclave/securecompiler.edl --search-path ${CMAKE_SOURCE_DIR}/enclave_src/share/PanoplyEnclave/ --search-path ${CMAKE_SOURCE_DIR}/enclave_src/share/vm/ --search-path "$ENV{SGX_SDK}/include"
        DEPENDS ${CMAKE_SOURCE_DIR}/make/linux/makefiles/enclave/securecompiler.edl
        OUTPUT ${CMAKE_BINARY_DIR}/securecompiler_t.h ${CMAKE_BINARY_DIR}/securecompiler_t.c)

add_custom_command(
        COMMAND ${SGX_ENCLAVE_SIGNER} sign -key "$ENV{SGX_SDK}/SampleCode/SampleEnclave/Enclave/Enclave_private.pem" -enclave $<TARGET_FILE:jvm_enclave> -out libjvm-sgx.so -config ${CMAKE_SOURCE_DIR}/enclave_src/share/vm/enclave/sc/securecompiler.config.xml
        DEPENDS jvm_enclave ${CMAKE_SOURCE_DIR}/enclave_src/share/vm/enclave/sc/securecompiler.config.xml
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libjvmsgx.so)

add_custom_target(enclave_trusted_signed_target ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libjvmsgx.so)
