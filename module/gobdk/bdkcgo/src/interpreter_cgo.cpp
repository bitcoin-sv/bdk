#include <iostream>

#include <script/script_flags.h>
#include <core/interpreter_bdk.hpp>
#include <bdkcgo/interpreter_cgo.h>

uint32_t cgo_script_verification_flags_v1(const char* lScriptPtr, int lScriptLen, bool isChronicle)
{
    try {
        std::span<const uint8_t> lScript ;
        if (lScriptPtr != nullptr && lScriptLen > 0) {
            const uint8_t* pL = static_cast<const uint8_t*>(reinterpret_cast<const void*>(lScriptPtr));
            lScript = std::span<const uint8_t>(pL, lScriptLen);
        }
        return bsv::script_verification_flags_v1(lScript, isChronicle);
    } catch (const std::exception& e) {
        std::cout<< "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ << "    at " << __func__ <<std::endl<< e.what() <<std::endl ;
        return SCRIPT_FLAG_LAST+1;
    }
}

uint32_t cgo_script_verification_flags_v2(const char* lScriptPtr, int lScriptLen, int32_t blockHeight)
{
    try {
        std::span<const uint8_t> lScript ;
        if (lScriptPtr != nullptr && lScriptLen > 0) {
            const uint8_t* pL = static_cast<const uint8_t*>(reinterpret_cast<const void*>(lScriptPtr));
            lScript = std::span<const uint8_t>(pL, lScriptLen);
        }
        return bsv::script_verification_flags_v2(lScript, blockHeight);
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << std::endl << e.what() << std::endl;
        return SCRIPT_FLAG_LAST + 1;
    }
}

uint32_t cgo_script_verification_flags(const char* lScriptPtr, int lScriptLen, int32_t utxoHeight, int32_t blockHeight) {
    try {
        std::span<const uint8_t> lScript ;
        if (lScriptPtr != nullptr && lScriptLen > 0) {
            const uint8_t* pL = static_cast<const uint8_t*>(reinterpret_cast<const void*>(lScriptPtr));
            lScript = std::span<const uint8_t>(pL, lScriptLen);
        }
        return bsv::script_verification_flags(lScript, utxoHeight, blockHeight);
    }
    catch (const std::exception& e) {
        std::cout << "CGO EXCEPTION : " << __FILE__ << ":" << __LINE__ << "    at " << __func__ << std::endl << e.what() << std::endl;
        return SCRIPT_FLAG_LAST + 1;
    }
}

int cgo_execute_no_verify(const char* scriptPtr, int scriptLen, bool consensus, unsigned int flags)
{
    try {
        std::span<const uint8_t> script ;
        if (scriptPtr != nullptr && scriptLen > 0) {
            const uint8_t* pS = static_cast<const uint8_t*>(reinterpret_cast<const void*>(scriptPtr));
            script = std::span<const uint8_t>(pS, scriptLen);
        }
        return bsv::execute(script, consensus, flags);
    } catch (const std::exception& e) {
        std::cout<< "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ << "    at " << __func__ <<std::endl;
        return SCRIPT_ERR_ERROR_COUNT+1;
    }

}

int cgo_execute(const char* scriptPtr, int scriptLen,
                bool consensus,
                unsigned int flags,
                const char* txPtr, int txLen,
                int index,
                unsigned long long amount)
{
    try {
        std::span<const uint8_t> script ;
        if (scriptPtr != nullptr && scriptLen > 0) {
            const uint8_t* pS = static_cast<const uint8_t*>(reinterpret_cast<const void*>(scriptPtr));
            script = std::span<const uint8_t>(pS, scriptLen);
        }

        std::span<const uint8_t> tx;
        if (txPtr != nullptr && txLen > 0) {
            const uint8_t* pT = static_cast<const uint8_t*>(reinterpret_cast<const void*>(txPtr));
            tx = std::span<const uint8_t>(pT, txLen);
        }

        return bsv::execute(script, consensus, flags, tx, index, amount);
    } catch (const std::exception& e) {
        std::cout<< "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ << "    at " << __func__ <<std::endl;
        return SCRIPT_ERR_ERROR_COUNT+1;
    }
}


int cgo_verify(const char* uScriptPtr, int uScriptLen,
               const char* lScriptPtr, int lScriptLen,
               bool consensus,
               unsigned int flags,
               const char* txPtr, int txLen,
               int index,
               unsigned long long amount)
{
    try {
        std::span<const uint8_t> uScript ;
        if (uScriptPtr != nullptr && uScriptLen > 0) {
            const uint8_t* pU = static_cast<const uint8_t*>(reinterpret_cast<const void*>(uScriptPtr));
            uScript = std::span<const uint8_t>(pU, uScriptLen);
        }

        std::span<const uint8_t> lScript ;
        if (lScriptPtr != nullptr && lScriptLen > 0) {
            const uint8_t* pL = static_cast<const uint8_t*>(reinterpret_cast<const void*>(lScriptPtr));
            lScript = std::span<const uint8_t>(pL, lScriptLen);
        }

        const uint8_t* pT = static_cast<const uint8_t*>(reinterpret_cast<const void*>(txPtr));
        const std::span<const uint8_t> tx(pT, txLen);

        return bsv::verify(uScript, lScript, consensus, flags, tx, index, amount);
    } catch (const std::exception& e) {
        std::cout<< "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ << "    at " << __func__ <<std::endl;
        return SCRIPT_ERR_ERROR_COUNT+1;
    }
}

int cgo_verify_extend(const char* extendedTxPtr, int extendedTxLen, int32_t blockHeight, bool consensus) {
    try {
        std::span<const uint8_t> extendedTx ;
        if (extendedTxPtr != nullptr && extendedTxLen > 0) {
            const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
            extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
        }
        return bsv::verify_extend(extendedTx, blockHeight, consensus);
    } catch (const std::exception& e) {
        std::cout<< "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ << "    at " << __func__ <<std::endl;
        return SCRIPT_ERR_ERROR_COUNT+1;
    }
}

int cgo_verify_extend_full(const char* extendedTxPtr, int extendedTxLen, const int32_t* hUTXOsPtr, int hUTXOsLen, int32_t blockHeight, bool consensus) {
    try {
        std::span<const uint8_t> extendedTx ;
        if (extendedTxPtr != nullptr && extendedTxLen > 0) {
            const uint8_t* pTX = static_cast<const uint8_t*>(reinterpret_cast<const void*>(extendedTxPtr));
            extendedTx = std::span<const uint8_t>(pTX, extendedTxLen);
        }

        std::span<const int32_t> hUTXOs {hUTXOsPtr, (size_t)hUTXOsLen};

        return bsv::verify_extend_full(extendedTx, hUTXOs, blockHeight, consensus);
    } catch (const std::exception& e) {
        std::cout<< "CGO EXCEPTION : " <<__FILE__ <<":"<<__LINE__ << "    at " << __func__ <<std::endl;
        return SCRIPT_ERR_ERROR_COUNT+1;
    }
}