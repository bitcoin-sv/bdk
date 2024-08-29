#include <com_nchain_bdk_Stack.h> // Generated

#include "limitedstack.h"// bitcoin source code

#include "jni_cppobj_helper.h"



JNIEXPORT jlong JNICALL Java_com_nchain_bdk_Stack_size(JNIEnv *env, jobject obj)
{
    jlong r{0};
    if(LimitedStack* p = getHandle<LimitedStack>(env,obj,"cppStack"))
        r = (jlong) p->size();
    else
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Stack::size unable to deference C++ pointer");

    return r;
}

JNIEXPORT jbyteArray JNICALL Java_com_nchain_bdk_Stack_at(JNIEnv *env, jobject obj, jint i)
{
    jbyteArray ret{nullptr};
    if(LimitedStack* p = getHandle<LimitedStack>(env,obj,"cppStack"))
    {
        const LimitedVector& row = p->at((uint64_t) i);
        const std::vector<uint8_t>& row_array = row.GetElement();

        ret = env->NewByteArray((jsize)row_array.size());
        env->SetByteArrayRegion(ret, 0, row_array.size(), reinterpret_cast<const jbyte*>(row_array.data()));
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Stack::at unable to deference C++ pointer");
    }

    return ret;
}

JNIEXPORT void JNICALL Java_com_nchain_bdk_Stack_initCppStack(JNIEnv *env, jobject obj, jlong maxsize)
{
    LimitedStack* cpp_stack = new LimitedStack((uint64_t)maxsize);
    setHandle<LimitedStack>(env, obj, cpp_stack, "cppStack");
}

JNIEXPORT void JNICALL Java_com_nchain_bdk_Stack_deleteCppStack(JNIEnv *env, jobject obj)
{
    if(LimitedStack* p = getHandle<LimitedStack>(env,obj,"cppStack"))
    {
        setHandle<LimitedStack>(env, obj, nullptr, "cppStack");
        delete p;
    }
}
