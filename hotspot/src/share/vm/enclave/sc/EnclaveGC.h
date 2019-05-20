//
// Created by max on 3/7/18.
//

#ifndef HOTSPOT_ENCLAVE_GC_H
#define HOTSPOT_ENCLAVE_GC_H

#include <list>
#include <queue>
#include "oops/oop.hpp"
#include "utilities/taskqueue.hpp"

// this data structure define two space, eden and survivor

// GC policy: expand 2 times when there are not enough memory
// when allocate of 2 time memory fails, then allocate the same size of memory

// TODO: a multi-thread version of gc
#ifdef DB_GC
#define GC_DEBUG(...) printf(__VA_ARGS__)
#else
#define GC_DEBUG(...)
#endif

#define ENCLAVE_EDEN_SIZE 40*M

//#define PUSH_TASK(oop) { push_task(oop); GC_DEBUG("   [GC OBJ]add gc root %lx, index %d, %s ,%s, %d\n", (intptr_t)(*oop), offset,(*oop)->klass()->name()->as_C_string(),__FILE__, __LINE__); }
#define PUSH_TASK(oop) { push_task(oop); GC_DEBUG("   [GC OBJ]add gc root %lx -> %lx, index %d, %s:%d\n", (intptr_t)oop,(intptr_t)(*oop), offset,__FILE__, __LINE__); }
#define PUSH_CLEAR(oop) { _clearQueue->push_back(oop); GC_DEBUG("   [GC OBJ] add clear task %lx\n", (intptr_t)(oop)); }
class EnclaveGC {
public:

    #define PS_CHUNKED_ARRAY_OOP_MASK  0x2

    static bool is_oop_masked(StarTask p) {
        // If something is marked chunked it's always treated like wide oop*
        return (((intptr_t)(oop*)p) & PS_CHUNKED_ARRAY_OOP_MASK) ==
               PS_CHUNKED_ARRAY_OOP_MASK;
    }

    static void* _eden;
    static void* _survivor;
    static int memory_ratio;
    // 0 - _eden -> eden, else _survior -> eden
    static void* _eden_start;
    static void* _eden_end;
    static void* _survivor_start;
    static bool _young_gen_is_full;
    static int current_size;
    static std::queue<StarTask>* _taskQueue;
    static std::queue<oop>* _clearQueue;

    static void init();

    static void reset();

    static void* allocate_eden(int size);

    static void* allocate_survivor(int size);

    static inline void push_task(oop* task) {
        _taskQueue->push(StarTask(task));
    }

    template <class T>
    static inline void claim_or_forward_depth(T* p) {
        if (p != NULL) { // XXX: error if p != NULL here
            oop o = oopDesc::load_decode_heap_oop_not_null(p);
            if (o->is_forwarded()) {
                o = o->forwardee();
                // Card mark
//                if (PSScavenge::is_obj_in_young(o)) {
//                    PSScavenge::card_table()->inline_write_ref_field_gc(p, o);
//                }
                oopDesc::encode_store_heap_oop_not_null(p, o);
            } else {
                _taskQueue->push(StarTask(p));
                GC_DEBUG("      [GC Task Node] add task %lx\n", (intptr_t)(*p));
            }
        }
    }

    static oop copy_to_allocated_space(oop o);

    static bool is_obj_in_young(address p);

    static bool is_obj_in_new(address p);

    static void copy_and_push_safe_barrier(oop* p);

    static void do_gc(std::queue<StarTask> *task_list);

    static void gc_start(StarTask *task_out, int n);

    static void frame_do_gc(frame* current_f, RegisterMap* map, TRAPS);

    static void frame_argument(frame *f, Symbol *signature, bool has_receiver);

    static void frame_expressions(frame* f, methodHandle m, int bci, int max_local);

    // do gc inside enclave
    static void enclave_gc(JavaThread*);

    static void compute_oopmap(Method*, int bci, InterpreterOopMap *map);
};


#endif //HOTSPOT_ENCLAVE_GC_H
