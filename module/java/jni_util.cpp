//
// Created by m.fletcher on 10/07/2020.
//

#include "jni_util.h"

using namespace jni;

unique_jstring_ptr jni::make_unique_jstring(JNIEnv* env, jstring& str)
{
    const char* str_value = env->GetStringUTFChars(str, 0);

    if(str_value == NULL)
    {
        env->ThrowNew(env->FindClass("java/lang/OutOfMemoryError"), "Unable to read jstring input");
        return NULL;
    }

    return unique_jstring_ptr(str_value, [=](char const* p) mutable { env->ReleaseStringUTFChars(str, p); });
}
