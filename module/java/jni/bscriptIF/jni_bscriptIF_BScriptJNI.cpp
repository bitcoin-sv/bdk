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
    //exeute the script. unhandled exceptions are treated as UNKNOWN_ERROR
    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = ScriptEngineIF::executeScript(script,concensus,scriptflags,hextxptr,nidx,amount);
    } catch (std::exception &ex) {}

    //class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    // last arguments are method parameters
    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));

    if(hextxptr != NULL)
        env->ReleaseStringUTFChars(hextx, hextxptr);
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
    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    
    const char * rawScript = env->GetStringUTFChars(script, NULL);
    if(rawScript == NULL)
        return NULL;
    
    const char * hextxptr = env->GetStringUTFChars(hextx, NULL);
    
    
    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = ScriptEngineIF::executeScript(std::string{rawScript},concensus,scriptflags, hextxptr,nidx,amount); 
    }catch(std::exception &ex){}
    
    env->ReleaseStringUTFChars(script, rawScript);   
    if(hextxptr != NULL)
        env->ReleaseStringUTFChars(hextx, hextxptr);
    
    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));
    return result;
  }


JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScriptString
  (JNIEnv * env, jobject obj, jstring scriptsig, jstring scriptpubkey, jboolean concensus, jint scriptflags, jstring hextx, jint nidx, jint amount){
      //class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    
    const char * rawScriptSig = env->GetStringUTFChars(scriptsig, NULL);
    if(rawScriptSig == NULL)
        return NULL;
    
    const char * rawScriptpubkey = env->GetStringUTFChars(scriptpubkey, NULL);
    if(rawScriptpubkey == NULL)
        return NULL;
        
    const char * hextxptr = env->GetStringUTFChars(hextx, NULL);
    
    
    
    ScriptError scriptResult = ScriptEngineIF::verifyScript(std::string{rawScriptSig},std::string{rawScriptpubkey},concensus,scriptflags, std::string{hextxptr},nidx,amount); 
    env->ReleaseStringUTFChars(scriptsig, rawScriptSig);
    env->ReleaseStringUTFChars(scriptpubkey,rawScriptpubkey);
    
    if(hextxptr != NULL)
        env->ReleaseStringUTFChars(hextx, hextxptr);
        
    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));
    return result;
  }
