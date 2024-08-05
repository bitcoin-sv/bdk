#ifndef JNI_CPPOBJ_HELPER_H
#define JNI_CPPOBJ_HELPER_H

#include <jni.h> // JNI header provided by JDK

// Helper allowing to hold cpp object inside java object
// Inspired from
//    https://thebreakfastpost.com/2012/01/26/wrapping-a-c-library-with-jni-part-2/


// Get the fieldID of a member variable from Java obj
inline jfieldID getHandleField(JNIEnv *env, jobject obj, const char* name)
{
    jclass c = env->GetObjectClass(obj);
    return env->GetFieldID(c, name, "J");
}

// Get the java object member variable as an C++ object
template <typename T>
T *getHandle(JNIEnv *env, jobject obj, const char* name)
{
    jlong handle = env->GetLongField(obj, getHandleField(env, obj, name));
    return reinterpret_cast<T *>(handle);
}

// Set an C++ object as a java object member variable
template <typename T>
void setHandle(JNIEnv *env, jobject obj, T *t, const char* name)
{
    jlong handle = reinterpret_cast<jlong>(t);
    env->SetLongField(obj, getHandleField(env, obj, name), handle);
}

#endif /* JNI_CPPOBJ_HELPER_H */