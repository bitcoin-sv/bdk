//
// Created by m.fletcher on 10/07/2020.
//

#ifndef BSCRYPT_JNI_UTIL_H
#define BSCRYPT_JNI_UTIL_H

#include <jni.h>                      // JNI header provided by JDK
#include <memory>
#include <functional>

namespace jni{
    using unique_jstring_ptr = std::unique_ptr<char const[], std::function<void(char const*)>>;

    unique_jstring_ptr make_unique_jstring(JNIEnv* env, jstring& str);
}

#endif // BSCRYPT_JNI_UTIL_H
