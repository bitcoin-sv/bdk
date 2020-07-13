//
// Created by m.fletcher on 10/07/2020.
//

#include <jni.h>
#include <com_nchain_bsv_scriptengine_Assembler.h> // Generated
#include "Assembler.h"
#include "jni_util.h"

using namespace bsv::jni;

JNIEXPORT jbyteArray JNICALL Java_com_nchain_bsv_scriptengine_Assembler_fromAsm
    (JNIEnv * env , jobject obj, jstring input_script_asm)
{
    if(input_script_asm == NULL){
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "value cannot be null");
        return NULL;
    }

    const unique_jstring_ptr input_script_asm_unique = make_unique_jstring(env, input_script_asm);

    CScript output_script;
    try
    {
        output_script = bsv::from_asm(input_script_asm_unique.get());
    }
    catch(const std::runtime_error& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
        return NULL;
    }
    catch(const std::exception& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return NULL;
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), "unable to convert to asm");
        return NULL;
    }

    jbyteArray converted_output_script = env->NewByteArray(output_script.size());
    env->SetByteArrayRegion(converted_output_script, 0, output_script.size(), reinterpret_cast<jbyte*>(output_script.data()));

    return converted_output_script;
}

JNIEXPORT jstring JNICALL Java_com_nchain_bsv_scriptengine_Assembler_toAsm
    (JNIEnv * env, jobject obj, jbyteArray input_script)
{
    if(input_script == NULL){
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "value cannot be null");
        return NULL;
    }

    int input_script_length = env->GetArrayLength(input_script);
    std::vector<uint8_t> converted_input_script(input_script_length);
    env->GetByteArrayRegion(input_script, 0, input_script_length, reinterpret_cast<jbyte*>(converted_input_script.data()));

    jstring asm_output_script = NULL;
    try
    {
        asm_output_script = env->NewStringUTF(bsv::to_asm(converted_input_script).c_str());
    }
    catch(const std::runtime_error& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ex.what());
    }
    catch(const std::exception& ex)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
    }
    catch(...)
    {
        env->ThrowNew(env->FindClass("java/lang/Exception"), "unable to convert to asm");
    }

    return asm_output_script;
}

