#include <jni.h>
#include <stdio.h>
#include "JNI_Test.h"

JNIEXPORT void JNICALL Java_JNI_1Test_print
(JNIEnv *env, jobject obj)
{
     printf("hello MEMEDA233333");
     return;
}
