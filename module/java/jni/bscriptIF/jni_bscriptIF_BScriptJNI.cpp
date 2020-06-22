#include <jni.h>        // JNI header provided by JDK
#include <iostream>
#include <vector>
#include <jni_bscriptIF_BScriptJNI.h>   // Generated
#include <ScriptEngineIF.h>


JNIEXPORT jobject JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScript
  (JNIEnv * env, jobject obj, jbyteArray arr){
 
    int len = env->GetArrayLength(arr);
    std::vector<uint8_t> script(len);
    env->GetByteArrayRegion(arr,0,len,reinterpret_cast<jbyte*>(script.data()));

    //exeute the script. unhandled exceptions are treated as UNKNOWN_ERROR
    ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
    try {
        scriptResult = ScriptEngineIF::executeScript(script);
    } catch (std::exception &ex) {}

    //class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    // last arguments are method parameters
    jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));

    return result;
  }


JNIEXPORT jboolean JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScript
  (JNIEnv *, jobject, jbyteArray){
    
    return false;
  }



JNIEXPORT jobjectArray JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScriptString
  (JNIEnv * env, jobject obj, jobjectArray strScript){

    int stringCount = env->GetArrayLength(strScript);

    //class we want to call
    jclass clazz = env->FindClass("jni/bscriptIF/BScriptJNIResult");
    //execute each script and store the returned error in the obj array
    jobjectArray scriptResultArray = env->NewObjectArray(stringCount, clazz, nullptr);
    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID methodId = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");

    for (int i=0; i<stringCount; i++) {
        // get the script
        jstring strval = (jstring) (env->GetObjectArrayElement(strScript, i));
        const char *rawScript = env->GetStringUTFChars(strval, 0);

        //execute the script and add the result to the return array. Uncaught failures are treated as an UNKNOWN_ERROR
        ScriptError scriptResult = SCRIPT_ERR_UNKNOWN_ERROR;
        try {
            scriptResult = ScriptEngineIF::executeScript(std::string{rawScript});
        } catch(std::exception &ex){}

        jobject result = env->NewObject(clazz, methodId, scriptResult, env->NewStringUTF(ScriptErrorString(scriptResult)));
        //add the result of the execution to the return array
        env->SetObjectArrayElement(scriptResultArray, i, result);

        // Don't forget to call `ReleaseStringUTFChars` when you're done.
        env->ReleaseStringUTFChars(strval, rawScript);
    }

    return scriptResultArray;
  }


JNIEXPORT jboolean JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScriptString
  (JNIEnv *, jobject, jobjectArray){
    return false;
  }
