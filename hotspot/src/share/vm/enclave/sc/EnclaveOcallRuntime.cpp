//
// Created by max on 4/9/18.
//

#include "EnclaveOcallRuntime.h"
#include "EnclaveMemory.h"
#include "EnclaveOcall.h"

void* EnclaveOcallRuntime::ocall_addr = NULL;

void* EnclaveOcallRuntime::copy_parameter(void *r14, int size) {
    intptr_t* heap_rsp = (intptr_t*)EnclaveMemory::heapMemory->alloc(size, NULL);
    intptr_t* start = (intptr_t*)r14;
    // reverse the stack and copy it to the heap
    for (int i = 0;i < size;i++) {
        heap_rsp[i] = start[-i];
    }
    return heap_rsp;
}

void* EnclaveOcallRuntime::call_interpreter(void *r14, int size, void *method, void *thread, void* sender) {
    char* ret;
    ocall_interpreter(&ret, r14, size, method, thread, sender);
    return ret;
}

