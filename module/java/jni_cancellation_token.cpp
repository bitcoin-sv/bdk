#include <com_nchain_bdk_CancellationToken.h> // Generated

#include <jni_cppobj_helper.h>

#include <taskcancellation.h> // BSV code


using CCancellationSourcePtr = std::shared_ptr<task::CCancellationSource>;


JNIEXPORT void JNICALL Java_com_nchain_bdk_CancellationToken_cancel(JNIEnv *env, jobject obj)
{
    if(CCancellationSourcePtr* p_ptr = getHandle<CCancellationSourcePtr>(env,obj,"cppToken"))
    {
        CCancellationSourcePtr p = *(p_ptr);
        p->Cancel();
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "CancellationToken::cancel unable to deference C++ pointer");
    }
}

JNIEXPORT void JNICALL Java_com_nchain_bdk_CancellationToken_initCppToken(JNIEnv *env, jobject obj)
{
    CCancellationSourcePtr* p = new CCancellationSourcePtr(std::move(task::CCancellationSource::Make()));
    setHandle<CCancellationSourcePtr>(env, obj, p, "cppToken");
}

JNIEXPORT void JNICALL Java_com_nchain_bdk_CancellationToken_deleteCppToken(JNIEnv *env, jobject obj)
{
    if(CCancellationSourcePtr* p = getHandle<CCancellationSourcePtr>(env,obj,"cppToken"))
    {
        setHandle<CCancellationSourcePtr>(env, obj, nullptr, "cppToken");
        delete p;
    }
}
