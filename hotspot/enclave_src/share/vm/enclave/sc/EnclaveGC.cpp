//
// Created by max on 3/7/18.
//

#include <precompiled.hpp>
#include <interpreter/oopMapCache.hpp>
#include "EnclaveGC.h"
#include "securecompiler.h"
#include "EnclaveOcall.h"
#include "EnclaveMemory.h"
#include "queue.hpp"

volatile int EnclaveGC::regions[MAX_REGION];

void EnclaveGC::init() {
    _eden = malloc(ENCLAVE_EDEN_SIZE);
    _eden_start = _eden;
    _eden_end = (address)_eden + ENCLAVE_EDEN_SIZE;
    _taskQueue = NULL;
    _clearQueue = NULL;
    current_size = ENCLAVE_EDEN_SIZE;

    int idx = 0;
    while (true) {
        idx += 1;
        int flag = regions[idx];
        if (flag == 1) {
            continue;
        }
        if (idx == MAX_REGION) {
            ShouldNotReachHere();
        }
        if (Atomic::cmpxchg(1, &regions[idx], flag) == 0) {
            region_id = idx;
            break;
        }
    }
    region_mark = markOop(markOopDesc::no_hash_in_place | markOopDesc::no_lock_in_place | (region_id << markOopDesc::age_shift));
}

void EnclaveGC::reset() {
    _eden_end = _eden + current_size;
    _eden_start = _eden;
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
    bool printGCTime = false;
    GC_DEBUG("[GC] start gc now %lx\n\n", (intptr_t)thread);
    if (!_taskQueue)
        _taskQueue = new enclave::queue<StarTask>(400000);
    if (!_clearQueue)
        _clearQueue = new enclave::queue<StarTask>(400000);
    // push stack oop in the gc task
    // TODO: other gc object

    printGCTime = ((EnclaveRuntime::debug_bit & debug_gc) == debug_gc);

    // do gc on all threads

    // printf("do gc on thread %lx\n", *itr);
    JavaThread* t = thread;
    RegisterMap map = RegisterMap(t);
    frame f = t->last_frame();
    while (true) {
        // if it is zero, then we reach the top of ecall
        if (!within_enclave(f.unextended_sp())) {
            GC_DEBUG("[Frame] init frame\n");
            break;
        }
        GC_DEBUG("[Frame]: sp:%lx, sender: %lx\n",
                 (intptr_t)f.sp(),
                 (intptr_t)f.unextended_sp());
        frame_do_gc(&f, &map, t);

        // we should use this method directly instead of sender()
        f = f.sender_for_interpreter_frame(&map);
    }

    // we push a null task, so that we will not mark the non-root object (see mark phase)
    _taskQueue->push(NULL);

    struct timeval ts, te, tmark, tsweep, tcopy;
    if (printGCTime)
        gettimeofday(&ts, 0);
    mark_phase(_taskQueue);
    // gettimeofday(&tmark, 0);
    sweep_phase();
    // gettimeofday(&tsweep, 0);
    adjust_phase(_clearQueue);
    // gettimeofday(&tcopy, 0);
    copy_phase();
    if (printGCTime)
        gettimeofday(&te, 0);
    // printf("time %ld\n", (tmark.tv_sec - ts.tv_sec) * 1000000 + (tmark.tv_usec - ts.tv_usec));
    // printf("time %ld\n", (tsweep.tv_sec - tmark.tv_sec) * 1000000 + (tsweep.tv_usec - tmark.tv_usec));
    // printf("time %ld\n", (tcopy.tv_sec - tsweep.tv_sec) * 1000000 + (tcopy.tv_usec - tsweep.tv_usec));
    if (printGCTime)
        printf("time %ld\n", (te.tv_sec - ts.tv_sec) * 1000000 + (te.tv_usec - ts.tv_usec));
    _taskQueue->empty();
    _clearQueue->empty();

    GC_DEBUG("[GC] gc finish\n");

    return;
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

void EnclaveGC::mark_phase(enclave::queue<StarTask> *task_list) {
    oop* task;
    oop obj;
    Klass* klass;
    bool changeMark = true;
    while (task_list->size() > 0) {
        task = task_list->front();
        task_list->pop();
        if (task == NULL) {
            changeMark = false;
            continue;
        }
        obj = *task;
        if (is_obj_in_young(address(obj)) && !obj->is_gc_marked()) {
            GC_DEBUG("mark %lx -> %lx %lx\n", task, obj, *((oop*)((address)(obj) + 0x10)));
            obj->set_mark(obj->mark()->set_marked());
            klass = obj->klass();
            if (!klass->oop_is_typeArray()) {
                // It might contain oops beyond the header, so take the virtual call.
                if (klass->oop_is_objArray()) {
                    ObjArrayKlass::oop_push_contents((ObjArrayKlass*)klass, obj);
                } else {
                    InstanceKlass::oop_push_contents((InstanceKlass*)klass, obj);
                }
            }
        }
        if (is_obj_in_young(address(obj)) && changeMark) {
            *task = (oop)((char*)(obj) + ENCLAVE_EDEN_SIZE);
        }
    }
}

void EnclaveGC::sweep_phase() {
    oop cur_oop;
    oop new_obj;
    HeapWord *start, *eden_cur;
    start = (HeapWord*)_eden;
    int size;
    eden_cur = (HeapWord*)_eden;

    HeapWord *last_sweep_oop = NULL;
    int sweep_oop_heap_words = 0;
    while (eden_cur < _eden_start) {
        cur_oop = (oop)eden_cur;
        size = cur_oop->size();
        if (cur_oop->is_gc_marked()) {
            // mark_copy
            new_obj = (oop)start;
            start += size;
            cur_oop->forward_to(new_obj);
            GC_DEBUG("forward %lx %lx\n", cur_oop, new_obj);
            if (last_sweep_oop) {
                ((oop)last_sweep_oop)->set_mark(markOop(sweep_oop_heap_words << 2));
                last_sweep_oop = NULL;
                sweep_oop_heap_words = 0;
            }
        } else {
            if (!last_sweep_oop) {
                last_sweep_oop = eden_cur;
                sweep_oop_heap_words += size;
            } else {
                sweep_oop_heap_words += size;
            }
        }
        eden_cur += size;
    }

    if (last_sweep_oop) {
        ((oop)last_sweep_oop)->set_mark(markOop(sweep_oop_heap_words << 2));
    }

}

void EnclaveGC::adjust_phase(enclave::queue<StarTask> *task_list) {
    oop* task;
    oop new_obj, old_obj;
    Klass* klass;
    // TODO: the task_list may contains the same task, how can we solve it?
    while (task_list->size() > 0) {
        task = task_list->front();
        task_list->pop();
        old_obj = *task;
        if (!is_obj_in_young((address)(old_obj)) && is_obj_in_young((address)((char*)old_obj - ENCLAVE_EDEN_SIZE))) {
            old_obj = (oop)((char*)old_obj - ENCLAVE_EDEN_SIZE);
            new_obj = old_obj->forwardee();
            if (!old_obj->is_gc_marked() || !is_obj_in_young((address)new_obj)) {
                // There are problems
                GC_DEBUG("not found %lx -> %lx\n", task, old_obj);
                ShouldNotReachHere();
            } else {
                GC_DEBUG("adjust %lx -> %lx (from %lx)\n", task, new_obj, old_obj);
                oopDesc::encode_store_heap_oop_not_null(task, new_obj);
            }
        }
    }
    oop cur_oop;
    HeapWord *eden_cur;

    int size;
    eden_cur = (HeapWord*)_eden;
    while (eden_cur < _eden_start) {
        cur_oop = (oop)eden_cur;
        size = cur_oop->size();
        if (cur_oop->is_gc_marked()) {
            // adjust fields
            klass = cur_oop->klass();
            if (!klass->oop_is_typeArray()) {
                GC_DEBUG("adjust obj field: %lx\n", cur_oop);
                // It might contain oops beyond the header, so take the virtual call.
                if (klass->oop_is_objArray()) {
                    ObjArrayKlass::oop_adjust_pointers((ObjArrayKlass*)klass, cur_oop);
                } else {
                    InstanceKlass::oop_adjust_pointers((InstanceKlass*)klass, cur_oop);
                }
            }
            eden_cur += size;
        } else {
            int count = (intptr_t)cur_oop->mark() >> 2;
            eden_cur += count;
        }

    }
}

void EnclaveGC::copy_phase() {
    oop cur_oop, new_obj;
    HeapWord *start, *eden_cur;
    start = (HeapWord*)_eden;

    int size;
    eden_cur = (HeapWord*)_eden;
    while (eden_cur < _eden_start) {
        cur_oop = (oop)eden_cur;
        size = cur_oop->size();
        if (cur_oop->is_gc_marked()) {
            GC_DEBUG("copy %lx -> %lx\n", eden_cur, start);
            // adjust fields
            if (eden_cur != start)
                Copy::aligned_conjoint_words(eden_cur, start, size);
            new_obj = (oop)start;
            new_obj->set_mark(mark_prototype());
            start += size;
            eden_cur += size;
        } else {
            int count = (intptr_t)cur_oop->mark() >> 2;
            eden_cur += count;
        }

    }
    // final processing
    _eden_start = start;
}

EnclaveGC::~EnclaveGC() {
    free(_eden);
    if (_taskQueue == NULL)
        delete _taskQueue;
    if (_clearQueue)
        delete _clearQueue;
    // free the slot
    regions[region_id] = 0;
}