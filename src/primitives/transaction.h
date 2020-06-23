// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCASH_PRIMITIVES_TRANSACTION_H
#define BITCASH_PRIMITIVES_TRANSACTION_H

#include <stdint.h>
#include <amount.h>
#include <script/script.h>
#include <serialize.h>
#include <uint256.h>
#include <pubkey.h>
#include <iostream>

static const int SERIALIZE_TRANSACTION_NO_WITNESS = 0x40000000;
extern bool userefline;
extern bool usenonprivacy;
extern bool usecurrency;
extern bool usemasterkeydummyonly;
extern bool usepriceranges;
extern bool usehashforcoinbase;

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    COutPoint(): n((uint32_t) -1) { }
    COutPoint(const uint256& hashIn, uint32_t nIn): hash(hashIn), n(nIn) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(n);
    }

    void SetNull() { hash.SetNull(); n = (uint32_t) -1; }
    bool IsNull() const { return (hash.IsNull() && n == (uint32_t) -1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        int cmp = a.hash.Compare(b.hash);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;
    CScriptWitness scriptWitness; //! Only serialized through CTransaction
    bool isnickname;
    bool isnonprivatenickname;
    bool nicknamehasviewkey;
    std::string nickname; 
    CPubKey address, viewpubkey;
    std::vector<unsigned char> nicknamesig;
    

    /* Setting nSequence to this value for every input in a transaction
     * disables nLockTime. */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /* If this flag set, CTxIn::nSequence is NOT interpreted as a
     * relative lock-time. */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1 << 31);

    /* If CTxIn::nSequence encodes a relative lock-time and this flag
     * is set, the relative lock-time has units of 512 seconds,
     * otherwise it specifies blocks with a granularity of 1. */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /* If CTxIn::nSequence encodes a relative lock-time, this mask is
     * applied to extract that lock-time from the sequence field. */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /* In order to use the same number of bits to encode roughly the
     * same wall-clock duration, and because blocks are naturally
     * limited to occur every 600s on average, the minimum granularity
     * for time-based relative lock-time is fixed at 512 seconds.
     * Converting from CTxIn::nSequence to seconds is performed by
     * multiplying by 512 = 2^9, or equivalently shifting up by
     * 9 bits. */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn()
    {
        prevout.SetNull();
        scriptSig = CScript();
        nSequence = SEQUENCE_FINAL;
        isnickname = false;
        isnonprivatenickname = false;
        nicknamehasviewkey = false;
        address = CPubKey();
        viewpubkey = CPubKey();
        nickname = "";
        nicknamesig.clear();
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);
    CTxIn(std::string nick, CPubKey addr, bool isnonprivate, bool hasviewkey, CPubKey viewkey);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {        
            READWRITE(isnickname);
            if (isnickname)
            {
                READWRITE(address);
                READWRITE(nickname);
                READWRITE(nicknamesig);
                if (usenonprivacy || (s.GetType() & SER_TXOUTALONE)) {
                    READWRITE(isnonprivatenickname);
                } else
                if (ser_action.ForRead()) {
                    isnonprivatenickname = false;
                }
                if (usemasterkeydummyonly || (s.GetType() & SER_TXOUTALONE)) {
                    READWRITE(nicknamehasviewkey);
                    READWRITE(viewpubkey);
                } else
                if (ser_action.ForRead()) {
                    nicknamehasviewkey = false;
                    viewpubkey = CPubKey();
                }
                prevout.SetNull();
                scriptSig = CScript();
                nSequence = SEQUENCE_FINAL;
            }else
            {
                READWRITE(prevout);
                READWRITE(scriptSig);
                READWRITE(nSequence);
                isnonprivatenickname = false;
                address = CPubKey();
                nickname = "";
                nicknamesig.clear();
            }            
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {        
        return (a.prevout     == b.prevout &&
                a.scriptSig   == b.scriptSig &&
                a.nSequence   == b.nSequence &&
                a.isnickname  == b.isnickname &&
                a.isnonprivatenickname  == b.isnonprivatenickname &&
                a.nicknamehasviewkey  == b.nicknamehasviewkey &&
                a.address     == b.address &&
                a.viewpubkey  == b.viewpubkey &&
                a.nickname    == b.nickname &&
                a.nicknamesig == b.nicknamesig);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    CAmount nValue;//Value in currency of transaction outputs
    CAmount nValueBitCash;//Value in currency of transaction inputs
    CScript scriptPubKey;

    std::string referenceline;
    char randomPrivatKey[32];
    CPubKey randomPubKey;
    unsigned char currency;//in which currency to store the coins 0=BitCash; 1=US Dollar
    bool isnonprivate;
    bool hasrecipientid, currencyisactive, masterkeyisremoved;
    unsigned char recipientid1;
    unsigned char recipientid2;

    CTxOut()
    {
        SetNull();
    }

//    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, std::string refererencelineIn, CPubKey senderPubKeyIn, CPubKey receiverPubKeyIn);
    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, unsigned char curr);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (usecurrency) { 
            if (!(s.GetType() & SER_GETHASH)) {
                READWRITE(nValue);
            }
        } else {
            READWRITE(nValue);
        }
        READWRITE(scriptPubKey);	        
        if (userefline || (s.GetType() & SER_TXOUTALONE)) {
            READWRITE(referenceline);
            READWRITE(randomPubKey);
            READWRITE(randomPrivatKey);
        }
        if (usenonprivacy || (s.GetType() & SER_TXOUTALONE)) {
            READWRITE(isnonprivate);
            READWRITE(recipientid1);
            READWRITE(recipientid2);
            hasrecipientid = true;
        } else 
        if (ser_action.ForRead()) {
            isnonprivate = false;
            recipientid1 = 0;
            recipientid2 = 0;
            hasrecipientid = false;
        }
        masterkeyisremoved = usemasterkeydummyonly;
        if (usecurrency || (s.GetType() & SER_TXOUTALONE)) {
            READWRITE(nValueBitCash);
            READWRITE(currency);
            currencyisactive = true;
        } else 
        if (ser_action.ForRead()) {
            currency = 0;
            nValueBitCash = nValue;
            currencyisactive = false;
        }
    }

    void SetNull()
    {
        nValue = -1;
	    nValueBitCash = -1;
        referenceline = "";
        scriptPubKey.clear();
        currency = 0;
        isnonprivate = false;
        hasrecipientid = false;
        currencyisactive = false;
        masterkeyisremoved = false;
        recipientid1 = 0;
        recipientid2 = 0;
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValueBitCash       == b.nValueBitCash &&
                a.scriptPubKey == b.scriptPubKey && 
                a.referenceline == b.referenceline/* &&
		a.randomPubKey == b.randomPubKey &&
		a.randomPrivatKey == b.randomPrivatKey*/&&
                a.currency == b.currency &&
                a.isnonprivate == b.isnonprivate &&
                a.hasrecipientid == b.hasrecipientid &&
                a.recipientid1 == b.recipientid1 &&
                a.recipientid2 == b.recipientid2);
    }


    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

struct CMutableTransaction;

/**
 * Basic transaction serialization format:
 * - int32_t nVersion
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - uint32_t nLockTime
 *
 * Extended transaction serialization format:
 * - int32_t nVersion
 * - unsigned char dummy = 0x00
 * - unsigned char flags (!= 0)
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - if (flags & 1):
 *   - CTxWitness wit;
 * - uint32_t nLockTime
 */
template<typename Stream, typename TxType>
inline void UnserializeTransaction(TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

    s >> tx.nVersion;
    unsigned char flags = 0;
    tx.vin.clear();
    tx.vout.clear();
    userefline = tx.nVersion >= 3;
    usenonprivacy = tx.nVersion >= 4;
    usecurrency = tx.nVersion >= 5;
    usemasterkeydummyonly = tx.nVersion >= 6;
    usepriceranges = tx.nVersion >= 7;
    usehashforcoinbase = tx.nVersion >= 8;
    /* Try to read the vin. In case the dummy is there, this will be read as an empty vector. */
    s >> tx.vin;
    if (tx.vin.size() == 0 && fAllowWitness) {
        /* We read a dummy or an empty vin. */
        s >> flags;
        if (flags != 0) {
            s >> tx.vin;
            s >> tx.vout;
        }
    } else {
        /* We read a non-empty vin. Assume a normal vout follows. */
        s >> tx.vout;
    }
    if ((flags & 1) && fAllowWitness) {
        /* The witness flag is present, and we support witnesses. */
        flags ^= 1;
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s >> tx.vin[i].scriptWitness.stack;
        }
    }
    if (flags) {
        /* Unknown flag in the serialization */
        throw std::ios_base::failure("Unknown transaction optional data");
    }
    s >> tx.nLockTime;

    if (usepriceranges) {
        s >> tx.haspricerange;
        if (tx.haspricerange) {
            s >> tx.minprice;
            s >> tx.maxprice;
        } else {
            tx.minprice = 0;
            tx.maxprice = 0;
        }
    } else {
        tx.haspricerange = false;
        tx.minprice = 0;
        tx.maxprice = 0;
    }

    if (usehashforcoinbase) {
        s >> tx.hashashinfo;
        if (tx.hashashinfo) {
            s >> tx.hashforpriceinfo;
        } else {
            tx.hashforpriceinfo = uint256S("0x0");
        }
    } else {
        tx.hashashinfo = false;
        tx.hashforpriceinfo = uint256S("0x0");
    }
}

template<typename Stream, typename TxType>
inline void SerializeTransaction(const TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

    s << tx.nVersion;
    userefline = tx.nVersion >= 3;
    usenonprivacy = tx.nVersion >= 4;
    usecurrency = tx.nVersion >= 5;
    usemasterkeydummyonly = tx.nVersion >= 6;
    usepriceranges = tx.nVersion >= 7;
    usehashforcoinbase = tx.nVersion >= 8;
    unsigned char flags = 0;
    // Consistency check
    if (fAllowWitness) {
        /* Check whether witnesses need to be serialized. */
        if (tx.HasWitness()) {
            flags |= 1;
        }
    }
    if (flags) {
        /* Use extended format in case witnesses are to be serialized. */
        std::vector<CTxIn> vinDummy;
        s << vinDummy;
        s << flags;
    }
    s << tx.vin;
    s << tx.vout;
    if (flags & 1) {
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s << tx.vin[i].scriptWitness.stack;
        }
    }
    s << tx.nLockTime;

    if (usepriceranges) {
        s << tx.haspricerange;
        if (tx.haspricerange) {
            s << tx.minprice;
            s << tx.maxprice;
        }
    }

    if (usehashforcoinbase) {
        s << tx.hashashinfo;
        if (tx.hashashinfo) {
            s << tx.hashforpriceinfo;
        }
    }

}


/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
public:
    // Default transaction version.
    static const int32_t OLD_VERSION = 8;
    static const int32_t CURRENT_VERSION = 9;

    // Changing the default transaction version requires a two step process: first
    // adapting relay policy by bumping MAX_STANDARD_VERSION, and then later date
    // bumping the default CURRENT_VERSION at which point both CURRENT_VERSION and
    // MAX_STANDARD_VERSION will be equal.
    static const int32_t MAX_STANDARD_VERSION = 9;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const int32_t nVersion;
    const uint32_t nLockTime;
    const unsigned char haspricerange;//0 = no price range 1 = check price to convert BitCash into Dollars; 2 = check price to convert Dollars into BitCash
    const CAmount minprice;
    const CAmount maxprice;
    const unsigned char hashashinfo;//0 = no hash 1 = has hash
    const uint256 hashforpriceinfo;


private:
    /** Memory only. */
    const uint256 hash;

    uint256 ComputeHash() const;

public:
    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    CTransaction(const CMutableTransaction &tx);
    CTransaction(CMutableTransaction &&tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }

    /** This deserializing constructor is provided instead of an Unserialize method.
     *  Unserialize is not possible, since it would require overwriting const fields. */
    template <typename Stream>
    CTransaction(deserialize_type, Stream& s) : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const {
        return hash;
    }

    // Compute a hash that includes both transaction and witness data
    uint256 GetWitnessHash() const;

    // Return sum of txouts.
    CAmount GetValueOut() const;
    CAmount GetValueOutInCurrency(unsigned char currency, CAmount price, CAmount pricegold) const;

    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    /**
     * Get the total transaction size in bytes, including witness data.
     * "Total Size" defined in BIP141 and BIP144.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const;

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull() && !vin[0].isnickname);
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    std::string ToString() const;

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }
};

/** A mutable version of CTransaction. */
struct CMutableTransaction
{
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    int32_t nVersion;
    uint32_t nLockTime;
    unsigned char haspricerange;//0 = no price range 1 = check price to convert BitCash into Dollars; 2 = check price to convert Dollars into BitCash
    CAmount minprice;
    CAmount maxprice;
    unsigned char hashashinfo;//0 = no hash 1 = has hash
    uint256 hashforpriceinfo;


    CMutableTransaction();
    CMutableTransaction(const CTransaction& tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }


    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeTransaction(*this, s);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    /** Compute the hash of this CMutableTransaction. This is computed on the
     * fly, as opposed to GetHash() in CTransaction, which uses a cached result.
     */
    uint256 GetHash() const;

    friend bool operator==(const CMutableTransaction& a, const CMutableTransaction& b)
    {
        return a.GetHash() == b.GetHash();
    }

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull() && !vin[0].isnickname);
    }

};

typedef std::shared_ptr<const CTransaction> CTransactionRef;
static inline CTransactionRef MakeTransactionRef() { return std::make_shared<const CTransaction>(); }
template <typename Tx> static inline CTransactionRef MakeTransactionRef(Tx&& txIn) { return std::make_shared<const CTransaction>(std::forward<Tx>(txIn)); }

#endif // BITCASH_PRIMITIVES_TRANSACTION_H
