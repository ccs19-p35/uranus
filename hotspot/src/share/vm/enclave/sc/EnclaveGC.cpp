//
// Created by max on 3/7/18.
//

#include <precompiled.hpp>
#include <interpreter/oopMapCache.hpp>
#include "EnclaveGC.h"
#include "securecompiler.h"
#include "EnclaveOcall.h"
#include "EnclaveMemory.h"

void* EnclaveGC::_eden = NULL;
void* EnclaveGC::_eden_start = NULL;
void* EnclaveGC::_eden_end = NULL;
void* EnclaveGC::_survivor = NULL;
void* EnclaveGC::_survivor_start = NULL;
bool EnclaveGC::_young_gen_is_full = false;
int EnclaveGC::current_size = ENCLAVE_EDEN_SIZE;
int EnclaveGC::memory_ratio = 1;
std::queue<StarTask>* EnclaveGC::_taskQueue = NULL;
std::queue<oop>* EnclaveGC::_clearQueue = NULL;

void EnclaveGC::init() {
    _eden = malloc(ENCLAVE_EDEN_SIZE);
    _eden_start = _eden;
    _eden_end = (address)_eden + ENCLAVE_EDEN_SIZE;
    _survivor = malloc(ENCLAVE_EDEN_SIZE);
    _survivor_start = _survivor;
}

void EnclaveGC::reset() {
    _eden_end = _eden + current_size;
    _eden_start = _eden;
    EnclaveMemory::heap_buffer_start = EnclaveMemory::heap_buffer;
}

void* EnclaveGC::allocate_eden(int size) {
    int new_size = size * sizeof(HeapWord);
    if ((address)_eden_start + new_size > (address)_eden + current_size) {
        return NULL;
    }

    while (true) {
        if ((address)_eden_start + new_size < (address)_eden + current_size) {
            address obj = (address) _eden_start;
            address new_top = obj + new_size;
            address result = (address) Atomic::cmpxchg_ptr(new_top, &_eden_start, obj);
            if (result != obj) {
                continue;
            }
            return result;
        } else {
            return NULL;
        }
    }
}

void* EnclaveGC::allocate_survivor(int size) {
    int new_size = size * sizeof(HeapWord);
    if ((address)_survivor_start + new_size > (address)_survivor + current_size) {
        _young_gen_is_full = true;
        printf("allocating %d fail\n", new_size);
        return NULL;
    }
    void* r = _survivor_start;
    _survivor_start = (address)_survivor_start + new_size;
    return r;
}

oop EnclaveGC::copy_to_allocated_space(oop o) {

    oop new_obj = NULL;

    // NOTE! We must be very careful with any methods that access the mark
    // in o. There may be multiple threads racing on it, and it may be forwarded
    // at any time. Do not use oop methods for accessing the mark!
    markOop test_mark = o->mark();

    // The same test as "o->is_forwarded()"
    if (!test_mark->is_marked()) {
        bool new_obj_is_tenured = false;
        size_t new_obj_size = o->size();

        // Try allocating obj in to-space (unless too old)
        new_obj = (oop) allocate_survivor(new_obj_size);

        guarantee(new_obj != NULL, D_ERROR("Mem")" allocation should have succeeded");

        // Copy obj
        Copy::aligned_disjoint_words((HeapWord*)o, (HeapWord*)new_obj, new_obj_size);

        // Now we have to CAS in the header.
        if (o->cas_forward_to(new_obj, test_mark)) {
            // We won any races, we "own" this object.
            guarantee(new_obj == o->forwardee(), "Something wrong");

            // Do the size comparison first with new_obj_size, which we
            // already have. Hopefully, only a few objects are larger than
            // _min_array_size_for_chunking, and most of them will be arrays.
            // So, the is->objArray() test would be very infrequent.
            int _min_array_size_for_chunking = 1000000;
            if (new_obj_size > _min_array_size_for_chunking &&
                new_obj->is_objArray() &&
                PSChunkLargeArrays) {
                GC_DEBUG("    [GC Task] obj oop\n");
//                    // we'll chunk it
//                    oop* const masked_o = mask_chunked_array_oop(o);
//                    push_depth(masked_o);
//                    TASKQUEUE_STATS_ONLY(++_arrays_chunked; ++_masked_pushes);
            } else {
                // we'll just push its contents
                // note: jianyu, we use the same api to reduce code modifications
//                    new_obj->push_contents(NULL);
//              // do nothing if it is a type array
                Klass* k = new_obj->klass();
                if (!k->oop_is_typeArray()) {
                    // It might contain oops beyond the header, so take the virtual call.
                    if (k->oop_is_objArray()) {
                        ObjArrayKlass::oop_push_contents((ObjArrayKlass*)k, new_obj);
                    } else {
                        InstanceKlass::oop_push_contents((InstanceKlass*)k, new_obj);
                    }
                }
            }
        }  else {
            // We lost, someone else "owns" this object
            ShouldNotReachHere();
        }
    } else {
        ShouldNotReachHere();
        new_obj = o->forwardee();
    }

    return new_obj;
}

bool EnclaveGC::is_obj_in_young(address p) {
    bool in = p >= (address)_eden && p < ((address)_eden + current_size);
/*
    if (!in && p != NULL) {
        printf(D_INFO("MEM")" address %lx:%s is not in eden\n", (intptr_t)p, ((oop)p)->klass()->name()->as_C_string());
    }
*/
    return in;
}

bool EnclaveGC::is_obj_in_new(address p){
    bool in = p >= (address)_survivor && p < ((address)_survivor + current_size);
    return in;
}

void EnclaveGC::copy_and_push_safe_barrier(oop* p) {
    oop o = oopDesc::load_decode_heap_oop_not_null(p);
    oop new_obj = NULL;
    if (o->is_forwarded()) {
        new_obj = o->forwardee();
        GC_DEBUG(D_INFO("GC TASK")" Forwarded %lx -> %lx\n", (intptr_t)p, (intptr_t)new_obj);
    } else {
        new_obj = copy_to_allocated_space(o);
        GC_DEBUG(D_INFO("GC TASK")" Copied %lx -> %lx\n", (intptr_t)p, (intptr_t)new_obj);
    }
    oopDesc::encode_store_heap_oop_not_null(p, new_obj);
}

void EnclaveGC::do_gc(std::queue<StarTask> *task_list) {
    timeval tvs, tve;
    gettimeofday(&tvs, 0);
    bool expand = false;
    if (memory_ratio > 1) {
//        current_size *= 2;
        _survivor = malloc(current_size * memory_ratio);
        if (_survivor != NULL) {
            expand = true;
        } else {
            _survivor = malloc(current_size);
        }
        guarantee(_survivor != NULL, "GC" "Allocation should succeed");
        _survivor_start = _survivor;
    } else {
//        _survivor = malloc(current_size);
//        _survivor_start = _survivor;
        guarantee(_survivor != NULL, "GC" "Allocation should succeed");
    }
    oop* task;
    while (task_list->size() > 0) {
        task = task_list->front();
//        GC_DEBUG("    [GC Task] %lx, Class: %s\n", (intptr_t)*task, (*task)->klass()->name()->as_C_string());
        task_list->pop();
//        printf("doing task %lx -> %lx\n", (intptr_t)task, (intptr_t)*task);
//        printf("doing task %lx -> %lx %s\n", (intptr_t)task, (intptr_t)*task, (*task)->klass()->name()->as_C_string());
        if (is_oop_masked(task)) {
            // is array_oop
            GC_DEBUG(D_INFO("GC TASK")" %lx is masked\n", (intptr_t)*task);
        } else if (!is_obj_in_young((address)(*task))) {
            // TODO: prevent putfield in non-enclave object
            // we do not search deep into non-enclave obj, as we prevent
            // writing enclave object into it
//            if (!is_obj_in_new((address)(*task)) && (address)(*task) != NULL) {
//                // do nothing
//                GC_DEBUG(D_INFO("GC TASK")" %lx -> %lx is not in enclave\n", (intptr_t)task, (intptr_t)(*task));
//                oop obj_out = *task;
//                markOop testMark = obj_out->mark();
//                if (testMark->is_mark_enclave_gc()) {
//                    continue;
//                }
//                Klass* k = obj_out->klass();
//                if (!k->oop_is_typeArray()) {
//                    // It might contain oops beyond the header, so take the virtual call.
//                    if (k->oop_is_objArray()) {
//                        obj_out->set_mark(testMark->set_sgx_gc_mark());
//                        PUSH_CLEAR(obj_out);
//                        ObjArrayKlass::oop_push_contents((ObjArrayKlass*)k, obj_out);
//                    } else {
//                        obj_out->set_mark(testMark->set_sgx_gc_mark());
//                        PUSH_CLEAR(obj_out);
//                        InstanceKlass::oop_push_contents((InstanceKlass*)k, obj_out);
//                    }
//                }
//            } else {
//                // recreated tasks
//            }
        } else {
            copy_and_push_safe_barrier(task);
        }
    }

/*    oop clear_task;
    markOop mark;
    while (_clearQueue->size() > 0) {
        clear_task = _clearQueue->front();
        _clearQueue->pop();
        mark = clear_task->mark();
        clear_task->set_mark(mark->clear_sgx_gc_mark());
    }*/
    //reinit
    // if there is no enough memory, then expand the eden
//    free(_eden);
    void* tmp = _eden;
    _eden = _survivor;
    _eden_start = _survivor_start;
    _eden_end = (address)_eden + current_size;
    _survivor = _survivor_start = tmp;
    if (expand) {
        current_size *= memory_ratio;
        _eden_end = (address)_eden + current_size;
        _survivor = _survivor_start = NULL;
        printf(D_INFO("Heap Expanded")" heap is expanded to %d\n", current_size);
    }
    gettimeofday(&tve, 0);
    printf("do gc %ld\n", (tve.tv_sec - tvs.tv_sec) * 1000000 + (tve.tv_usec - tvs.tv_usec));
}

void EnclaveGC::gc_start(StarTask *task_out, int n) {
    // at first we create a task list of copying the task
    GC_DEBUG(D_INFO("GC")" gc started out of enclave\n");
    if (!_taskQueue)
        _taskQueue = new std::queue<StarTask>();
    if (!_clearQueue)
        _clearQueue = new std::queue<oop>();
    for (int i = 0;i < n;i++) {
        push_task(task_out[i]);
    }
    do_gc(_taskQueue);
    _taskQueue->empty();
    _clearQueue->empty();
}

// as we are only using interpreter, there are two kinds of situations of a frame
// 1. current frame is interpreting -> add expression stacks
// 2. current frame is calling a method -> add arguments and (expression stacks?)
// 3. current frame is in native call (not possible, but if in a multi-threading enviroment?)
// see frame:990
void EnclaveGC::frame_argument(frame* f, Symbol *signature, bool has_receiver) {
    int offset = ArgumentSizeComputer(signature).size() + (has_receiver ? 1 : 0);
    GC_DEBUG(D_INFO("GC Argument")" %s: %d\n", signature->as_C_string(),offset);
    // add recv to list
    if (has_receiver) {
        --offset;
        oop* addr = (oop*)f->interpreter_frame_tos_at(offset);
        PUSH_TASK(addr);
    }
    int _index = 1;
    while (signature->byte_at(_index) != ')') {
        switch(signature->byte_at(_index)) {
            case 'B': _index++; offset -= T_BYTE_size   ; break;
            case 'C': _index++; offset -= T_CHAR_size   ; break;
            case 'D': _index++; offset -= T_DOUBLE_size ; break;
            case 'F': _index++; offset -= T_FLOAT_size  ; break;
            case 'I': _index++; offset -= T_INT_size    ; break;
            case 'J': _index++; offset -= T_LONG_size   ; break;
            case 'S': _index++; offset -= T_SHORT_size  ; break;
            case 'Z': _index++; offset -= T_BOOLEAN_size; break;
            case 'V': _index++; offset -= T_VOID_size;  ; break;
            case 'L': _index++; offset -= T_OBJECT_size;
                {
                    while (signature->byte_at(_index++) != ';');
                    oop *addr = (oop *) f->interpreter_frame_tos_at(offset);
                    PUSH_TASK(addr);
                }
                break;
            case '[': _index++; offset -= T_ARRAY_size;
                {
                    while (signature->byte_at(_index) == '[') {
                        _index++;
                    }
                    if (signature->byte_at(_index) == 'L') {
                        while (signature->byte_at(_index++) != ';');
                    } else {
                        _index++;
                    }
                    oop *addr = (oop *) f->interpreter_frame_tos_at(offset);
                    PUSH_TASK(addr);
                }
                break;
            default:
                ShouldNotReachHere();
            break;
        }
    }
}

void EnclaveGC::frame_do_gc(frame* f, RegisterMap* map, TRAPS) {
    bool query_oop_map_cache;
    methodHandle m (THREAD, f->interpreter_frame_method());
    jint      bci = f->interpreter_frame_bci();

    GC_DEBUG("  [Method] %s:%s %d\n", m->klass_name()->as_C_string(), m->name()->as_C_string(), bci);
    // do nothing if native
//    if (m->is_native() PPC32_ONLY(&& m->is_static())) {
//        f->do_oop(interpreter_frame_temp_oop_addr());
//    }

    int max_locals = m->is_native() ? m->size_of_parameters() : m->max_locals();

    Symbol* signature = NULL;
    bool has_receiver = false;

    // Process a callee's arguments if we are at a call site
    // (i.e., if we are at an invoke bytecode)
    // This is used sometimes for calling into the VM, not for another
    // interpreted or compiled frame.
    if (!m->is_native()) {
        Bytecode_invoke call = Bytecode_invoke_check(m, bci);
        if (call.is_valid()) {
            signature = call.signature();
            has_receiver = call.has_receiver();
            if (map->include_argument_oops() &&
                f->interpreter_frame_expression_stack_size() > 0) {
                // we are at a call site & the expression stack is not empty
                // => process callee's arguments
                //
                // Note: The expression stack can be empty if an exception
                //       occurred during method resolution/execution. In all
                //       cases we empty the expression stack completely be-
                //       fore handling the exception (the exception handling
                //       code in the interpreter calls a blocking runtime
                //       routine which can cause this code to be executed).
                //       (was bug gri 7/27/98)
                frame_argument(f, signature, has_receiver);
            }
        }
    }

    // process locals & expression stack
    frame_expressions(f, m, bci, max_locals);
}

void EnclaveGC::frame_expressions(frame* f, methodHandle m, int bci, int max_local) {
    InterpreterOopMap oopMap;
    compute_oopmap(m(), bci, &oopMap);
    m->mask_for(bci, &oopMap);
    int num_entries = oopMap._mask_size / InterpreterOopMap::bits_per_entry;
    int word_index = 0;
    uintptr_t value = 0;
    uintptr_t mask = 0;
    // iterate over entries
    for (int offset = 0; offset < num_entries; offset++, mask <<= InterpreterOopMap::bits_per_entry) {
        // get current word
        if (mask == 0) {
            value = oopMap.bit_mask()[word_index++];
            mask = 1;
        }
        // test for oop
        if ((value & (mask << InterpreterOopMap::oop_bit_number)) != 0) {
            if (offset < max_local) {
                oop* addr = (oop*)f->interpreter_frame_local_at(offset);
                PUSH_TASK(addr);
            } else {
                bool in_stack;
                oop* addr = (oop*)f->interpreter_frame_expression_stack_at(offset - max_local);
                // if it is not in stack, then we should not add it
                if (frame::interpreter_frame_expression_stack_direction() > 0) {
                    // TODO
                    // quick fix: we use sp instead of tos addr, as tos is not set?
                    in_stack = (intptr_t*)addr <= f->interpreter_frame_last_sp();
                } else {
                    in_stack = (intptr_t*)addr >= f->interpreter_frame_last_sp();
                }
                if (in_stack) {
                    PUSH_TASK(addr);
                } else {
//                    printf(D_INFO("GC Expression") "OBJ %lx -> %lx\n is not in stack", (intptr_t)addr, (intptr_t)*addr);
                }
            }
        }
    }
}

void EnclaveGC::enclave_gc(JavaThread* thread) {
    GC_DEBUG("[GC] start gc now %lx\n\n", (intptr_t)thread);
    if (!_taskQueue)
        _taskQueue = new std::queue<StarTask>();
    if (!_clearQueue)
        _clearQueue = new std::queue<oop>();
    // push stack oop in the gc task
    // TODO: other gc object
    RegisterMap map = RegisterMap(thread);
    frame f = thread->last_frame();
    while (true) {
        // if it is zero, then we reach the top of ecall
        if (!within_enclave(f.unextended_sp())) {
            GC_DEBUG("[Frame] init frame\n");
            break;
        }
        GC_DEBUG("[Frame]: sp:%lx, sender: %lx\n",
               (intptr_t)f.sp(),
               (intptr_t)f.unextended_sp());
        frame_do_gc(&f, &map, thread);

        // we should use this method directly instead of sender()
        f = f.sender_for_interpreter_frame(&map);
    }
    do_gc(_taskQueue);
    _taskQueue->empty();
    _clearQueue->empty();
    GC_DEBUG("[GC] gc finish\n");
    BREAKPOINT;
}

inline unsigned int hash_value_for_oopMap(methodHandle method, int bci) {
    // We use method->code_size() rather than method->identity_hash() below since
    // the mark may not be present if a pointer to the method is already reversed.
    return   ((unsigned int) bci)
             ^ ((unsigned int) method->max_locals()         << 2)
             ^ ((unsigned int) method->code_size()          << 4)
             ^ ((unsigned int) method->size_of_parameters() << 6);
}

void EnclaveGC::compute_oopmap(Method* method, int bci, InterpreterOopMap *map) {
    InstanceKlass *klass = method->constants()->pool_holder();
    if (klass->_oop_map_cache == NULL) {

    } else {
        OopMapCache *cache = klass->_oop_map_cache;
        OopMapCacheEntry* entry = NULL;
        int probe = hash_value_for_oopMap(method, bci);
        for (int i = 0;i < OopMapCache::_probe_depth;i++) {
            entry = cache->entry_at(probe + i);
            if (entry->match(methodHandle(method), bci)) {
                // fill in the map
             map->resource_copy(entry);
             return;
            }
        }
    }
    KLASS_compute_oopmap(klass, method, bci);

    OopMapCache *cache = klass->_oop_map_cache;
    OopMapCacheEntry* entry = NULL;
    int probe = hash_value_for_oopMap(method, bci);
    for (int i = 0;i < OopMapCache::_probe_depth;i++) {
        entry = cache->entry_at(probe + i);
        if (entry->match(methodHandle(method), bci)) {
            // fill in the map
            map->resource_copy(entry);
            return;
        }
    }

    ShouldNotReachHere();
}