//
// Created by max on 4/9/18.
//

#ifndef HOTSPOT_ENCLAVEOCALLRUNTIME_H
#define HOTSPOT_ENCLAVEOCALLRUNTIME_H


class EnclaveOcallRuntime {
public:
    static void* ocall_addr;
    static void* copy_parameter(void* r14, int size);
    static void* call_interpreter(void* r14, int size, void* method, void* thread, void* sender);
};


#endif //HOTSPOT_ENCLAVEOCALLRUNTIME_H
