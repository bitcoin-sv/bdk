#include <ScriptEngineIF.h>
#include <functional>
#include <iostream>
#include <jni.h>                      // JNI header provided by JDK
#include <jni_bscriptIF_BScriptJNI.h> // Generated
#include <memory>
#include <vector>

namespace
{
    using unique_jstring_ptr = std::unique_ptr<char const[], std::function<void(char const*)>>;

    unique_jstring_ptr make_unique_jstring(jstring& str, JNIEnv* env)
    {
        const char* str_value = env->GetStringUTFChars(str, 0);

        if(str_value == NULL)
        {
            env->ThrowNew(env->FindClass("java/lang/OutOfMemoryError"), "Unable to read jstring input");
            return NULL;
        }

        return unique_jstring_ptr(str_value,
                                  [=](char const* p) mutable { env->ReleaseStringUTFChars(str, p); });
    }
}

JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScript(JNIEnv* env,
                                                                   jobject obj,
                                                                   jbyteArray arr,
                                                                   jboolean concensus,
                                                                   jint scriptflags,
                                                                   jstring hextx,
                                                                   jint nidx,
                                                                   jint amount)
{

    int len = env->GetArrayLength(arr);

    std::vector<uint8_t> script(len);
    env->GetByteArrayRegion(arr, 0, len, reinterpret_cast<jbyte*>(script.data()));

    const unique_jstring_ptr hextxptr = make_unique_jstring(hextx, env);

    // class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID method_id = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    // execute the script
    ScriptError script_result = SCRIPT_ERR_UNKNOWN_ERROR;
    try
    {
        script_result = bsv::evaluate(script, concensus, scriptflags, hextxptr.get(), nidx, amount);
    }
    catch(const std::runtime_error& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    }
    catch(const std::exception& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return NULL;
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(script_result));
        return NULL;
    }

    // last arguments are method parameters
    jobject result =
        env->NewObject(clazz, method_id, script_result, env->NewStringUTF(ScriptErrorString(script_result)));

    return result;
}

JNIEXPORT jboolean JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScript(JNIEnv*, jobject, jbyteArray)
{

    return false;
}

JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScriptString(JNIEnv* env,
                                                                         jobject obj,
                                                                         jstring script,
                                                                         jboolean concensus,
                                                                         jint scriptflags,
                                                                         jstring hextx,
                                                                         jint nidx,
                                                                         jint amount)
{

    // tx and script data
    const unique_jstring_ptr raw_script = make_unique_jstring(script, env);
    const unique_jstring_ptr hextxptr = make_unique_jstring(hextx, env);

    // class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID method_id = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    // run the script
    ScriptError script_result = SCRIPT_ERR_UNKNOWN_ERROR;
    try
    {
        script_result = bsv::evaluate(std::string{raw_script.get()}, concensus, scriptflags, hextxptr.get(),
                                      nidx, amount);
    }
    catch(const std::runtime_error& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    }
    catch(const std::exception& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return NULL;
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(script_result));
        return NULL;
    }

    jobject result =
        env->NewObject(clazz, method_id, script_result, env->NewStringUTF(ScriptErrorString(script_result)));

    return result;
}

JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScriptString(JNIEnv* env,
                                                                           jobject obj,
                                                                           jstring scriptsig,
                                                                           jstring scriptpubkey,
                                                                           jboolean concensus,
                                                                           jint scriptflags,
                                                                           jstring hextx,
                                                                           jint nidx,
                                                                           jint amount)
{

    // script / public key data
    const unique_jstring_ptr raw_script_sig = make_unique_jstring(scriptsig, env);
    const unique_jstring_ptr raw_script_pubkey = make_unique_jstring(scriptpubkey, env);
    const unique_jstring_ptr hextxptr = make_unique_jstring(hextx, env);

    // class we want to return
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID method_id = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck())
    {
        return NULL;
    }

    ScriptError script_result = SCRIPT_ERR_UNKNOWN_ERROR;
    try
    {
        script_result =
            bsv::verifyScript(std::string{raw_script_sig.get()}, std::string{raw_script_pubkey.get()},
                              concensus, scriptflags, std::string{hextxptr.get()}, nidx, amount);
    }
    catch(const std::runtime_error& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    }
    catch(const std::exception& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return NULL;
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(script_result));
        return NULL;
    }

    jobject result =
        env->NewObject(clazz, method_id, script_result, env->NewStringUTF(ScriptErrorString(script_result)));

    return result;
}
