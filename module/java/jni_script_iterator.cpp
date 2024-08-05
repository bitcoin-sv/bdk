#include <com_nchain_sesdk_ScriptIterator.h> // Generated
#include <jni.h>

#include <config.h>// bsv code
#include <util.h>  // bsv code

#include "jni_memory.h"// string helper

#include <stdexcept>
#include <string>

using namespace bsv::jni;

JNIEXPORT void JNICALL Java_com_nchain_sesdk_ScriptIterator_helpctor(JNIEnv* env, jobject obj, jbyteArray arr)
{
    if(arr == nullptr)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "ScriptIterator on null script");
        return;
    }

    // Set member scriptBinaryArray
    env->SetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "scriptBinaryArray", "[B"), static_cast<jobject>(arr));

    const jbyteArray byte_arr = static_cast<const jbyteArray>( env->GetObjectField(obj,  env->GetFieldID(env->GetObjectClass(obj), "scriptBinaryArray", "[B")) );
    const int len = env->GetArrayLength(byte_arr);
    std::vector<uint8_t> uint_arr(len);
    env->GetByteArrayRegion(byte_arr, 0, len, reinterpret_cast<jbyte*>(uint_arr.data()));

    // Try to evalulate script to get number of opcode and test if script is valid
    jint nb_op{0};
    try
    {
        CScript script {uint_arr.begin(), uint_arr.end()};
        CScript::const_iterator pc = script.begin();
        CScript::const_iterator pend = script.end();
        opcodetype opcode;
        valtype vchPushValue;
        while (pc < pend)
        {
            if (script.GetOp(pc, opcode, vchPushValue))
            {
                ++nb_op;
            }
            else
            {
                env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Error parsing CScript when constructing ScriptIterator");
                break;
            }
        }
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "ScriptIterator on invalid script");
    }

    env->SetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "numOpcodes", "I"), nb_op);
    env->SetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "posOpcode", "I"), -1);
    env->SetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "opcode", "I"), -1);
}

JNIEXPORT jboolean JNICALL Java_com_nchain_sesdk_ScriptIterator_next(JNIEnv* env, jobject obj)
{
    jboolean ret = true;

    const jint numOpcodes = env->GetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "numOpcodes", "I") );
    const jint old_pos_opcode = env->GetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "posOpcode", "I") );
    if(old_pos_opcode >= numOpcodes-1)
    {
        return false;
    }

    jint posOpcode{-1};
    const jbyteArray byte_arr = static_cast<const jbyteArray>( env->GetObjectField(obj,  env->GetFieldID(env->GetObjectClass(obj), "scriptBinaryArray", "[B")) );
    const int len = env->GetArrayLength(byte_arr);
    std::vector<uint8_t> uint_arr(len);
    env->GetByteArrayRegion(byte_arr, 0, len, reinterpret_cast<jbyte*>(uint_arr.data()));
    CScript script {uint_arr.begin(), uint_arr.end()};
    CScript::const_iterator pc = script.begin();
    CScript::const_iterator pend = script.end();
    opcodetype opcode;
    valtype vchPushValue;
    while (pc < pend && posOpcode<=old_pos_opcode)
    {
        if (script.GetOp(pc, opcode, vchPushValue))
        {
            ++posOpcode;
        }
        else
        {
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Error parsing CScript when calling ScriptIterator::next()");
            break;
        }
    }

    if(vchPushValue.size()>0)
    {
        jbyteArray data_array = env->NewByteArray(vchPushValue.size());
        env->SetByteArrayRegion(data_array, 0, vchPushValue.size(), reinterpret_cast<jbyte*>(vchPushValue.data()));
        env->SetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "pushData", "[B"), static_cast<jobject>(data_array));
    }
    else
    {
        env->SetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "pushData", "[B"), nullptr);
    }

    const jint op_code = static_cast<jint>(opcode);
    env->SetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "posOpcode", "I"), posOpcode);
    env->SetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "opcode", "I"), op_code);

    return ret;
}

JNIEXPORT jboolean JNICALL Java_com_nchain_sesdk_ScriptIterator_reset(JNIEnv* env, jobject obj)
{
    env->SetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "posOpcode", "I"), -1);
    env->SetIntField(obj, env->GetFieldID(env->GetObjectClass(obj), "opcode", "I"), -1);
    return true;
}
