//
// Created by m.fletcher on 10/07/2020.
//
#include "jni_memory.h"

#include <cassert>

bsv::jni::unique_jstring_ptr bsv::jni::make_unique_jstring(JNIEnv* env,
                                                           const jstring& jstr)
{
    assert(env);

    const char* utf{env->GetStringUTFChars(jstr, nullptr)};
    if(!utf)
    {
        env->ThrowNew(env->FindClass("java/lang/OutOfMemoryError"), "Unable to read jstring input");
        return nullptr;
    }

    return unique_jstring_ptr(utf, [env, jstr](const char* utf) {
        env->ReleaseStringUTFChars(jstr, utf);
    });
}
