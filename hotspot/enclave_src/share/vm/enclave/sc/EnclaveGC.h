//
// Created by max on 3/7/18.
//

#ifndef HOTSPOT_ENCLAVE_GC_H
#define HOTSPOT_ENCLAVE_GC_H

#include "oops/oop.hpp"
#include "queue.hpp"

// this data structure define two space, eden and survivor

// GC policy: expand 2 times when there are not enough memory
// when allocate of 2 time memory fails, then allocate the same size of memory

// TODO: a multi-thread version of gc
#ifdef DB_GC
#define GC_DEBUG(...) printf(__VA_ARGS__)
#else
#define GC_DEBUG(...)
#endif

#define ENCLAVE_EDEN_SIZE 20*M

//#define PUSH_TASK(oop) { push_task(oop); GC_DEBUG("   [GC OBJ]add gc root %lx, index %d, %s ,%s, %d\n", (intptr_t)(*oop), offset,(*oop)->klass()->name()->as_C_string(),__FILE__, __LINE__); }
#define PUSH_TASK(oop) { push_task(oop); GC_DEBUG("   [GC OBJ]add gc root %lx -> %lx, index %d, %s:%d\n", (intptr_t)oop,(intptr_t)(*oop), offset,__FILE__, __LINE__); }
#define PUSH_CLEAR(oop) { _clearQueue->push_back(oop); GC_DEBUG("   [GC OBJ] add clear task %lx\n", (intptr_t)(oop)); }

typedef oop* StarTask;

#define GC_ALLOCATE_HEAP(size) JavaThread::current()->mem()->allocate_eden(size)
#define THREAD_ALLOCATE_HEAP(thread, size) thread->mem()->allocate_eden(size)
#define DO_GC() JavaThread::current()->mem()->allocate_eden(size)
#define THREAD_GC(thread) thread->mem()->allocate_eden(size)
#define CUR_GC JavaThread::current()->mem()

class EnclaveGC {
public:

    #define PS_CHUNKED_ARRAY_OOP_MASK  0x2
    #define MAX_REGION 8

    static bool is_oop_masked(StarTask p) {
        // If something is marked chunked it's always treated like wide oop*
        return (((intptr_t)(oop*)p) & PS_CHUNKED_ARRAY_OOP_MASK) ==
               PS_CHUNKED_ARRAY_OOP_MASK;
    }

    static volatile int regions[MAX_REGION];

    int region_id;
    void* _eden;
    // 0 - _eden -> eden, else _survior -> eden
    void* _eden_start;
    void* _eden_end;
    int current_size;
    enclave::queue<StarTask>* _taskQueue;
    enclave::queue<StarTask>* _clearQueue;
    markOop region_mark;

    static ByteSize eden_end_offset()                    { return byte_offset_of(EnclaveGC, _eden_end); }
    static ByteSize eden_start_offset()                    { return byte_offset_of(EnclaveGC, _eden_start); }
    static ByteSize region_mark_offset()                    { return byte_offset_of(EnclaveGC, region_mark); }

    explicit EnclaveGC() {
        init();
    }

    inline markOop mark_prototype() {
        return region_mark;
    }

    void init();

    void reset();

    void* allocate_eden(int size);

    inline void push_task(oop* task) {
        _taskQueue->push(StarTask(task));
        _clearQueue->push(task);
    }

    template <class T>
    inline void claim_or_forward_depth(T* p) {
        if (p != NULL) { // XXX: error if p != NULL here
            oop o = oopDesc::load_decode_heap_oop_not_null(p);
            if (!o->is_gc_marked()) {
                _taskQueue->push(StarTask(p));
            }
        }
    }

    template <class T>
    inline void adjust_pointers(T* l) {
        oop o = oopDesc::load_decode_heap_oop_not_null(l);
        oop new_obj = o->forwardee();
        if (!is_obj_in_young((address)new_obj)) {
            GC_DEBUG("not found %lx -> %lx\n", l, new_obj);
            ShouldNotReachHere();
        }
        GC_DEBUG("adjust %lx -> %lx\n", l, new_obj);
        oopDesc::encode_store_heap_oop_not_null(l, new_obj);
    }

    inline oop copy_to_allocated_space(oop o);

    inline bool is_obj_in_young(address p) {
        bool in = p >= (address)_eden && p < ((address)_eden + current_size);
        return in;
    }

    void copy_and_push_safe_barrier(oop* p);

    void frame_do_gc(frame* current_f, RegisterMap* map, TRAPS);

    void frame_argument(frame *f, Symbol *signature, bool has_receiver);

    void frame_expressions(frame* f, methodHandle m, int bci, int max_local);

    // do gc inside enclave
    void enclave_gc(JavaThread*);

    void compute_oopmap(Method*, int bci, InterpreterOopMap *map);

    void mark_phase(enclave::queue<StarTask> *task_list);
    void sweep_phase();
    void adjust_phase(enclave::queue<StarTask> *task_list);
    void copy_phase();
    ~EnclaveGC();
};


#endif //HOTSPOT_ENCLAVE_GC_H
