//
// Created by max on 3/21/18.
//

#ifndef HOTSPOT_ENCLAVEDEBUG_H
#define HOTSPOT_ENCLAVEDEBUG_H

#ifdef ENCLAVE_UNIX
// print every frame information
// #define DB_FRAME
//#define DB_GC
//#define DB_MEM
#define DB_EXP
//#define DB_WARN

void print_enclave_frame(void* , void* sender_sp, void* method);

void print_native_enclave_frame(void *t, void* addr, void* m);

void print_exception_frame(void* thread, void* exp_oop);

#endif

#ifdef DB_FRAME
#define FRAME_DEBUG(...) printf(__VA_ARGS__)
#else
#define FRAME_DEBUG(...)
#endif

#define COLOR_RED       "[0;31m"
#define COLOR_GREEN     "[0;32m"
#define COLOR_YELLOW    "[0;33m"
#define COLOR_BLUE      "[0;34m"


#define P_COLOR(color, text) "\033"color text"\033[0m"

#define D_INFO(text) P_COLOR(COLOR_GREEN, "["text"]")
#define D_WARN(text) P_COLOR(COLOR_YELLOW, "["text"]")
#define D_ERROR(text) P_COLOR(COLOR_RED, "["text"]")
#define D_NORMAL(text) P_COLOR(COLOR_BLUE, "["text"]")

#ifdef ENCLAVE_UNIX
#ifdef DB_WARN
#define D_WARN_Unstable printf(D_WARN("Unstable")" using unstable function call: %s\n", __FUNCTION__)
#define D_WARN_Unimplement printf(D_WARN("Un-implemented")" using un-implemented function call: %s\n", __FUNCTION__)
#else
#define D_WARN_Unstable
#define D_WARN_Unimplement
#endif
#else
#define D_WARN_Unstable
#define D_WARN_Unimplement
#endif


#endif //HOTSPOT_ENCLAVEDEBUG_H
