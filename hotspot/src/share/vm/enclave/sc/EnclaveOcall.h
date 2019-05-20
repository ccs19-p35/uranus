
#ifndef ENCLAVE_HEADER_OCALL
#define ENCLAVE_HEADER_OCALL
#include "securecompiler_t.h"

// allocate memory outside enclave
void* JVM_ENTRY_omalloc(int size);

void JVM_ENTRY_resolve_invoke(void* thread,int bytecode);

void JVM_ENTRY_resolve_invoke_dynamic(void* thread);

void JVM_ENTRY_resolve_invoke_handle(void* thread);

void* JVM_ENTRY_ldc(void* thread, bool, void*, int);

void* JVM_ENTRY_resolve_ldc(void* thread, void*, int, int);

void KLASS_compute_oopmap(void*, void*, int);

void* KLASS_resolve_or_fail(const char* name);

void* KLASS_get_type_klass();

void* KLASS_get_type_array_klass();

void* KLASS_array_klass(void*, int, int);

void* KLASS_get_obj_array_klass(void* pool, int index);

void* KLASS_get_multi_array_klass(void* pool, int index);

void* JVM_ENTRY_resolve_klass(void* thread, void* pool, int index);

void* JVM_ENTRY_quick_cc(void* thread, void* pool, int index);

void JVM_ENTRY_resolve_get_put(void* thread, int bytecode);

void JVM_ENTRY_pre_native(void* thread, void* method, bool resolve);

#endif
