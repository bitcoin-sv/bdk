#ifndef __EXTENDED_TX_HPP__
#define __EXTENDED_TX_HPP__

#include <string>
#include <cstdint>
#include <span>
#include <vector>

#include <primitives/transaction.h>
#include <streams.h>
#include <serialize.h>
#include <version.h>

namespace bsv
{

// ConstGlueExtendedInput hold the list inputs and its utxo together
// to the single class in order to make it serializable
class ConstGlueExtendedInput {
public:
    const std::vector<CTxIn>& vin;    // The list of standard inputs
    const std::vector<CTxOut>& vutxo; // Extended part, list of prevous outputs data

    ConstGlueExtendedInput(const std::vector<CTxIn>& vi, const std::vector<CTxOut>& vu) : vin(vi), vutxo(vu) {
        if (vin.size() != vutxo.size()) {
            throw std::range_error("inconsistent size of vin and utxo");
        }
    }

    // This is the modified version of the method
    //    void Serialize_impl(Stream &os, const std::vector<T, A> &v, const V &)
    // in $BSV/src/serialize.h:793
    template <typename Stream> inline void Serialize(Stream& os) const {
        if (this->vin.size() != this->vutxo.size()) {
            throw std::range_error("inconsistent size of vin and utxo");
        }

        WriteCompactSize(os, this->vin.size());
        for (size_t i = 0; i < this->vin.size(); ++i) {
            ::Serialize(os, this->vin[i]);
            ::Serialize(os, this->vutxo[i]);
        }
    }
};

// GlueExtendedInput hold the list inputs and its utxo together
// to the single class in order to make it unserializable
class GlueExtendedInput {
public:
    std::vector<CTxIn>& vin;    // The list of standard inputs
    std::vector<CTxOut>& vutxo; // Extended part, list of prevous outputs data

    GlueExtendedInput(std::vector<CTxIn>& vi, std::vector<CTxOut>& vu) : vin(vi), vutxo(vu) {
        if (vin.size() != vutxo.size()) {
            throw std::range_error("inconsistent size of vin and utxo");
        }
    }

    // This is the modified version of the method
    //    void Unserialize_impl(Stream &is, std::vector<T, A> &v, const V &)
    // in $BSV/src/serialize.h:823
    template <typename Stream> inline void Unserialize(Stream& is) {
        this->vin.clear();
        this->vutxo.clear();
        size_t nSize = ReadCompactSize(is);
        size_t i = 0;
        size_t nMid = 0;
        size_t chunkSize = STARTING_CHUNK_SIZE;
        while (nMid < nSize) {
            nMid += std::min(nSize, size_t(1 + (chunkSize - 1) / (sizeof(CTxIn)+ sizeof(CTxOut)) ));
            chunkSize *= CHUNK_GROWTH_RATE;
            if (nMid > nSize) {
                nMid = nSize;
            }
            this->vin.resize(nMid);
            this->vutxo.resize(nMid);
            for (; i < nMid; i++) {
                ::Unserialize(is, this->vin[i]);
                ::Unserialize(is, this->vutxo[i]);
            }
        }
    }
};

// Implementation of extended transaction and its serialization/unserialization
// See
//     https://github.com/bitcoin-sv/arc/blob/main/doc/BIP-239.md
// As it is an "extended" transaction, we don't modify the BSV source code
// as the same memory layout in BIP-239, we separate the list of utxo.
// In the serialization/unresialization methods, we use the 'glue' to make it use
// of the DataStream selializer/unserializer
class CMutableTransactionExtended {
public:
    std::vector<CTxOut> vutxo;       // Extended part, list of prevous outputs data
    CMutableTransaction mtx;         // Standard part, a mutable transaction

    // See $BSV/src/primitives/transaction.h:256
    // Method inline void SerializeTransaction(const TxType &tx, Stream &s)
    template <typename Stream> inline void Serialize(Stream& os) const {
        // The 6 bytes marking the extended transactions (see BIP-239)
        const std::array<uint8_t, 6> efMarker{ 0x00, 0x00, 0x00, 0x00, 0x00, 0xEF };
        const ConstGlueExtendedInput glue(this->mtx.vin, this->vutxo);
        os << this->mtx.nVersion;
        os << efMarker;
        os << glue;
        os << this->mtx.vout;
        os << this->mtx.nLockTime;
    }

    // See $BSV/src/primitives/transaction.h:243
    // inline void UnserializeTransaction(TxType &tx, Stream &s)
    template <typename Stream> inline void Unserialize(Stream& is) {
        this->vutxo.clear();
        this->mtx.vin.clear();
        this->mtx.vout.clear();

        std::array<uint8_t, 6> efMarker; // The 6 bytes marking the extended transactions (see BIP-239)
        GlueExtendedInput glue(this->mtx.vin, this->vutxo);
        is >> this->mtx.nVersion;
        is >> efMarker;
        is >> glue;
        is >> this->mtx.vout;
        is >> this->mtx.nLockTime;
    }
};

} // namespace bsv

#endif /* __EXTENDED_TX_HPP__ */