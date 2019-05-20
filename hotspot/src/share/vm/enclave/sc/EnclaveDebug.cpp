//
// Created by max on 3/27/18.
//
#include <precompiled.hpp>
#include "EnclaveDebug.h"
#include "securecompiler.h"

void print_enclave_frame(void *t, void* addr, void* m) {
    JavaThread* THREAD = (JavaThread*)t;
    address sender_sp = (address)addr;
    Method* method = (Method*)m;
    if (!within_enclave(sender_sp)) {
        printf(D_INFO("Frame")" Init frame into enclave %s\n", method->name()->as_C_string());
    } else {
        Method* call_method = THREAD->last_frame().interpreter_frame_method();
        int bci = THREAD->last_frame().interpreter_frame_bci();
        const char* klass_caller = (call_method->klass_name()) ? call_method->klass_name()->as_C_string() : "";
        const char* klass_callee = (method->klass_name()) ? method->klass_name()->as_C_string() : "";
        // callee name cause sigsegv
        printf(D_INFO("FRAME")" %s:%s:%d -> %s:%s\n",
               klass_caller, call_method->name()->as_C_string(), bci,
               klass_callee, method->name()->as_C_string());
    }
}

void print_native_enclave_frame(void *t, void* addr, void* m) {
    JavaThread* THREAD = (JavaThread*)t;
    address sender_sp = (address)addr;
    Method* method = (Method*)m;
    if (!within_enclave(sender_sp)) {
        printf(D_INFO("Frame")" Init frame into enclave %s\n", method->name()->as_C_string());
    } else {
        Method* call_method = THREAD->last_frame().interpreter_frame_method();
        int bci = THREAD->last_frame().interpreter_frame_bci();
        const char* klass_caller = (call_method->klass_name()) ? call_method->klass_name()->as_C_string() : "";
        const char* klass_callee = (method->klass_name()) ? method->klass_name()->as_C_string() : "";
        // callee name cause sigsegv
        printf(D_INFO("Native FRAME")" %s:%s:%d -> %s:%s\n",
               klass_caller, call_method->name()->as_C_string(), bci,
               klass_callee, method->name()->as_C_string());
    }
}

void print_exception_frame(void* t, void* exp) {
    JavaThread* thread = (JavaThread*)t;
    oop exp_oop = (oop)exp;
    Method* method = thread->last_frame().interpreter_frame_method();
    int bci = thread->last_frame().interpreter_frame_bci();
    printf(D_INFO("EXCEPTION")" method %s:%s:%d\n   [EXP] handle exception %s:%s:%d\n",
           method->klass_name()->as_C_string(), method->name()->as_C_string(), bci,
           exp_oop->klass()->name()->as_C_string(),
           thread->exception_file(), thread->exception_line());
}