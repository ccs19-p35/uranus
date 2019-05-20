
#include <prims/nativeLookup.hpp>
#include "EnclaveNative.h"

extern struct JNINativeInterface_* jni_functions();

char* EnclaveNative::native_name[] = {0};
void* EnclaveNative::native_entry[] = {0};

void EnclaveNative::init(){
   DO_ENCLAVE_NATIVE(VM_ENCLAVE_ENTRY)
}

void EnclaveNative::resolve_function(Method* m) {
    char* name = NativeLookup::pure_jni_name(m);
    for (int i = 0;i < EnclaveNative::NATIVE_COUNT;i++) {
        if (strstr(name, EnclaveNative::native_name[i]) != NULL) {
            m->set_enclave_native_function(CAST_FROM_FN_PTR(address, EnclaveNative::native_entry[i]));
//            printf("set entry %s %lx\n", name, (intptr_t)EnclaveNative::native_name[i]);
            return;
        }
    }
    printf(D_ERROR("Native")" cannot find native func %s\n", name);
}

extern "C" {

    JNIEXPORT jdouble JNICALL
    Double_longBitsToDouble(JNIEnv *env, jclass unused, jlong v)
    {
        union {
            jlong l;
            double d;
        } u;
        u.l = v;
        return (jdouble)u.d;
    }

    /*
     * Find the bit pattern corresponding to a given double float, NOT collapsing NaNs
     */
    JNIEXPORT jlong JNICALL
            Double_doubleToRawLongBits(JNIEnv *env, jclass unused, jdouble v)
    {
        union {
            jlong l;
            double d;
        } u;
        u.d = (double)v;
        return u.l;
    }

    JNIEXPORT jint JNICALL
    System_identityHashCode(JNIEnv *env, jobject ignore, jobject x) {
        return JVM_IHashCode(env, x);
    }

    jboolean
    JVM_IsInstanceOf(JNIEnv *env, jclass c, jobject o) {
        return jni_functions()->IsInstanceOf(env, o, c);
    }

}