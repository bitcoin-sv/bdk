#include <jni.h>        // JNI header provided by JDK
#include <iostream>
#include <vector>
#include <jni_bscriptIF_BScriptJNI.h>   // Generated
#include <ScriptEngineIF.h>


JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScript
        (JNIEnv * env, jobject obj, jbyteArray arr,jboolean concensus, jint scriptflags,jstring hextx, jint nidx, jint amount){

    int len = env->GetArrayLength(arr);

    std::vector<uint8_t> script(len);
    env->GetByteArrayRegion(arr,0,len,reinterpret_cast<jbyte*>(script.data()));

    const char * hextxptr = env->GetStringUTFChars(hextx, NULL);
    // Can be NULL if out of memory
    if(hextxptr == NULL)
        return NULL;

    //execute the script
    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = ScriptEngineIF::executeScript(script,concensus,scriptflags,hextxptr,nidx,amount);
    } catch(std::runtime_error &ex){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(scriptResult));
        return NULL;
    }

    //release the memory
    env->ReleaseStringUTFChars(hextx, hextxptr);

    //class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck()){return NULL;}

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck()){return NULL;}

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

    //class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck()){return NULL;}

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck()){return NULL;}

    //tx and script data
    const char * rawScript = env->GetStringUTFChars(script, NULL);
    const char * hextxptr = env->GetStringUTFChars(hextx, NULL);

    //can be null if we've ran out of memory
    if(rawScript == NULL || hextxptr == NULL){
        return NULL;
    }

    // run the script
    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = ScriptEngineIF::executeScript(std::string{rawScript},concensus,scriptflags, hextxptr,nidx,amount);
    }catch(std::runtime_error &ex){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(scriptResult));
        return NULL;
    }

    //Not releasing UTFChars will cause memory leak
    env->ReleaseStringUTFChars(script, rawScript);
    env->ReleaseStringUTFChars(hextx, hextxptr);

    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));

    return result;
}


JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScriptString
        (JNIEnv * env, jobject obj, jstring scriptsig, jstring scriptpubkey, jboolean concensus, jint scriptflags, jstring hextx, jint nidx, jint amount){

    //class we want to return
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    if(env->ExceptionCheck()){return NULL;}

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck()){return NULL;}

    //script / public key data
    const char * rawScriptSig = env->GetStringUTFChars(scriptsig, NULL);
    const char * rawScriptpubkey = env->GetStringUTFChars(scriptpubkey, NULL);
    const char * hextxptr = env->GetStringUTFChars(hextx, NULL);

    //can be null if can't allocate memory
    if(rawScriptpubkey == NULL || rawScriptSig == NULL || hextxptr == NULL)
        return NULL;

    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = ScriptEngineIF::verifyScript(std::string{rawScriptSig}, std::string{rawScriptpubkey},
                                                    concensus, scriptflags, std::string{hextxptr}, nidx,
                                                    amount);
    } catch(std::runtime_error &err){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), err.what());
        return NULL;
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(scriptResult));
        return NULL;
    }

    env->ReleaseStringUTFChars(scriptsig, rawScriptSig);
    env->ReleaseStringUTFChars(scriptpubkey,rawScriptpubkey);
    env->ReleaseStringUTFChars(hextx, hextxptr);

    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));

    return result;
}
