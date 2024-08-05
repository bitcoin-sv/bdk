//
// Created by m.fletcher on 10/07/2020.
//
#pragma once

#include <cassert>
#include <functional>
#include <jni.h> // JNI header provided by JDK
#include <memory>

namespace bsv::jni
{
    // precondition env != nullptr
    inline auto make_unique_jstring(JNIEnv* env, const jstring& jstr)
    {
        assert(env);

        const char* utf{env->GetStringUTFChars(jstr, nullptr)};
        if(!utf)
        {
            env->ThrowNew(env->FindClass("java/lang/OutOfMemoryError"),
                          "Unable to read jstring input");

            return std::unique_ptr<const char[],
                                   std::function<void(const char*)>>{};
        }

        return std::unique_ptr<const char[], std::function<void(const char*)>>(
            utf, [env, jstr](const char* utf) {
                env->ReleaseStringUTFChars(jstr, utf);
            });
    }
}
