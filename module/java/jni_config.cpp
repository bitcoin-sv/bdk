#include <com_nchain_sesdk_Config.h> // Generated
#include <jni.h>

#include <config.h>// bsv code
#include <util.h>  // bsv code

#include "jni_memory.h"// string helper
#include "jni_cppobj_helper.h"

#include <stdexcept>
#include <string>

using namespace bsv::jni;

inline jfieldID getBooleanFieldID(JNIEnv *env, jobject obj, const char* name)
{
    assert(env);

    jclass c = env->GetObjectClass(obj);
    return env->GetFieldID(c, name, "Z");
}

JNIEXPORT void JNICALL Java_com_nchain_sesdk_Config_load(JNIEnv* env, jobject obj, jstring filename)
{
    assert(env);

    if(filename == nullptr)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::load() passed null filename");
        return;
    }

    const auto filename_str = make_unique_jstring(env, filename);
    const std::string fn(filename_str.get());
    gArgs.ReadConfigFile(fn);

    if (gArgs.IsArgSet("-maxopsperscriptpolicy"))
    {
        const uint64_t v = gArgs.GetArg("-maxopsperscriptpolicy",0);
        Java_com_nchain_sesdk_Config_setMaxOpsPerScriptPolicy(env, obj, static_cast<jlong>(v));
    }

    // Configure maximum length of numbers in scripts
    if (gArgs.IsArgSet("-maxscriptnumlengthpolicy"))
    {
        const int64_t value = gArgs.GetArgAsBytes("-maxscriptnumlengthpolicy", DEFAULT_SCRIPT_NUM_LENGTH_POLICY);
        Java_com_nchain_sesdk_Config_setMaxScriptnumLengthPolicy(env, obj, static_cast<jlong>(value));
    }

    //Configure max script size after genesis
    if (gArgs.IsArgSet("-maxscriptsizepolicy"))
    {
        const int64_t maxScriptSize = gArgs.GetArgAsBytes("-maxscriptsizepolicy", DEFAULT_MAX_SCRIPT_SIZE_POLICY_AFTER_GENESIS);
        Java_com_nchain_sesdk_Config_setMaxScriptSizePolicy(env, obj, static_cast<jlong>(maxScriptSize));
    }

    // Configure max number of public keys per MULTISIG operation
    if (gArgs.IsArgSet("-maxpubkeyspermultisigpolicy"))
    {
        const int64_t value = gArgs.GetArg("-maxpubkeyspermultisigpolicy", DEFAULT_PUBKEYS_PER_MULTISIG_POLICY_AFTER_GENESIS);
        Java_com_nchain_sesdk_Config_setMaxPubkeysPerMultisigPolicy(env, obj, static_cast<jlong>(value));
    }

    if (gArgs.IsArgSet("-maxstackmemoryusageconsensus") || gArgs.IsArgSet("-maxstackmemoryusagepolicy") )
    {
        const int64_t v1 = gArgs.GetArgAsBytes("-maxstackmemoryusageconsensus", 0);
        const int64_t v2 = gArgs.GetArgAsBytes("-maxstackmemoryusagepolicy", DEFAULT_STACK_MEMORY_USAGE_POLICY_AFTER_GENESIS);
        Java_com_nchain_sesdk_Config_setMaxStackMemoryUsage(env, obj, static_cast<jlong>(v1), static_cast<jlong>(v2));
    }

}


JNIEXPORT jlong JNICALL Java_com_nchain_sesdk_Config_getMaxOpsPerScript(JNIEnv* env, jobject obj)
{
    const jboolean isGenesisEnabled = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isGenesisEnabled"));
    const jboolean isConsensus = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isConsensus"));

    jlong r{0};
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj,"cppConfig"))
        r = static_cast<jlong>(pConfig->GetMaxOpsPerScript(static_cast<bool>(isGenesisEnabled), static_cast<bool>(isConsensus)));
    else
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::getMaxOpsPerScript unable to deference C++ pointer");

    return r;
}

//// Temporary dissable as it failed the build. The method has changed
// JNIEXPORT jlong JNICALL Java_com_nchain_sesdk_Config_getMaxScriptNumLength(JNIEnv* env, jobject obj)
// {
//     const jboolean isGenesisEnabled = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isGenesisEnabled"));
//     const jboolean isConsensus = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isConsensus"));

//     jlong r{0};
//     if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj,"cppConfig"))
//         r = static_cast<jlong>(pConfig->GetMaxScriptNumLength(static_cast<bool>(isGenesisEnabled), static_cast<bool>(isConsensus)));
//     else
//         env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::getMaxScriptNumLength unable to deference C++ pointer");

//     return r;
// }

JNIEXPORT jlong JNICALL Java_com_nchain_sesdk_Config_getMaxScriptSize(JNIEnv* env, jobject obj)
{
    const jboolean isGenesisEnabled = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isGenesisEnabled"));
    const jboolean isConsensus = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isConsensus"));

    jlong r{0};
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj,"cppConfig"))
        r = static_cast<jlong>(pConfig->GetMaxScriptSize(static_cast<bool>(isGenesisEnabled), static_cast<bool>(isConsensus)));
    else
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::getMaxScriptSize unable to deference C++ pointer");

    return r;
}

JNIEXPORT jlong JNICALL Java_com_nchain_sesdk_Config_getMaxPubKeysPerMultiSig(JNIEnv* env, jobject obj)
{
    const jboolean isGenesisEnabled = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isGenesisEnabled"));
    const jboolean isConsensus = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isConsensus"));

    jlong r{0};
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj,"cppConfig"))
        r = static_cast<jlong>(pConfig->GetMaxPubKeysPerMultiSig(static_cast<bool>(isGenesisEnabled), static_cast<bool>(isConsensus)));
    else
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::getMaxPubKeysPerMultiSig unable to deference C++ pointer");

    return r;
}

JNIEXPORT jlong JNICALL Java_com_nchain_sesdk_Config_getMaxStackMemoryUsage(JNIEnv* env, jobject obj)
{
    const jboolean isGenesisEnabled = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isGenesisEnabled"));
    const jboolean isConsensus = env->GetBooleanField(obj, getBooleanFieldID(env, obj, "isConsensus"));

    jlong r{0};
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj,"cppConfig"))
        r = static_cast<jlong>(pConfig->GetMaxStackMemoryUsage(static_cast<bool>(isGenesisEnabled), static_cast<bool>(isConsensus)));
    else
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::getMaxStackMemoryUsage unable to deference C++ pointer");

    return r;
}


JNIEXPORT void JNICALL Java_com_nchain_sesdk_Config_setMaxOpsPerScriptPolicy(JNIEnv* env, jobject obj, jlong v)
{
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj, "cppConfig"))
    {
        if(std::string err; !pConfig->SetMaxOpsPerScriptPolicy(static_cast<int64_t>(v), &err))
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), err.c_str());
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::setMaxOpsPerScriptPolicy unable to deference C++ pointer");
    }
}

JNIEXPORT void JNICALL Java_com_nchain_sesdk_Config_setMaxScriptnumLengthPolicy(JNIEnv* env, jobject obj, jlong v)
{
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj, "cppConfig"))
    {
        if (std::string err; !pConfig->SetMaxScriptNumLengthPolicy(static_cast<uint64_t>(v), &err))
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), err.c_str());
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::setMaxScriptnumLengthPolicy unable to deference C++ pointer");
    }
}

JNIEXPORT void JNICALL Java_com_nchain_sesdk_Config_setMaxScriptSizePolicy(JNIEnv* env, jobject obj, jlong v)
{
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj, "cppConfig"))
    {
        if (std::string err; !pConfig->SetMaxScriptSizePolicy(static_cast<uint64_t>(v), &err))
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), err.c_str());
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::setMaxScriptSizePolicy unable to deference C++ pointer");
    }
}

JNIEXPORT void JNICALL Java_com_nchain_sesdk_Config_setMaxPubkeysPerMultisigPolicy(JNIEnv* env, jobject obj, jlong v)
{
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj, "cppConfig"))
    {
        if (std::string err; !pConfig->SetMaxPubKeysPerMultiSigPolicy(static_cast<uint64_t>(v), &err))
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), err.c_str());
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::setMaxPubkeysPerMultisigPolicy unable to deference C++ pointer");
    }
}

JNIEXPORT void JNICALL Java_com_nchain_sesdk_Config_setMaxStackMemoryUsage(JNIEnv* env, jobject obj, jlong v1, jlong v2)
{
    if(GlobalConfig* pConfig = getHandle<GlobalConfig>(env, obj, "cppConfig"))
    {
        if (std::string err; !pConfig->SetMaxStackMemoryUsage(static_cast<uint64_t>(v1),static_cast<uint64_t>(v2),&err))
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), err.c_str());
    }
    else
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Config::setMaxStackMemoryUsage unable to deference C++ pointer");
    }
}


JNIEXPORT void JNICALL Java_com_nchain_sesdk_Config_initCppConfig(JNIEnv *env, jobject obj)
{
    GlobalConfig* cpp_config = new GlobalConfig();
    setHandle<GlobalConfig>(env, obj, cpp_config, "cppConfig");
}

JNIEXPORT void JNICALL Java_com_nchain_sesdk_Config_deleteCppConfig(JNIEnv *env, jobject obj)
{
    if(GlobalConfig* p = getHandle<GlobalConfig>(env,obj,"cppConfig"))
    {
        setHandle<GlobalConfig>(env, obj, nullptr, "cppConfig");
        delete p;
    }
}
