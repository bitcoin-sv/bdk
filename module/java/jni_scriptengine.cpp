#include <com_nchain_bdk_ScriptEngine.h> // Generated

#include "jni.h" // JNI header provided by JDK

#include "jni_memory.h"
#include "jni_cppobj_helper.h"

#include <ecc_guard.h> // sdk core
#include <interpreter_bdk.hpp>

#include <base58.h>                 // bsv src
#include <chainparams.h>            // bsv src
#include <config.h>                 // bsv src
#include <core_io.h>                // bsv src
#include <ecc_guard.h>              // bsv src
#include <script/interpreter.h>     // bsv src

#include <algorithm>
#include <stdexcept>
#include <vector>

using namespace std;
using namespace bsv::jni;

// Helper to create java Stack object from c++ LimitedStack*
jobject createNewJavaStackObject(JNIEnv* env, LimitedStack* cppStack)
{
    jobject jStack{};
    jclass clazzStack = env->FindClass("com/nchain/bdk/Stack");
    jmethodID methodIDStackCtor = env->GetMethodID(clazzStack, "<init>", "(J)V");
    jStack = env->NewObject(clazzStack, methodIDStackCtor, (jlong) cppStack->getCombinedStackSize());

    jmethodID methodIDStackClose = env->GetMethodID(clazzStack, "close", "()V");
    env->CallVoidMethod(jStack, methodIDStackClose);
    setHandle<LimitedStack>(env, jStack, cppStack, "cppStack");
    return jStack;
}

int get_flags_member(JNIEnv* env, jobject obj)
{
    jclass clazzSE = env->GetObjectClass(obj);
    jfieldID fieldIDIFlags =  env->GetFieldID(clazzSE, "flags", "I");
    return static_cast<int>(env->GetIntField(obj, fieldIDIFlags));
}

bool get_consensus(JNIEnv* env, jobject obj)
{
    jclass clazzSE = env->GetObjectClass(obj);
    jfieldID fieldIDIConfig =  env->GetFieldID(clazzSE, "config", "Lcom/nchain/bdk/Config;");

    jobject objectConfig = env->GetObjectField(obj, fieldIDIConfig);
    jclass clazzConfig = env->GetObjectClass(objectConfig);
    jfieldID fieldIDIConsensus = env->GetFieldID(clazzConfig, "isConsensus", "Z");
    const jboolean isConsensus = env->GetBooleanField(objectConfig, fieldIDIConsensus);
    return static_cast<bool>(isConsensus);
}

LimitedStack* get_stack_member(JNIEnv* env, jobject obj, const char* member_name)
{
    jclass clazzSE = env->GetObjectClass(obj);
    jfieldID fieldIDStack =  env->GetFieldID(clazzSE, member_name, "Lcom/nchain/bdk/Stack;");

    jobject objectStack = env->GetObjectField(obj, fieldIDStack);
    if(LimitedStack* p = getHandle<LimitedStack>(env, objectStack, "cppStack"))
        return p;
    else
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "ScriptEngine unable to deference C++ pointer to Stack member");

    return nullptr;
}

GlobalConfig* get_config_member(JNIEnv* env, jobject obj)
{
    jclass clazzSE = env->GetObjectClass(obj);
    jfieldID fieldIDConfig =  env->GetFieldID(clazzSE, "config", "Lcom/nchain/bdk/Config;");

    jobject objectConfig = env->GetObjectField(obj, fieldIDConfig);
    if(GlobalConfig* p = getHandle<GlobalConfig>(env, objectConfig, "cppConfig"))
        return p;
    else
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "ScriptEngine unable to deference C++ pointer to Config member");

    return nullptr;
}

std::shared_ptr<task::CCancellationSource>* get_cpp_cancellation_source(JNIEnv* env, jobject token_obj)
{
    if(std::shared_ptr<task::CCancellationSource>* p = getHandle<std::shared_ptr<task::CCancellationSource>>(env, token_obj, "cppToken"))
        return p;
    else
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "ScriptEngine unable to deference C++ pointer to CancellationToken");

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

JNIEXPORT jobject JNICALL
Java_com_nchain_bdk_ScriptEngine_execute___3BLcom_nchain_bdk_CancellationToken_2Ljava_lang_String_2IJ(
    JNIEnv* env,
    jobject obj,
    jbyteArray arr,
    jobject token,
    jstring hextx,
    jint idx,
    jlong amount)
{
    if(arr == nullptr || hextx == nullptr)
    {
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "value cannot be null");
        return nullptr;
    }

    const int scriptflags = get_flags_member(env, obj);
    const bool concensus = get_consensus(env, obj);

    const auto hextxptr = make_unique_jstring(env, hextx);

    int len = env->GetArrayLength(arr);

    std::vector<uint8_t> script_binary(len);
    env->GetByteArrayRegion(arr, 0, len, reinterpret_cast<jbyte*>(script_binary.data()));

    // class we want to call
    jclass clazz = env->FindClass("com/nchain/bdk/Status");
    if(env->ExceptionCheck())
    {
        return nullptr;
    }

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID method_id = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck())
    {
        return nullptr;
    }

    // execute the script
    ScriptError script_result = SCRIPT_ERR_UNKNOWN_ERROR;
    try
    {
        CScript script{script_binary.begin(), script_binary.end()};
        const std::string hextx_str (hextxptr.get());
        CMutableTransaction mtx;

        if(!hextx_str.empty() && hextx_str.find_first_not_of(" /n/t/f") != std::string::npos)
        {
            if(!DecodeHexTx(mtx, hextx_str))
            {
                env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Unable to create a CMutableTransaction from supplied transaction hex");
                return nullptr;
            }
        }

        std::unique_ptr<BaseSignatureChecker> sig_check = std::make_unique<BaseSignatureChecker>();
        CTransaction tx(mtx);
        if(!mtx.vin.empty() && !mtx.vout.empty())
        {
            sig_check = std::make_unique<TransactionSignatureChecker>(&tx, idx, Amount{amount});
        }

        auto psource = get_cpp_cancellation_source(env, token);
        auto pconfig = get_config_member(env, obj);
        if(!psource || !pconfig)
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Unable to get C++ Cancellation Source or Config");

        auto& source = *psource;
        GlobalConfig& config = *pconfig;

        LimitedStack* pStack = (get_stack_member(env, obj, "stack"));
        LimitedStack* pAltStack = (get_stack_member(env, obj, "altstack"));

        std::vector<bool>* pvfExec = getHandle< std::vector<bool> >(env,obj,"cppExecState");
        std::vector<bool>* pvfElse = getHandle< std::vector<bool> >(env,obj,"cppElseState");

        if(pStack!=nullptr && pAltStack!=nullptr && pvfExec!=nullptr && pvfElse!=nullptr)
        {
            LimitedStack& stack = *pStack;
            LimitedStack& altstack = *pAltStack;
            long ipc{0};
            std::vector<bool>& vfExec = *pvfExec;
            std::vector<bool>& vfElse = *pvfElse;

            EvalScript(config, concensus, source->GetToken(), stack, script, scriptflags, *sig_check.get(), altstack, ipc, vfExec, vfElse, &script_result);
        }
    }
    catch(const std::runtime_error& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return nullptr;
    }
    catch(const std::exception& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return nullptr;
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(script_result));
        return nullptr;
    }

    // last arguments are method parameters
    jobject result = env->NewObject(clazz, method_id, script_result, env->NewStringUTF(ScriptErrorString(script_result)));

    return result;
}

JNIEXPORT jobject JNICALL Java_com_nchain_bdk_ScriptEngine_execute__Ljava_lang_String_2Lcom_nchain_bdk_CancellationToken_2Ljava_lang_String_2IJ(
                                                                         JNIEnv* env,
                                                                         jobject obj,
                                                                         jstring script_jstr,
                                                                         jobject token,
                                                                         jstring hextx,
                                                                         jint idx,
                                                                         jlong amount)
{
    if(script_jstr == nullptr || hextx == nullptr)
    {
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "value cannot be null");
        return nullptr;
    }

    // tx and script data
    const auto raw_script = make_unique_jstring(env, script_jstr);
    const auto hextxptr = make_unique_jstring(env, hextx);

    const std::string script_str{raw_script.get()};
    if(script_str.empty())
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "empty script");
        return nullptr;
    }

    if(script_str.find_first_not_of(" /n/t/f") == std::string::npos)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Control character in script");
        return nullptr;
    }

    CScript script;
    try{
        script = ParseScript(script_str);
    }
    catch(std::exception& e){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    }

    jbyteArray script_binary = env->NewByteArray(script.size());
    env->SetByteArrayRegion(script_binary, 0, script.size(), reinterpret_cast<const jbyte*>(script.data()));

    return Java_com_nchain_bdk_ScriptEngine_execute___3BLcom_nchain_bdk_CancellationToken_2Ljava_lang_String_2IJ(
        env, obj, script_binary, token, hextx, idx, amount
    );
}



JNIEXPORT jbooleanArray JNICALL Java_com_nchain_bdk_ScriptEngine_getExecState(JNIEnv* env, jobject obj)
{
    jbooleanArray ret{nullptr};
    if(std::vector<bool>* p = getHandle< std::vector<bool> >(env,obj,"cppExecState"))
    {
        std::vector<bool>& bool_array = *p;

        ret = env->NewBooleanArray((jsize)bool_array.size());
        jboolean* arr = env->GetBooleanArrayElements(ret, nullptr);

        copy(begin(bool_array), end(bool_array), arr);

        env->ReleaseBooleanArrayElements(ret, arr, 0);
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "ScriptEngine::getVfExec unable to deference C++  pointer");
    }

    return ret;
}

JNIEXPORT jbooleanArray JNICALL Java_com_nchain_bdk_ScriptEngine_getElseState(JNIEnv* env, jobject obj)
{
    jbooleanArray ret{nullptr};
    if(std::vector<bool>* p = getHandle< std::vector<bool> >(env,obj,"cppElseState"))
    {
        std::vector<bool>& bool_array = *p;

        ret = env->NewBooleanArray((jsize)bool_array.size());
        jboolean* arr = env->GetBooleanArrayElements(ret, nullptr);

        copy(begin(bool_array), end(bool_array), arr);

        env->ReleaseBooleanArrayElements(ret, arr, 0);
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "ScriptEngine::cppElseState unable to deference C++  pointer");
    }

    return ret;

}

JNIEXPORT void JNICALL Java_com_nchain_bdk_ScriptEngine_initAltStack(JNIEnv* env, jobject obj)
{
    LimitedStack* stack_ptr = get_stack_member(env, obj, "stack");
    LimitedStack* altstack_ptr = new LimitedStack(stack_ptr->makeChildStack());
    jobject jAltStack = createNewJavaStackObject(env, altstack_ptr);

    jclass clazzScriptEngine = env->GetObjectClass(obj);
    const jfieldID altstackFieldID = env->GetFieldID(clazzScriptEngine, "altstack", "Lcom/nchain/bdk/Stack;");
    env->SetObjectField(obj, altstackFieldID, jAltStack);
}


JNIEXPORT void JNICALL Java_com_nchain_bdk_ScriptEngine_initBranchStates(JNIEnv* env, jobject obj)
{
    std::vector<bool>* cpp_vfExec = new std::vector<bool>();
    setHandle<std::vector<bool>>(env, obj, cpp_vfExec, "cppExecState");

    std::vector<bool>* cpp_vfElse = new std::vector<bool>();
    setHandle<std::vector<bool>>(env, obj, cpp_vfElse, "cppElseState");
}

//JNIEXPORT void JNICALL Java_com_nchain_bdk_ScriptEngine_clearBranchStates(JNIEnv* env, jobject obj)
//{
//    if(std::vector<bool>* pvfExec = getHandle< std::vector<bool> >(env,obj,"cppExecState"))
//        pvfExec->clear();
//
//    if(std::vector<bool>* pvfElse = getHandle< std::vector<bool> >(env,obj,"cppElseState"))
//        pvfElse->clear();
//}

JNIEXPORT void JNICALL Java_com_nchain_bdk_ScriptEngine_deleteBranchStates(JNIEnv* env, jobject obj)
{
    if(std::vector<bool>* p = getHandle< std::vector<bool> >(env,obj,"cppExecState"))
    {
        setHandle< std::vector<bool> >(env, obj, nullptr, "cppExecState");
        delete p;
    }

    if(std::vector<bool>* p = getHandle< std::vector<bool> >(env,obj,"cppElseState"))
    {
        setHandle< std::vector<bool> >(env, obj, nullptr, "cppElseState");
        delete p;
    }
}

JNIEXPORT jobject JNICALL Java_com_nchain_bdk_ScriptEngine_execute___3BLcom_nchain_bdk_CancellationToken_2_3BIJ(JNIEnv* env,
                                                                                                                    jobject obj,
                                                                                                                    jbyteArray binary_script,
                                                                                                                    jobject token,
                                                                                                                    jbyteArray rawtx,
                                                                                                                    jint idx,
                                                                                                                    jlong amount)
{
    if(binary_script == nullptr || rawtx == nullptr)
    {
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "value cannot be null");
        return nullptr;
    }

    // get script
    int scriptLen = env->GetArrayLength(binary_script);
    std::vector<uint8_t> scriptArray(scriptLen);
    env->GetByteArrayRegion(binary_script, 0, scriptLen, reinterpret_cast<jbyte*>(scriptArray.data()));

    // get tx
    int rawtxLen = env->GetArrayLength(rawtx);
    std::vector<uint8_t> txArray(rawtxLen);
    env->GetByteArrayRegion(rawtx, 0, rawtxLen, reinterpret_cast<jbyte*>(txArray.data()));

    // class we want to return
    jclass clazz = env->FindClass("com/nchain/bdk/Status");
    if(env->ExceptionCheck())
    {
        return nullptr;
    }

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID method_id = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck())
    {
        return nullptr;
    }

    // get script configurations
    const int scriptflags = get_flags_member(env, obj);
    const bool consensus = get_consensus(env, obj);

    ScriptError script_result = SCRIPT_ERR_UNKNOWN_ERROR;
    try
    {
        script_result = bsv::execute(scriptArray, consensus, scriptflags, txArray, idx, amount);
    }
    catch(const std::runtime_error& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return nullptr;
    }
    catch(const std::exception& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return nullptr;
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(script_result));
        return nullptr;
    }

    jobject result =
        env->NewObject(clazz, method_id, script_result, env->NewStringUTF(ScriptErrorString(script_result)));

    return result;
}

JNIEXPORT jobject JNICALL Java_com_nchain_bdk_ScriptEngine_verify(JNIEnv* env,
                                                                    jobject obj,
                                                                    jbyteArray scriptSig,
                                                                    jbyteArray scriptPub,
                                                                    jboolean concensus,
                                                                    jint scriptflags,
                                                                    jbyteArray hextx,
                                                                    jint nidx,
                                                                    jlong amount)
{

    if(scriptSig == nullptr || scriptPub == nullptr || hextx == nullptr)
    {
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "value cannot be null");
        return nullptr;
    }

    // get script sig
    int scriptSigLen = env->GetArrayLength(scriptSig);
    std::vector<uint8_t> scriptSigScript(scriptSigLen);
    env->GetByteArrayRegion(scriptSig, 0, scriptSigLen, reinterpret_cast<jbyte*>(scriptSigScript.data()));

    // get scriptpub
    int scriptPubLen = env->GetArrayLength(scriptPub);
    std::vector<uint8_t> scriptPubScript(scriptPubLen);
    env->GetByteArrayRegion(scriptPub, 0, scriptPubLen, reinterpret_cast<jbyte*>(scriptPubScript.data()));

    // get hextx
    int txHexLen = env->GetArrayLength(hextx);
    std::vector<uint8_t> txHex(txHexLen);
    env->GetByteArrayRegion(hextx, 0, txHexLen, reinterpret_cast<jbyte*>(txHex.data()));

    // class we want to return
    jclass clazz = env->FindClass("com/nchain/bdk/Status");
    if(env->ExceptionCheck())
    {
        return nullptr;
    }

    //<init> = constructor, <(ZL...) method signature. See JNI documentation for symbol definitions
    jmethodID method_id = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if(env->ExceptionCheck())
    {
        return nullptr;
    }

    ScriptError script_result = SCRIPT_ERR_UNKNOWN_ERROR;
    try
    {
        script_result =
            bsv::verify(scriptSigScript, scriptPubScript, concensus, scriptflags, txHex, nidx, amount);
    }
    catch(const std::runtime_error& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return nullptr;
    }
    catch(const std::exception& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return nullptr;
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ScriptErrorString(script_result));
        return nullptr;
    }

    jobject result =
        env->NewObject(clazz, method_id, script_result, env->NewStringUTF(ScriptErrorString(script_result)));

    return result;
}
