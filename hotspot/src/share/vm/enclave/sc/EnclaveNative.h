//
// Created by Maxxie Jiang on 31/1/2018.
//

#ifndef HOTSPOT_SHAREDHEADER_H
#define HOTSPOT_SHAREDHEADER_H

#include <precompiled.hpp>

#define ENCLAVE_NATIVE_EUNM(name)   name
#define VM_ENCLAVE_ENUM(name, string, func) ENCLAVE_NATIVE_EUNM(name),
#define VM_ENCLAVE_ENTRY(name, string, func)    native_name[name] = (char*)(string);        \
                                                native_entry[name] = CAST_FROM_FN_PTR(void*, func);
#define DO_ENCLAVE_NATIVE(template) \
    template(system_arraycopy,                  "Java_java_lang_System_arraycopy",                      &JVM_ArrayCopy)                     \
    template(system_identityHash,               "Java_java_lang_System_identityHashCode",               &System_identityHashCode)           \
    template(object_clone,                      "Java_java_lang_Object_clone",                          &JVM_Clone)                         \
    template(object_hashcode,                   "Java_java_lang_Object_hashCode",                       &JVM_IHashCode)                     \
    template(object_notifyAll,                  "Java_java_lang_Object_notifyAll",                      &JVM_MonitorNotifyAll)              \
    template(object_notify,                     "Java_java_lang_Object_notify",                         &JVM_MonitorNotify)                 \
    template(array_new,                         "Java_java_lang_reflect_Array_newArray",                &JVM_EnclaveNewArray)               \
    template(array_multi_new,                   "Java_java_lang_reflect_Array_multiNewArray",           &JVM_EnclaveNewMultiArray)          \
    template(array_get,                         "NULL",                                                 NULL)                               \
    template(array_set,                         "Java_java_lang_reflect_Array_set",                     &JVM_EnclaveSetArrayElement)        \
    template(class_get,                         "Java_java_lang_Object_getClass",                       jni_functions()->GetObjectClass)    \
    template(class_getName,                     "Java_java_lang_Class_getName0",                        &JVM_GetClassName)                  \
    template(class_getSuper,                    "Java_java_lang_Class_getSuperclass",                   jni_functions()->GetSuperclass)     \
    template(class_component,                   "Java_java_lang_Class_getComponentType",                &JVM_GetComponentType)              \
    template(class_isInstance,                  "Java_java_lang_Class_isInstance",                      &JVM_IsInstanceOf)                  \
    template(class_isArray,                     "Java_java_lang_Class_isArray",                         &JVM_IsArrayClass)                  \
    template(class_isInterface,                 "Java_java_lang_Class_isInterface",                     &JVM_IsInterface)                   \
    template(class_isPrimitive,                 "Java_java_lang_Class_isPrimitive",                     &JVM_IsPrimitiveClass)              \
    template(class_isAssignableFrom,            "Java_java_lang_Class_isAssignableFrom",                jni_functions()->IsAssignableFrom)  \
    template(class_getInterfaces,               "Java_java_lang_Class_getInterfaces0",                  &JVM_GetClassInterfaces)            \
    template(throwable_FillInStackTrace,        "Java_java_lang_Throwable_fillInStackTrace",            &JVM_FillInStackTrace)              \
    template(throwable_GetStackTraceDepth,      "Java_java_lang_Throwable_getStackTraceDepth",          &JVM_GetStackTraceDepth)            \
    template(throwable_GetStackTraceElement,    "Java_java_lang_Throwable_getStackTraceElement",        &JVM_GetStackTraceElement)          \
    template(tools_print,                       "Java_edu_anonymity_sgx_Tools_print",                   &JVM_EnclaveDebug)                  \
    template(tools_clone,                       "Java_edu_anonymity_sgx_Tools_copy_1out",               &JVM_EnclaveCopy)                   \
    template(tools_deep_copy,                   "Java_edu_anonymity_sgx_Tools_deep_1copy",              &JVM_EnclaveDeepCopy)               \
    template(tools_clean,                       "Java_edu_anonymity_sgx_Tools_clean",                   &JVM_EnclaveClean)                  \
    template(thread_currentThread,              "Java_java_lang_Thread_currentThread",                  &JVM_CurrentThread)                 \
    template(sgx_encrypt,                       "Java_edu_anonymity_sgx_Crypto_sgx_1encrypt",           &JVM_BytesEncrypt)                  \
    template(sgx_decrypt,                       "Java_edu_anonymity_sgx_Crypto_sgx_1decrypt",           &JVM_BytesDecrypt)                  \
    template(sgx_encrypt_int,                   "Java_edu_anonymity_sgx_Crypto_sgx_1encrypt_1int",      &JVM_BytesEncryptInt)               \
    template(sgx_decrypt_int,                   "Java_edu_anonymity_sgx_Crypto_sgx_1decrypt_1int",      &JVM_BytesDecryptInt)               \
    template(sgx_encrypt_double,                "Java_edu_anonymity_sgx_Crypto_sgx_1encrypt_1double",   &JVM_BytesEncryptDouble)            \
    template(sgx_decrypt_double,                "Java_edu_anonymity_sgx_Crypto_sgx_1decrypt_1double",   &JVM_BytesDecryptDouble)            \
    template(sgx_hash,                          "Java_edu_anonymity_sgx_Crypto_sgx_1hash",              &JVM_BytesHash)                     \
    template(sgx_verify,                        "Java_edu_anonymity_sgx_Crypto_sgx_1verify",            &JVM_BytesHashVerify)               \
    template(sun_unsafe_compareSwapObject,      "Java_sun_misc_Unsafe_compareAndSwapObject",            &Unsafe_CompareAndSwapObject)       \
    template(sun_unsafe_compareSwapLong,        "Java_sun_misc_Unsafe_compareAndSwapLong",              &Unsafe_CompareAndSwapLong)         \
    template(sun_unsafe_compareSwapInt,         "Java_sun_misc_Unsafe_compareAndSwapInt",               &Unsafe_CompareAndSwapInt)          \
    template(sun_unsafe_getObject,              "Java_sun_misc_Unsafe_getObject",                       &Unsafe_GetObject)                  \
    template(sun_unsafe_getLong,                "Java_sun_misc_Unsafe_getLong",                         &Unsafe_GetLong)                    \
    template(sun_unsafe_getInt,                 "Java_sun_misc_Unsafe_getInt",                          &Unsafe_GetInt)                     \
    template(sun_unsafe_getShort,               "Java_sun_misc_Unsafe_getShort",                        &Unsafe_GetShort)                   \
    template(sun_unsafe_getFloat,               "Java_sun_misc_Unsafe_getFloat",                        &Unsafe_GetFloat)                   \
    template(sun_unsafe_getDouble,              "Java_sun_misc_Unsafe_getDouble",                       &Unsafe_GetDouble)                  \
    template(sun_unsafe_getChar,                "Java_sun_misc_Unsafe_getChar",                         &Unsafe_GetChar)                    \
    template(sun_unsafe_getByte,                "Java_sun_misc_Unsafe_getByte",                         &Unsafe_GetByte)                    \
    template(sun_unsafe_getObjectVolatile,      "Java_sun_misc_Unsafe_getObjectVolatile",               &Unsafe_GetObjectVolatile)          \
    template(sun_unsafe_getIntVolatile,         "Java_sun_misc_Unsafe_getIntVolatile",                  &Unsafe_GetIntVolatile)             \
    template(sun_unsafe_getLongVolatile,        "Java_sun_misc_Unsafe_getLongVolatile",                 &Unsafe_GetLongVolatile)            \
    template(sun_unsafe_getShortVolatile,       "Java_sun_misc_Unsafe_getShortVolatile",                &Unsafe_GetShortVolatile)           \
    template(sun_unsafe_getFloatVolatile,       "Java_sun_misc_Unsafe_getFloatVolatile",                &Unsafe_GetFloatVolatile)           \
    template(sun_unsafe_getByteVolatile,        "Java_sun_misc_Unsafe_getByteVolatile",                 &Unsafe_GetByteVolatile)            \
    template(sun_unsafe_getCharVolatile,        "Java_sun_misc_Unsafe_getCharVolatile",                 &Unsafe_GetCharVolatile)            \
    template(sun_unsafe_getDoubleVolatile,      "Java_sun_misc_Unsafe_getDoubleVolatile",               &Unsafe_GetDoubleVolatile)          \
    template(sun_unsafe_putObjectVolatile,      "Java_sun_misc_Unsafe_putObjectVolatile",               &Unsafe_SetObjectVolatile)          \
    template(sun_unsafe_putIntVolatile,         "Java_sun_misc_Unsafe_putIntVolatile",                  &Unsafe_SetIntVolatile)             \
    template(sun_unsafe_putLongVolatile,        "Java_sun_misc_Unsafe_putLongVolatile",                 &Unsafe_SetLongVolatile)            \
    template(sun_unsafe_putShortVolatile,       "Java_sun_misc_Unsafe_putShortVolatile",                &Unsafe_SetShortVolatile)           \
    template(sun_unsafe_putFloatVolatile,       "Java_sun_misc_Unsafe_putFloatVolatile",                &Unsafe_SetFloatVolatile)           \
    template(sun_unsafe_putByteVolatile,        "Java_sun_misc_Unsafe_putByteVolatile",                 &Unsafe_SetByteVolatile)            \
    template(sun_unsafe_putCharVolatile,        "Java_sun_misc_Unsafe_putCharVolatile",                 &Unsafe_SetCharVolatile)            \
    template(sun_unsafe_putDoubleVolatile,      "Java_sun_misc_Unsafe_putDoubleVolatile",               &Unsafe_SetDoubleVolatile)          \
    template(sun_unsafe_copyMemory,             "Java_sun_misc_Unsafe_copyMemory",                      &Unsafe_CopyMemory)                 \
    template(double_long_bit_to_double,         "Java_java_lang_Double_longBitsToDouble",               &Double_longBitsToDouble)           \
    template(double_double_to_long_bit,         "Java_java_lang_Double_doubleToRawLongBits",            &Double_doubleToRawLongBits)


extern "C" {
    extern jboolean Unsafe_CompareAndSwapObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject e, jobject x);
    extern jboolean Unsafe_CompareAndSwapLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong e, jlong x);
    extern jboolean Unsafe_CompareAndSwapInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint e, jint x);
    extern jobject Unsafe_GetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jlong Unsafe_GetLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jint Unsafe_GetInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jshort Unsafe_GetShort(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jdouble Unsafe_GetDouble(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jfloat Unsafe_GetFloat(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jchar Unsafe_GetChar(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jbyte Unsafe_GetByte(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jobject Unsafe_GetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jlong Unsafe_GetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jint Unsafe_GetIntVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jshort Unsafe_GetShortVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jfloat Unsafe_GetFloatVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jbyte Unsafe_GetByteVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jchar Unsafe_GetCharVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern jdouble Unsafe_GetDoubleVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
    extern void Unsafe_SetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h);
    extern void Unsafe_SetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong x);
    extern void Unsafe_SetIntVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint x);
    extern void Unsafe_SetShortVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jshort x);
    extern void Unsafe_SetFloatVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jfloat x);
    extern void Unsafe_SetByteVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jbyte x);
    extern void Unsafe_SetCharVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jchar x);
    extern void Unsafe_SetDoubleVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jdouble x);
    extern void Unsafe_CopyMemory(JNIEnv *env, jobject unsafe, jlong srcAddr, jlong dstAddr, jlong size);

    JNIEXPORT jdouble JNICALL
    Double_longBitsToDouble(JNIEnv *env, jclass unused, jlong v);

    /*
     * Find the bit pattern corresponding to a given double float, NOT collapsing NaNs
     */
    JNIEXPORT jlong JNICALL
    Double_doubleToRawLongBits(JNIEnv *env, jclass unused, jdouble v);

    JNIEXPORT jint JNICALL
    System_identityHashCode(JNIEnv *env, jobject ignore, jobject x);

    jboolean
    JVM_IsInstanceOf(JNIEnv *env, jclass c, jobject o);

}

class EnclaveNative {
public:
    enum Natives {
        DO_ENCLAVE_NATIVE(VM_ENCLAVE_ENUM)
        NATIVE_COUNT
    };

    static char* native_name[NATIVE_COUNT];

    static void* native_entry[NATIVE_COUNT];

    static void init();

    static void resolve_function(Method*);
};

#endif //HOTSPOT_SHAREDHEADER_H
