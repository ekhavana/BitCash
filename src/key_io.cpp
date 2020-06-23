// Copyright (c) 2014-2016 The Bitcash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>

#include <base58.h>
#include <bech32.h>
#include <script/script.h>
#include <utilstrencodings.h>
#include <wallet/wallet.h>
#include <interfaces/wallet.h>
#include <nicknames.h>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <assert.h>
#include <string.h>
#include <algorithm>

namespace
{

class DestinationEncoder : public boost::static_visitor<std::string>
{
private:
    const CChainParams& m_params;

public:
    DestinationEncoder(const CChainParams& params) : m_params(params) {}

    std::string operator()(const CKeyID& id) const
    {
        std::vector<unsigned char> data;
        if (id.hasviewkey) {
            data = m_params.Base58Prefix(CChainParams::PUBKEY_ADDRESSVIEWKEY);
        } else {
        if (id.nonprivate) {
           data = m_params.Base58Prefix(CChainParams::PUBKEY_ADDRESSNONPRIVATE);
        } else 
           data = m_params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        }
//        data.insert(data.end(), id.begin(), id.end());
        data.insert(data.end(), id.recokey.begin(), id.recokey.end());
        if (id.hasviewkey) {
            data.insert(data.end(), id.viewkey.begin(), id.viewkey.end());
        }
        return EncodeBase58Check(data);
    }

    std::string operator()(const CScriptID& id) const
    {
        std::vector<unsigned char> data = m_params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
//        data.insert(data.end(), id.begin(), id.end());
        //data.insert(data.end(), &id.recokey[0], &id.recokey[33]);
        data.insert(data.end(), id.recokey.begin(), id.recokey.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const WitnessV0KeyHash& id) const
    {
        std::vector<unsigned char> data = {0};
        data.reserve(33);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
        return bech32::Encode(m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessV0ScriptHash& id) const
    {
        std::vector<unsigned char> data = {0};
        data.reserve(53);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
        return bech32::Encode(m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessUnknown& id) const
    {
        if (id.version < 1 || id.version > 16 || id.length < 2 || id.length > 40) {
            return {};
        }
        std::vector<unsigned char> data = {(unsigned char)id.version};
        data.reserve(1 + (id.length * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.program, id.program + id.length);
        return bech32::Encode(m_params.Bech32HRP(), data);
    }

    std::string operator()(const CNoDestination& no) const { return {}; }
};

CTxDestination DecodeDestinationNoNickname(const std::string& str, const CChainParams& params)
{
    std::vector<unsigned char> data;
    uint160 hash;
    std::vector<unsigned char> temprecokey;

    if (DecodeBase58Check(str, data)) {
        // base58-encoded Bitcash addresses.
        // Public-key-hash-addresses have version 0 (or 111 testnet).
        // The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
        const std::vector<unsigned char>& pubkey_prefix = params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        const std::vector<unsigned char>& pubkey_prefixtrezor = params.Base58Prefix(CChainParams::PUBKEY_ADDRESSTREZOR);
        const std::vector<unsigned char>& pubkey_prefixnonprivate = params.Base58Prefix(CChainParams::PUBKEY_ADDRESSNONPRIVATE);
        if (data.size() == 33 + /*hash.size() +*/ pubkey_prefix.size() && (std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin()) || std::equal(pubkey_prefixtrezor.begin(), pubkey_prefixtrezor.end(), data.begin()) || std::equal(pubkey_prefixnonprivate.begin(), pubkey_prefixnonprivate.end(), data.begin()))) {
            temprecokey.resize(33);
            std::copy(data.begin() + pubkey_prefix.size(), data.begin() + pubkey_prefix.size()+33, temprecokey.begin());
            CPubKey pkey;
            pkey.Set(temprecokey.begin(),temprecokey.end());
            hash=pkey.GetID();
            CKeyID key;
            key=CKeyID(hash);      
            key.recokey.resize(33);
            key.nonprivate = std::equal(pubkey_prefixnonprivate.begin(), pubkey_prefixnonprivate.end(), data.begin());
//            LogPrintf("key.nonprivate %d\n",*(unsigned char*)&key.nonprivate);
            std::copy(data.begin() + pubkey_prefix.size(), data.begin() + pubkey_prefix.size()+33, key.recokey.begin());
            return key;
        }
        //Address with viewkey / 2 pubkeys
        const std::vector<unsigned char>& pubkey_prefixviewkey = params.Base58Prefix(CChainParams::PUBKEY_ADDRESSVIEWKEY);
        if (data.size() == 2* 33 + pubkey_prefixviewkey.size() && (std::equal(pubkey_prefixviewkey.begin(), pubkey_prefixviewkey.end(), data.begin()))) {
            temprecokey.resize(33);
            std::copy(data.begin() + pubkey_prefix.size(), data.begin() + pubkey_prefix.size()+33, temprecokey.begin());
            CPubKey pkey;
            pkey.Set(temprecokey.begin(),temprecokey.end());
            hash = pkey.GetID();
            CKeyID key;
            key = CKeyID(hash);      
            key.nonprivate = false;
            key.hasviewkey = true;
            key.recokey.resize(33);
            std::copy(data.begin() + pubkey_prefixviewkey.size(), data.begin() + pubkey_prefixviewkey.size() + 33, key.recokey.begin());
            key.viewkey.resize(33);
            std::copy(data.begin() + pubkey_prefixviewkey.size() + 33, data.begin() + pubkey_prefixviewkey.size()+ 2 * 33, key.viewkey.begin());
            return key;
        }
        // Script-hash-addresses have version 5 (or 196 testnet).
        // The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
        const std::vector<unsigned char>& script_prefix = params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        const std::vector<unsigned char>& script_prefixtrezor = params.Base58Prefix(CChainParams::SCRIPT_ADDRESSTREZOR);
        if (data.size() == 33 + /*hash.size() +*/ script_prefix.size() && (std::equal(script_prefix.begin(), script_prefix.end(), data.begin()) || std::equal(script_prefixtrezor.begin(), script_prefixtrezor.end(), data.begin()))) {
            temprecokey.resize(33);
            std::copy(data.begin() + pubkey_prefix.size(), data.begin() + pubkey_prefix.size()+33, temprecokey.begin());
            CPubKey pkey;
            pkey.Set(temprecokey.begin(),temprecokey.end());
            hash=pkey.GetID();
            CKeyID key;
            key=CKeyID(hash);                      
            key.recokey.resize(33);                
            std::copy(data.begin() + pubkey_prefix.size(), data.begin() + pubkey_prefix.size()+33, key.recokey.begin());
            return key;

        }
    }
    data.clear();
    auto bech = bech32::Decode(str);
    if (bech.second.size() > 0 && bech.first == params.Bech32HRP()) {
        // Bech32 decoding
        int version = bech.second[0]; // The first 5 bit symbol is the witness version (0-16)
        // The rest of the symbols are converted witness program bytes.
        data.reserve(((bech.second.size() - 1) * 5) / 8);
        if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, bech.second.begin() + 1, bech.second.end())) {
            if (version == 0) {
                {
                    WitnessV0KeyHash keyid;
                    if (data.size() == keyid.size()) {
                        std::copy(data.begin(), data.end(), keyid.begin());
                        return keyid;
                    }
                }
                {
                    WitnessV0ScriptHash scriptid;
                    if (data.size() == scriptid.size()) {
                        std::copy(data.begin(), data.end(), scriptid.begin());
                        return scriptid;
                    }
                }
                return CNoDestination();
            }
            if (version > 16 || data.size() < 2 || data.size() > 40) {
                return CNoDestination();
            }
            WitnessUnknown unk;
            unk.version = version;
            std::copy(data.begin(), data.end(), unk.program);
            unk.length = data.size();
            return unk;
        }
    }
    //Search Nickname
    
    return CNoDestination();
}

void LocalSetCurrencyForDestination(CTxDestination& dest, const unsigned char key2)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->currency=key2;
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        id->currency=key2;
    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        id->currency=key2;
    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        id->currency=key2;
    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        id->currency=key2;
    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        id->currency=key2;
    }
}

unsigned char LocalGetCurrencyForDestination(const CTxDestination& dest)
{
    unsigned char key2 = 0;

    if (auto id = boost::get<CKeyID>(&dest)) {
        key2 = id->currency;
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        key2 = id->currency; 
    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        key2 = id->currency; 
    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        key2 = id->currency; 
    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        key2 = id->currency; 
    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        key2 = id->currency;
    }
    return key2;   
}

void LocalSetDepositForDestination(CTxDestination& dest, const bool isdeposit)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->isdeposit = isdeposit;
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        id->isdeposit = isdeposit;
    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        id->isdeposit = isdeposit;
    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        id->isdeposit = isdeposit;
    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        id->isdeposit = isdeposit;
    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        id->isdeposit = isdeposit;
    }
}

bool LocalGetDepositForDestination(const CTxDestination& dest)
{
    bool isdeposit = 0;

    if (auto id = boost::get<CKeyID>(&dest)) {
        isdeposit = id->isdeposit;
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        isdeposit = id->isdeposit;
    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        isdeposit = id->isdeposit;
    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        isdeposit = id->isdeposit;
    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        isdeposit = id->isdeposit;
    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        isdeposit = id->isdeposit;
    }
    return isdeposit;   
}

std::string encodecurrency(const CTxDestination& dest,std::string str)
{
    unsigned char currency = LocalGetCurrencyForDestination(dest);
    bool isdeposit = LocalGetDepositForDestination(dest);
    if (currency == 2) {
        return "gold@"+str;
    } else
    if (currency == 1) {
        return "dollar@"+str;
    } else
    if (isdeposit) {
        return "deposit@"+str;
    }

    return str;    
}

void LocalSetSecondPubKeyForDestination(CTxDestination& dest, const CPubKey& key2)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->recokey.resize(33);
        std::copy(key2.begin(), key2.end() , id->recokey.begin());
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        id->recokey.resize(33);
        std::copy(key2.begin(), key2.end() , id->recokey.begin());
    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        id->recokey.resize(33);
        std::copy(key2.begin(), key2.end() , id->recokey.begin());
    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        id->recokey.resize(33);
        std::copy(key2.begin(), key2.end() , id->recokey.begin());
    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        id->recokey.resize(33);
        std::copy(key2.begin(), key2.end() , id->recokey.begin());
    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        id->recokey.resize(33);
        std::copy(key2.begin(), key2.end() , id->recokey.begin());
    }
}

void LocalSetNonPrivateForDestination(CTxDestination& dest, bool nonprivate)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->nonprivate = nonprivate;
//        LogPrintf("id->nonprivate %d\n",*(unsigned char*)&id->nonprivate);
    }
}

void LocalSetHasViewKeyForDestination(CTxDestination& dest, bool hasviewkey)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->hasviewkey = hasviewkey;
    }
}

void LocalSetViewPubKeyForDestination(CTxDestination& dest, const CPubKey& key2)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->viewkey.resize(33);
        std::copy(key2.begin(), key2.end() , id->viewkey.begin());
    }
}

CTxDestination DecodeDestination(const std::string& strinput, const CChainParams& params)
{
    unsigned char currency = 0;
    bool isdeposit = false;
    std::string str = strinput;

/*    if (str.length()>=7 && str[0]=='g' && str[1]=='o' && str[2]=='l' && str[3]=='d' && str[4]=='@') {
        currency = 2;
        str.erase(0, 5);
    } else
    if (str.length()>=7 && str[0]=='d' && str[1]=='o' && str[2]=='l' && str[3]=='l' && str[4]=='a' && str[5]=='r' && str[6]=='@') {
        currency = 1;
        str.erase(0, 7);
    } else*/
    if (str.length()>=8 && str[0]=='b' && str[1]=='i' && str[2]=='t' && str[3]=='c' && str[4]=='a' && str[5]=='s' && str[6]=='h' && str[7]=='@') {
        currency = 0;
        str.erase(0, 8);
    } else
    if (str.length()>=8 && str[0]=='d' && str[1]=='e' && str[2]=='p' && str[3]=='o' && str[4]=='s' && str[5]=='i' && str[6]=='t' && str[7]=='@') {
        isdeposit = true;
        str.erase(0, 8);
    }

    CTxDestination dest = DecodeDestinationNoNickname(str, params);
    LocalSetCurrencyForDestination(dest, currency);
    LocalSetDepositForDestination(dest, isdeposit);
    if (auto id = boost::get<CNoDestination>(&dest)) {
       //try to search nickname       
       CPubKey pubkey = GetAddressForNickname(str);
       if (pubkey.IsValid()) {
           dest = GetDestinationforNickname(pubkey, IsNonPrivateNickname(str), HasViewKeyNickname(str), GetViewKeyNickname(str));
           LocalSetCurrencyForDestination(dest, currency);
           LocalSetDepositForDestination(dest, isdeposit);
           return dest;
       } else return CNoDestination();
    } else return dest;
}

} // namespace

CKey DecodeSecret(const std::string& str)
{
    CKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& privkey_prefix = Params().Base58Prefix(CChainParams::SECRET_KEY);
        const std::vector<unsigned char>& privkey_prefixBTC = Params().Base58Prefix(CChainParams::SECRET_KEYBTC);
        //RECO key
        if ((data.size() == 32 + privkey_prefix.size() || (data.size() == 33 + privkey_prefix.size() && data.back() == 1)) &&
            std::equal(privkey_prefix.begin(), privkey_prefix.end(), data.begin())) {
            bool compressed = data.size() == 33 + privkey_prefix.size();
            key.Set(data.begin() + privkey_prefix.size(), data.begin() + privkey_prefix.size() + 32, compressed);
        } else
        //BTC key
        if ((data.size() == 32 + privkey_prefixBTC.size() || (data.size() == 33 + privkey_prefixBTC.size() && data.back() == 1)) &&
            std::equal(privkey_prefixBTC.begin(), privkey_prefixBTC.end(), data.begin())) {
            bool compressed = data.size() == 33 + privkey_prefixBTC.size();
            key.Set(data.begin() + privkey_prefixBTC.size(), data.begin() + privkey_prefixBTC.size() + 32, compressed);
        }
    }
    memory_cleanse(data.data(), data.size());
    return key;
}

std::string EncodeSecret(const CKey& key)
{
    assert(key.IsValid());
    std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::SECRET_KEY);
    data.insert(data.end(), key.begin(), key.end());
    if (key.IsCompressed()) {
        data.push_back(1);
    }
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());   
    return ret;
}

CExtPubKey DecodeExtPubKey(const std::string& str)
{
    CExtPubKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string EncodeExtPubKey(const CExtPubKey& key)
{
    std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    return ret;
}

CExtKey DecodeExtKey(const std::string& str)
{
    CExtKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string EncodeExtKey(const CExtKey& key)
{
    std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

bool DestinationHasSecondPubKey(const CTxDestination& dest)
{
    bool ret=false;

    if (auto id = boost::get<CKeyID>(&dest)) {
        ret=(id->recokey.size()>0);
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        ret=(id->recokey.size()>0);    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        ret=(id->recokey.size()>0);    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        ret=(id->recokey.size()>0);    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        ret=(id->recokey.size()>0);    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        ret=(id->recokey.size()>0);    
    }
    return ret;
}

std::string EncodeDestinationHasSecondKey(const CTxDestination& dest)
{    
    return encodecurrency(dest, boost::apply_visitor(DestinationEncoder(Params()), dest));
}

CTxDestination GetDestinationforNickname(CPubKey pubkey, bool isnonprivatenickname, bool nicknamehasviewkey, CPubKey viewpubkey)
{
    CTxDestination dest = CTxDestination(pubkey.GetID());
    LocalSetSecondPubKeyForDestination(dest, pubkey);        
    LocalSetNonPrivateForDestination(dest, isnonprivatenickname);
    LocalSetHasViewKeyForDestination(dest, nicknamehasviewkey);
    LocalSetViewPubKeyForDestination(dest, viewpubkey);
    return dest;
}

std::string EncodeDestination(const CTxDestination& dest,const CPubKey& key2)
{    
    CTxDestination destnew=dest;
    
    if (!DestinationHasSecondPubKey(destnew)){
        LocalSetSecondPubKeyForDestination(destnew,key2);  
    }
    return encodecurrency(destnew, boost::apply_visitor(DestinationEncoder(Params()), destnew));
}

std::string EncodeDestination(const CPubKey& key2)
{    
    return EncodeDestination(key2.GetID(),key2);
}


CTxDestination DecodeDestination(const std::string& str)
{
    return DecodeDestination(str,Params());
}

CTxDestination DecodeDestinationNoNickname(const std::string& str)
{
    return DecodeDestinationNoNickname(str,Params());
}

CTxDestination PubKeyToDestination(CPubKey pkey)
{
    CTxDestination dest=pkey.GetID();       
    LocalSetSecondPubKeyForDestination(dest,pkey);
    return dest;
}


bool IsValidDestinationString(const std::string& str, const CChainParams& params)
{
    return IsValidDestination(DecodeDestination(str, params));
}

bool IsValidDestinationString(const std::string& str)
{
    return IsValidDestinationString(str, Params());
}

bool IsValidDestinationStringNoNickname(const std::string& str)
{
    return IsValidDestination(DecodeDestinationNoNickname(str, Params()));
}

