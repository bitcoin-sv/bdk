//
// Created by m.fletcher on 10/07/2020.
//
#pragma once

#include <functional>
#include <jni.h> // JNI header provided by JDK
#include <memory>

namespace bsv::jni
{
    using unique_jstring_ptr =
        std::unique_ptr<const char[], std::function<void(const char*)>>;

    unique_jstring_ptr make_unique_jstring(JNIEnv*, const jstring&);
}

