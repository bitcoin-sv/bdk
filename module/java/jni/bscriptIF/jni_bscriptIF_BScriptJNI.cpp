#include <jni.h>        // JNI header provided by JDK
#include <iostream>
#include <vector>
#include <jni_bscriptIF_BScriptJNI.h>   // Generated
#include <ScriptEngineIF.h>
#include <memory>
#include <functional>

namespace {
    using unique_jstring_ptr = std::unique_ptr<char const[], std::function<void(char const *)>>;

    unique_jstring_ptr make_unique_jstring(jstring &str, JNIEnv *env) {
        const char *strValue = env->GetStringUTFChars(str, 0);

        if (strValue == NULL) {
            env->ThrowNew(env->FindClass("java/lang/OutOfMemoryError"), "Unable to read jstring input");
            return NULL;
        }

        return unique_jstring_ptr(strValue, [=](char const *p) mutable { env->ReleaseStringUTFChars(str, p); });
    }
}


JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScript
        (JNIEnv * env, jobject obj, jbyteArray arr,jboolean concensus, jint scriptflags,jstring hextx, jint nidx, jint amount){

    int len = env->GetArrayLength(arr);

    std::vector<uint8_t> script(len);
    env->GetByteArrayRegion(arr,0,len,reinterpret_cast<jbyte*>(script.data()));

    const unique_jstring_ptr hextxptr = make_unique_jstring(hextx, env);

    //class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck()){return NULL;}

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck()){return NULL;}

    //execute the script
    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = bsv::evaluate(script,concensus,scriptflags,hextxptr.get(),nidx,amount);
    } catch(const std::runtime_error &ex){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    } catch(const std::exception &ex) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return NULL;
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(scriptResult));
        return NULL;
    }


    // last arguments are method parameters
    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));

    return result;
}


JNIEXPORT jboolean JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScript
        (JNIEnv *, jobject, jbyteArray){

    return false;
}



JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScriptString
        (JNIEnv * env, jobject obj, jstring script, jboolean concensus, jint scriptflags, jstring hextx, jint nidx, jint amount){

    //tx and script data
    const unique_jstring_ptr rawScript = make_unique_jstring(script, env);
    const unique_jstring_ptr hextxptr = make_unique_jstring(hextx, env);

    //class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck()){return NULL;}

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck()){return NULL;}

    // run the script
    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = bsv::evaluate(std::string{rawScript.get()},concensus,scriptflags, hextxptr.get(),nidx,amount);
    } catch(const std::runtime_error &ex){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    } catch(const std::exception &ex) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return NULL;
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(scriptResult));
        return NULL;
    }

    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));

    return result;
}

JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScriptString
        (JNIEnv * env, jobject obj, jstring scriptsig, jstring scriptpubkey, jboolean concensus, jint scriptflags, jstring hextx, jint nidx, jint amount){

    //script / public key data
    const unique_jstring_ptr rawScriptSig = make_unique_jstring(scriptsig, env);
    const unique_jstring_ptr rawScriptpubkey = make_unique_jstring(scriptpubkey, env);
    const unique_jstring_ptr hextxptr = make_unique_jstring(hextx, env);

    //class we want to return
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck()){return NULL;}

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck()){return NULL;}

    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = bsv::verifyScript(std::string{rawScriptSig.get()}, std::string{rawScriptpubkey.get()},
                                                    concensus, scriptflags, std::string{hextxptr.get()}, nidx,
                                                    amount);
    } catch(const std::runtime_error &ex){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    } catch(const std::exception &ex) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return NULL;
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(scriptResult));
        return NULL;
    }

    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));

    return result;
}
