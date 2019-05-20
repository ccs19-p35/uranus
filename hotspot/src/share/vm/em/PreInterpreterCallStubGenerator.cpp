//
// Created by max on 1/15/18.
//

#include "runtime/stubCodeGenerator.hpp"
#include "asm/assembler.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/java.hpp"
#include "PreInterpreterCallStubGenerator.h"
#include "CompilerEnclave.h"

#ifndef ENCLAVE_UNIX
#include <enclave/sc/securecompiler_u.h>
#endif

pre_interpreter_stub_t PreInterpreterCallStubGenerator::pre_interpreter_stub = NULL;
ocall_interpreter_stub_t PreInterpreterCallStubGenerator::ocall_interpreter_stub = NULL;

address PreInterpreterCallStubGenerator::register_buf_rbx = (address)(new char[8]);
address PreInterpreterCallStubGenerator::register_buf_rcx = (address)(new char[8]);
address PreInterpreterCallStubGenerator::register_buf_rdx = (address)(new char[8]);

PreInterpreterCallStubGenerator::PreInterpreterCallStubGenerator(CodeBuffer *c) : StubCodeGenerator(c) {}

address PreInterpreterCallStubGenerator::generate_interpreter_entry() {
#ifndef ENCLAVE_UNIX
    StubCodeMark mark(this, "PreInterpreter", "interpreter_entry");
    #   define __ _masm->
    address start = __ pc();
    // get the ret address, rax is not necessary to preserve
    // get enough space for ret address and ret value from the enclave
     __ movl(Address(r15, JavaThread::is_in_ecall_offset()), 1);
     __ pop(rax);
     __ push(0); // -> place for the ret value // rsp + 8
     __ push(rax); // rsp + 0
     __ movptr(c_rarg1, rsp);
     __ push(0); // rsp -8
     __ push(0); // rsp -16

//    __ movptr(r12, (intptr_t)register_buf_rdx);
//    __ movptr(Address(r12, 0), rax);

    __ push(rbx);
    __ push(rcx);
    __ push(rdx);
    __ push(rdi);
    __ push(rsi);
    __ push(r10);
    __ push(r11);
    __ push(r13);
    __ push(r14);
    __ push(r15);

    // put the args
    __ movptr(c_rarg0, rsp);
    __ mov(c_rarg2, rbx);

    address jjp = (address)CompilerEnclave::call_interpreter_zero_locals;
    __ movptr(rax, (intptr_t)jjp);
    __ call(rax);

    __ movl(Address(r15, JavaThread::is_in_ecall_offset()), 0);
    // return to the origin execution
    // __ movptr(rcx, (intptr_t)register_buf_rdx);
    // __ movptr(rax, (Address(rcx, 0)));
    __ movptr(r12, rax);
    __ movptr(rax, Address(r12, 8));
    __ movptr(r12, Address(r12, 0));

    // restore sender sp?
    __ mov(rsp, r13);
//    __ decrementq(rsp, 8 * 10);
//    __ pop(r12);
    __ jmp(r12);
//    __ ret(0);
#   undef __
    return start;
#else
    return NULL;
#endif
};

address PreInterpreterCallStubGenerator::generate_ocall_interpreter_entry() {
#ifndef ENCLAVE_UNIX
    StubCodeMark mark(this, "PreInterpreter", "interpreter_entry");
    #   define __ _masm->
    address ret = __ pc();
    __ movptr(rsp, rbp);
    __ pop(rbp);
    __ pop(r15);
    __ pop(r14);
    __ pop(r13);
    __ pop(r12);
    __ pop(rbx);
    __ ret(0);

    address start = __ pc();
    __ push(rbx);
    __ push(r12);
    __ push(r13);
    __ push(r14);
    __ push(r15);
    __ push(rbp);
    __ mov(rbp, rsp);
    __ mov(r15_thread, c_rarg3);
    __ movptr(r14, c_rarg0);
    __ movptr(rcx, c_rarg1);
    __ mov(rbx, c_rarg2);
    __ movptr(r13, (intptr_t)-1);

    Label fin, copy_par;
    __ cmpptr(rcx, 0);
    __ jcc(Assembler::zero, fin);
    __ movptr(rdx, 0);
    __ bind(copy_par);
    __ movptr(rax, Address(r14, rdx, Address::times_8));
    __ push(rax);
    __ incrementl(rdx);
    __ cmpl(rdx, rcx);
    __ jcc(Assembler::less, copy_par);
    __ bind(fin);


    // push ret address
    __ movptr(rax, (intptr_t)ret);
    __ push(rax);
    __ jmp(Address(rbx, Method::from_interpreted_offset()));
    return start;
#else
    return NULL;
#endif
};

void PreInterpreterCallStubGenerator::generate_pre_interpreter_entry() {
    ResourceMark rm;
    // Making this stub must be FIRST use of assembler

    BufferBlob* stub_blob = BufferBlob::create("get_cpu_info_stub", 128);
    if (stub_blob == NULL) {
        vm_exit_during_initialization("Unable to allocate get_cpu_info_stub");
    }
    CodeBuffer c(stub_blob);
    PreInterpreterCallStubGenerator g = PreInterpreterCallStubGenerator(&c);
    pre_interpreter_stub = CAST_TO_FN_PTR(pre_interpreter_stub_t ,
                                          g.generate_interpreter_entry());
}
