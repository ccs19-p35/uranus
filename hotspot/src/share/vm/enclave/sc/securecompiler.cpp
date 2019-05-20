
#include <precompiled.hpp>
#include <sgx_thread.h>
#include "EnclaveNative.h"
#include "EnclaveOcall.h"
#include "EnclaveException.h"
#include "EnclaveOcallRuntime.h"
#include "EnclaveMemory.h"

typedef void* (*interpreter_stub_t)(void*, void*);
typedef address (*get_exception_stub_t)();

static interpreter_stub_t interpreter_stub = NULL;
static get_exception_stub_t get_exception = NULL;

// frame layout of calling into enclave

// Arguments:
//
// rbx: Method*
//
// Stack layout immediately at entry
//

//    [expression stack      ] * <- sp
//    [monitors              ]   \
//     ...                        | monitor block size
//    [monitors              ]   /
//    [monitor block size    ]
//    [byte code index/pointr]                   = bcx()                bcx_offset
//    [pointer to locals     ]                   = locals()             locals_offset
//    [constant pool cache   ]                   = cache()              cache_offset
//    [methodData            ]                   = mdp()                mdx_offset
//    [Method*               ]                   = method()             method_offset
//    [last sp               ]                   = last_sp()            last_sp_offset
//    [old stack pointer     ]                     (sender_sp)          sender_sp_offset
//    [old frame pointer     ]   <- fp           = link()
//    [ return address     ] <--- rsp when calling into enclave
//    [ parameter n        ]
//      ...
//    [ parameter 1        ]
//    [ expression stack   ] (caller's java expression stack)


// Stack frame when an ecall happend
//
//    [ ret_addr ] -> enclave_interpreter_ret
//    [ 0 ]
//    [ 0 ]
//    [ expression stack   ] (ecall rsp) <- r14

// register used across encalve
// rbx [ Method* ]]
// r13 -> clear
// r14 -> old parameter top
// r15 -> [ JavaThread* ]
// r11 [ 0: normal call, 1 -> ecall ]

int in_enclave_thread_count = 0;
sgx_thread_mutex_t count_mutex = SGX_THREAD_MUTEX_INITIALIZER;

// TODO: frame info of ocall
class InterpreterCallStubGenerator: public StubCodeGenerator {
public:

    InterpreterCallStubGenerator(CodeBuffer *c) : StubCodeGenerator(c) {}

    address generate_interpreter_entry() {
        StubCodeMark mark(this, "VM_Version", "get_cpu_info_stub");
#   define __ _masm->
        address ret = __ pc();
        EnclaveMemory::enclave_interpreter_ret = ret;
        // regular exit path
        __ movptr(r11, 0);
        __ movptr(rsp, rbp);
        __ pop(rbp);
        __ ret(0);
        address exception_ret = __ pc();
        EnclaveMemory::enclave_interpreter_exception_ret = exception_ret;
        __ movptr(r11, 1);
        __ movptr(rsp, rbp);
        __ pop(rbp);
        __ ret(0);
        address start = __ pc();
        // restore the sp and bp first, then restore the rbx and r14
        // __ movptr(rbp, Address(c_rarg0, 0));
        __ push(rbp);
        __ mov(rbp, rsp);
        __ movptr(rax, (intptr_t)ret);
        __ movptr(r12, c_rarg1);
        __ movptr(Address(r12, 0), rsp);

        __ movptr(rsp, c_rarg0);

        __ pop(r15);
        __ pop(r14);
        __ pop(r13);
        __ pop(r11);
        __ pop(r10);
        __ pop(rsi);
        __ pop(rdi);
        __ pop(rdx);
        __ pop(rcx);
        __ pop(rbx);

        // clear r13, so the sender sp is 0, used for gc
        __ movptr(r13, rsp);

        // get the ret address, it may cause SEV if a pop/push is used
//      __ movptr(rax, Address(rsp, 0));
        // we have add three stack element, so we need to pack it
        // why 5 instead of 4 ???
        __ lea(r14, Address(rsp, 2, Address::times_8, wordSize));
        __ movptr(Address(rsp, 0), 0);
        __ movptr(Address(rsp, 4), 0);
        __ movptr(rsp, Address(r12, 0));

        // push the ret address
        __ push(rax);

        __ movptr(r11, Address(rbx, Method::kind_offset()));
        __ movptr(r12, (intptr_t)AbstractInterpreter::getEntryTable());
        __ movptr(r12, Address(r12, r11, Address::times_8));

        // tell that this is a ecall to the interpreter
        __ movl(r11, 1);
        __ jmp(r12);
        address get_exception_addr = __ pc();
        __ mov(rax, r11);
        __ ret(0);
        get_exception = CAST_TO_FN_PTR(get_exception_stub_t , get_exception_addr);

        address new_heap_object = __ pc();

        const Register RtopAddr = rdi;
        const Register RendAddr = rsi;
        const Register Rsize    = rdx;

        __ push(rbx);
        __ push(rbp);
        __ mov(rbp, rsp);
        __ movptr(rax, Address(RtopAddr, 0));

        // For retries rax gets set by cmpxchgq
        Label retry, slow_case, fin;
        __ bind(retry);
        __ lea(rbx, Address(rax, Rsize, Address::times_1));
        __ cmpptr(rbx, Address(RendAddr, 0));
        __ jcc(Assembler::above, slow_case);

        // Compare rax with the top addr, and if still equal, store the new
        // top addr in rbx at the address of the top addr pointer. Sets ZF if was
        // equal, and clears it otherwise. Use lock prefix for atomicity on MPs.
        //
        // rax: object begin
        // rbx: object end
        // rdx: instance size in bytes
        if (os::is_MP()) {
            __ lock();
        }
        __ cmpxchgptr(rbx, Address(RtopAddr, 0));

        // if someone beat us on the allocation, try again, otherwise continue
        __ jcc(Assembler::notEqual, retry);

        // should put r15 here first
        // __ incr_allocated_bytes(r15_thread, Rsize, 0);

        __ jmp(fin);
        __ bind(slow_case);
        __ movptr(rax, 0);
        __ bind(fin);
        __ mov(rsp, rbp);
        __ pop(rbp);
        __ pop(rbx);
        __ ret(0);
#   undef __
        EnclaveMemory::fast_heap_alloc = CAST_TO_FN_PTR(heap_allocator, new_heap_object);
        return start;
    };

    address generate_ocall_entry() {
#   define __ _masm->
        address entry = __ pc();
        // first we get the number of parameters
        const Address size_of_parameters(rdx,
                                         ConstMethod::size_of_parameters_offset());
        const Address constMethod(rbx, Method::const_offset());
        __ movptr(rdx, constMethod);
        __ load_unsigned_short(rcx, size_of_parameters);
        __ lea(r14, Address(rsp, rcx, Address::times_8, -wordSize));
        __ addptr(r14, 8);
        __ mov(c_rarg0, r14);
        __ mov(c_rarg1, rcx);
        __ push(rcx);
        __ movptr(r12, (intptr_t)EnclaveOcallRuntime::copy_parameter);
        __ call(r12);

        // ocall parameter: parameter_start, rbx, r15
        __ pop(rcx);
        __ mov(c_rarg0, rax);
        __ mov(c_rarg1, rcx);
        __ mov(c_rarg2, rbx);
        __ mov(c_rarg3, r15_thread);
        __ mov(c_rarg4, r13);
        __ movptr(r12, (intptr_t)EnclaveOcallRuntime::call_interpreter);
        __ call(r12);

        // get ret addr and jmp
        __ pop(r12);
        __ jmp(r12);
        return entry;
#   undef __
    }
};

static bool init = false;

void generate_interpreter_stub() {
    ResourceMark rm;
    // Making this stub must be FIRST use of assembler

//    // this code should be generate first to let stub return correctly
//    BufferBlob* post_blob = BufferBlob::create("post_stub", 128);
//    CodeBuffer cc(post_blob);
//    PostInterpreterCallStubGenerator gg = PostInterpreterCallStubGenerator(&cc);
//    post_ret = gg.generate_interpreter_entry();


    BufferBlob* stub_blob = BufferBlob::create("InterpreterCallStubGenerator:", 512);
    if (stub_blob == NULL) {
        vm_exit_during_initialization("Unable to allocate InterpreterCallStubGenerator:");
    }
    CodeBuffer c(stub_blob);
    InterpreterCallStubGenerator g = InterpreterCallStubGenerator(&c);
    interpreter_stub = CAST_TO_FN_PTR(interpreter_stub_t ,
                                      g.generate_interpreter_entry());

    EnclaveOcallRuntime::ocall_addr = g.generate_ocall_entry();

}

void pre_set_object_alignment() {
    // Object alignment.
    assert(is_power_of_2(ObjectAlignmentInBytes), "ObjectAlignmentInBytes must be power of 2");
    MinObjAlignmentInBytes     = ObjectAlignmentInBytes;
    assert(MinObjAlignmentInBytes >= HeapWordsPerLong * HeapWordSize, "ObjectAlignmentInBytes value is too small");
    MinObjAlignment            = MinObjAlignmentInBytes / HeapWordSize;
    assert(MinObjAlignmentInBytes == MinObjAlignment * HeapWordSize, "ObjectAlignmentInBytes value is incorrect");
    MinObjAlignmentInBytesMask = MinObjAlignmentInBytes - 1;

    LogMinObjAlignmentInBytes  = exact_log2(ObjectAlignmentInBytes);
    LogMinObjAlignment         = LogMinObjAlignmentInBytes - LogHeapWordSize;

    // Oop encoding heap max
    OopEncodingHeapMax = (uint64_t(max_juint) + 1) << LogMinObjAlignmentInBytes;

}

int enclave_signal_handler(sgx_exception_info_t *info) {
    switch (info->exception_vector) {
        case SGX_EXCEPTION_VECTOR_DE:
            info->cpu_context.rip = (u_int64_t)Interpreter::throw_ArithmeticException_entry();
            return -1;
        case SGX_EXCEPTION_VECTOR_SEV:
            if (info->cpu_context.r10 == 0xfe3f912) {
                info->cpu_context.rip = (u_int64_t)Interpreter::throw_StackOverflowError_entry();
            } else {
                info->cpu_context.rip = (u_int64_t) Interpreter::throw_NullPointerException_entry();
            }
            return -1;
        default:
            break;
    }
}

void* c1_initialize(void* cpuid, void** heap_top, void** heap_bottom, void** klass_list) {

    // Exception handler
    sgx_register_exception_handler(10, enclave_signal_handler);

  // set mode
    UseCompiler              = false;
    UseLoopCounter           = false;
    AlwaysCompileLoopMethods = false;
    UseOnStackReplacement    = false;
    UseCompressedClassPointers = false;
    UseCompressedOops = false;
    UseTLAB = false;
    pre_set_object_alignment();


    ClassLoaderData::init_null_class_loader_data();
    StringTable::create_table();

      char* memory_start = EnclaveMemory::init();
      EnclaveMemory::heap_top = heap_top;
      EnclaveMemory::heap_bottom = heap_bottom;
      os::set_memory_serialize_page(EnclaveMemory::new_page());
      SystemDictionary::_box_klasses = (Klass**)KLASS_get_type_klass();
      EnclaveMemory::wk_classes = (Klass**)klass_list;

    Abstract_VM_Version::initialize();
    Bytecodes::initialize();
    icache_init();
    VM_Version::_cpuid_info_global = (VM_Version::CpuidInfo*)cpuid;
    VM_Version_init();

    JavaClasses::compute_hard_coded_offsets();

    generate_interpreter_stub();
    Interpreter::initialize();
    init = true;
    EnclaveNative::init();
    EnclaveException::init();
    return memory_start;
}

void* interpreter_wrapper(void* rbx_buf) {
  char rsp_tmp[8];
  return (u_char*)interpreter_stub(rbx_buf, rsp_tmp);
}

void* interpreted_entry_zero_locals(void *rbx_buf, int* has_exception, int funtional) {
    // if (!init)
        // c1_initialize(NULL);
    init = true;
    sgx_thread_mutex_lock(&count_mutex);
    std::list<void*> *clean_pointer = NULL;
    in_enclave_thread_count += 1;
    if (in_enclave_thread_count < 2) {
//        printf("funtional\n");
        clean_pointer = EnclaveMemory::emalloc_clean();
        if (funtional)
            EnclaveGC::reset();
    }
    sgx_thread_mutex_unlock(&count_mutex);

    if (clean_pointer != NULL) {
        void* head;
        while (clean_pointer->size() > 0) {
            head = clean_pointer->front();
            free(head);
            clean_pointer->pop_front();
        }
        delete clean_pointer;
    }

    u_char* r = (u_char*)interpreter_wrapper(rbx_buf);
    if ((intptr_t)get_exception() == 1) {
        *has_exception = 1;
    }
    sgx_thread_mutex_lock(&count_mutex);
    in_enclave_thread_count -= 1;
    sgx_thread_mutex_unlock(&count_mutex);
    return (void*)r;
}

int within_enclave(void *addr) {
    return sgx_is_within_enclave(addr, 1);
}

void gc_scavenge(void *queue, int n) {
    EnclaveGC::gc_start((StarTask*)queue, n);
}

//void c1_compile_method(ciEnv* env,
//        ciMethod* target,
//        int entry_bci) {
//          c1compiler->compile_method(env, target, entry_bci);
//        }
