#include <jni.h>        // JNI header provided by JDK
#include <iostream>
#include <memory>
#include <jni_bscriptIF_BScriptJNI.h>   // Generated
#include <ScriptEngineIF.h>

JNIEXPORT jboolean JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScript
  (JNIEnv * env, jobject obj, jbyteArray arr){
 
    int len = env->GetArrayLength(arr);
    std::unique_ptr<unsigned char []> script(new unsigned char [len]);
    env->GetByteArrayRegion(arr,0,len,reinterpret_cast<jbyte*>(script.get()));
   
    bool retval = ScriptEngineIF::executeScript(script,len);
    return retval;
  
  }


JNIEXPORT jboolean JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScript
  (JNIEnv *, jobject, jbyteArray){
    
    return false;
  }



JNIEXPORT jboolean JNICALL Java_jni_bscriptIF_BScriptJNI_EvalScriptString
  (JNIEnv * env, jobject obj, jobjectArray strScript){
  
    bool retVal(false);
    int stringCount = env->GetArrayLength(strScript);
    for (int i=0; i<stringCount; i++) {
        jstring strval = (jstring) (env->GetObjectArrayElement(strScript, i));
        const char *rawScript = env->GetStringUTFChars(strval, 0);
        // Don't forget to call `ReleaseStringUTFChars` when you're done.
        retVal = ScriptEngineIF::executeScript(rawScript); 
        env->ReleaseStringUTFChars(strval, rawScript);
    }
    
    return retVal;
  }


JNIEXPORT jboolean JNICALL Java_jni_bscriptIF_BScriptJNI_VerifyScriptString
  (JNIEnv *, jobject, jobjectArray){
    return false;
  }
