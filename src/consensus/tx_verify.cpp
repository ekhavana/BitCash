// Copyright (c) 2017-2017 The Bitcash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_verify.h>

#include <consensus/consensus.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <consensus/validation.h>
#include <primitives/block.h>

// TODO remove the following dependencies
#include <chain.h>
#include <coins.h>
#include <miner.h>
#include <utilmoneystr.h>
#include <nicknames.h>
#include <key_io.h>
#include <utilstrencodings.h>

extern std::string NicknameMasterPubKey;

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
    if (tx.nLockTime == 0)
        return true;
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    for (const auto& txin : tx.vin) {
        if (!(txin.nSequence == CTxIn::SEQUENCE_FINAL))
            return false;
    }
    return true;
}

std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    assert(prevHeights->size() == tx.vin.size());

    // Will be set to the equivalent height- and time-based nLockTime
    // values that would be necessary to satisfy all relative lock-
    // time constraints given our view of block chain history.
    // The semantics of nLockTime are the last invalid height/time, so
    // use -1 to have the effect of any height or time being valid.
    int nMinHeight = -1;
    int64_t nMinTime = -1;

    // tx.nVersion is signed integer so requires cast to unsigned otherwise
    // we would be doing a signed comparison and half the range of nVersion
    // wouldn't support BIP 68.
    bool fEnforceBIP68 = static_cast<uint32_t>(tx.nVersion) >= 2
                      && flags & LOCKTIME_VERIFY_SEQUENCE;

    // Do not enforce sequence numbers as a relative lock time
    // unless we have been instructed to
    if (!fEnforceBIP68) {
        return std::make_pair(nMinHeight, nMinTime);
    }

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
        const CTxIn& txin = tx.vin[txinIndex];

        // Sequence numbers with the most significant bit set are not
        // treated as relative lock-times, nor are they given any
        // consensus-enforced meaning at this point.
        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
            // The height of this input is not relevant for sequence locks
            (*prevHeights)[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = (*prevHeights)[txinIndex];

        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) {
            int64_t nCoinTime = block.GetAncestor(std::max(nCoinHeight-1, 0))->GetMedianTimePast();
            // NOTE: Subtract 1 to maintain nLockTime semantics
            // BIP 68 relative lock times have the semantics of calculating
            // the first block or time at which the transaction would be
            // valid. When calculating the effective block time or height
            // for the entire transaction, we switch to using the
            // semantics of nLockTime which is the last invalid block
            // time or height.  Thus we subtract 1 from the calculated
            // time or height.

            // Time-based relative lock-times are measured from the
            // smallest allowed timestamp of the block containing the
            // txout being spent, which is the median time past of the
            // block prior.
            nMinTime = std::max(nMinTime, nCoinTime + (int64_t)((txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
        } else {
            nMinHeight = std::max(nMinHeight, nCoinHeight + (int)(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }

    return std::make_pair(nMinHeight, nMinTime);
}

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair)
{
    assert(block.pprev);
    int64_t nBlockTime = block.pprev->GetMedianTimePast();
    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime)
        return false;

    return true;
}

bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    return EvaluateSequenceLocks(block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    for (const auto& txin : tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const auto& txout : tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        if (tx.vin[i].isnickname)continue;
        const Coin& coin = inputs.AccessCoin(tx.vin[i].prevout);
        assert(!coin.IsSpent());
        const CTxOut &prevout = coin.out;
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, int flags)
{
    int64_t nSigOps = GetLegacySigOpCount(tx) * WITNESS_SCALE_FACTOR;

    if (tx.IsCoinBase())
        return nSigOps;

    if (flags & SCRIPT_VERIFY_P2SH) {
        nSigOps += GetP2SHSigOpCount(tx, inputs) * WITNESS_SCALE_FACTOR;
    }

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        if (tx.vin[i].isnickname)continue;
        const Coin& coin = inputs.AccessCoin(tx.vin[i].prevout);
        assert(!coin.IsSpent());
        const CTxOut &prevout = coin.out;
        nSigOps += CountWitnessSigOps(tx.vin[i].scriptSig, prevout.scriptPubKey, &tx.vin[i].scriptWitness, flags);
    }
    return nSigOps;
}

bool CheckTransaction(const CTransaction& tx, CValidationState &state, bool fCheckDuplicateInputs, uint256 blockhash, bool checkdblnicks)
{
    // Basic checks that don't depend on any context
    CPubKey nicknamemasterpubkey(ParseHex(NicknameMasterPubKey));
    bool isnick=true;


    for (const auto& txin : tx.vin){
        if (!txin.isnickname) isnick=false;
    }

    if (tx.haspricerange && tx.minprice > tx.maxprice) {
        return state.DoS(10, false, REJECT_INVALID, "Minprice-higher-than-maxprice");
    }

    if (tx.vin.empty())
        return state.DoS(10, false, REJECT_INVALID, "Bad-txns-vin-empty");
    if (tx.vout.empty() && !isnick)
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT)
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;

    for (const auto& txout : tx.vout)
    {
        if (txout.currency > 0 && tx.nVersion >= 9) return state.DoS(100, false, REJECT_INVALID, "Transactions which create Dollar or Gold are deactivated for now.");

        if (txout.currency >= 3) return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-currency-not-supported");

        if (txout.referenceline.length()>1000)
            return state.DoS(100, false, REJECT_INVALID, "CTransaction::CheckTransaction() : txout.referenceline encrypted length>1000 characters");

//std::cout << "i" << i << std::endl;i++;
//std::cout << "txout.nValue" << FormatMoney(txout.nValue) << std::endl;
//std::cout << "txout.nValueBitCash" << FormatMoney(txout.nValueBitCash) << std::endl;
        if (txout.nValue < 0)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs - note that this check is slow so we skip it in CheckBlock
    if (fCheckDuplicateInputs && !isnick) {
        std::set<COutPoint> vInOutPoints;
        for (const auto& txin : tx.vin)
        {
            if (!vInOutPoints.insert(txin.prevout).second)
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");
        }
    }

    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
    }
    else
    if (!isnick) {
        for (const auto& txin : tx.vin)
            if (txin.prevout.IsNull())
                return state.DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null");
    }

    //check nicknames
    for (unsigned int i = 0; i < tx.vin.size(); ++i) {
        if (tx.vin[i].isnickname) {
            if (tx.vin[i].nickname.size()<3) 
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the nickname is too short.");
            if (tx.vin[i].nickname.size()>20) 
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the nickname is too long.");

            bool validcharacters=true;
            for (unsigned int idx = 0; idx < tx.vin[i].nickname.size(); ++idx)
            {
                int ch = tx.vin[i].nickname[idx];
                if (((ch >= '0' && ch<='9') ||
                   (ch >= 'a' && ch<='z') ||
                   (ch >= 'A' && ch<='Z')))
                {
                }
                else
                {
                    validcharacters=false;
                    break;
                }
            }

            if (!validcharacters)
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the nickname contains invalid characters.");
            if (checkdblnicks && DoesNicknameExist(tx.vin[i].nickname) && GetHashForNickname(tx.vin[i].nickname) != blockhash)
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the nickname " + tx.vin[i].nickname + "already exists. New block: "+blockhash.ToString()+" Old block:"+ GetHashForNickname(tx.vin[i].nickname).ToString());
            if (!tx.vin[i].address.IsValid())
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the address is invalid.");
            if (tx.vin[i].nicknamehasviewkey && !tx.vin[i].viewpubkey.IsValid())
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the viewpubkey is invalid.");


            uint256 hash = Hash(tx.vin[i].nickname.begin(), tx.vin[i].nickname.end(), tx.vin[i].address.begin(), tx.vin[i].address.end());

            if (checkdblnicks && GetAnyNicknameForAddress(tx.vin[i].address).size() > 0 && GetHashForAddress(tx.vin[i].address) != blockhash && !nicknamemasterpubkey.Verify(hash, tx.vin[i].nicknamesig))                
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: this address already has a nickname.");
    
            if (!tx.vin[i].address.Verify(hash, tx.vin[i].nicknamesig) && !nicknamemasterpubkey.Verify(hash, tx.vin[i].nicknamesig)) 
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the signature for the nickname transaction is invalid.");

            continue;
        }
    }


    return true;
}

bool Consensus::CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, CAmount& txfee, uint256 blockhash, bool usehash, const CBlock* block)
{
    CPubKey nicknamemasterpubkey(ParseHex(NicknameMasterPubKey));
    // are the actual inputs available?
    if (!inputs.HaveInputs(tx)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-missingorspent", false,
                         strprintf("%s: inputs missing/spent", __func__));
    }

    bool isfirst = true;
    unsigned char currency = 0;
    CAmount nValueIn = 0;

    for (unsigned int i = 0; i < tx.vin.size(); ++i) {
        if (tx.vin[i].isnickname) {
            if (tx.vin[i].nickname.size()<3) 
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the nickname is too short.");
            if (tx.vin[i].nickname.size()>20) 
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the nickname is too long.");

            bool validcharacters=true;
            for (unsigned int idx = 0; idx < tx.vin[i].nickname.size(); ++idx)
            {
                int ch = tx.vin[i].nickname[idx];
                if (((ch >= '0' && ch<='9') ||
                   (ch >= 'a' && ch<='z') ||
                   (ch >= 'A' && ch<='Z')))
                {
                }
                else
                {
                    validcharacters=false;
                    break;
                }
            }

            if (!validcharacters)
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the nickname contains invalid characters.");
            if (DoesNicknameExist(tx.vin[i].nickname) && (!usehash || GetHashForNickname(tx.vin[i].nickname)!=blockhash))
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the nickname already exists.");
            if (!tx.vin[i].address.IsValid())
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the address is invalid.");

            uint256 hash=Hash(tx.vin[i].nickname.begin(),tx.vin[i].nickname.end(),tx.vin[i].address.begin(),tx.vin[i].address.end());

            if (GetAnyNicknameForAddress(tx.vin[i].address).size() > 0 && (!usehash || GetHashForAddress(tx.vin[i].address)!=blockhash) && !nicknamemasterpubkey.Verify(hash, tx.vin[i].nicknamesig))
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: this address already has a nickname."); 
    
            if (!tx.vin[i].address.Verify(hash, tx.vin[i].nicknamesig) && !nicknamemasterpubkey.Verify(hash, tx.vin[i].nicknamesig)) 
                return state.Invalid(false, REJECT_INVALID, "Bad transaction: the signature for the nickname transaction is invalid.");

            continue;
        }

        const COutPoint &prevout = tx.vin[i].prevout;
        const Coin& coin = inputs.AccessCoin(prevout);
        assert(!coin.IsSpent());

        // If prev is coinbase, check that it's matured
        if (coin.IsCoinBase() && nSpendHeight - coin.nHeight < COINBASE_MATURITY) {
            return state.Invalid(false,
                REJECT_INVALID, "bad-txns-premature-spend-of-coinbase",
                strprintf("tried to spend coinbase at depth %d", nSpendHeight - coin.nHeight));
        }

        unsigned char curr = 0;
        if (coin.IsCoinBase()) curr = 0;else
        curr = coin.out.currency;
        
        if (isfirst) { 
            currency = curr;
            isfirst = false;
        } else {
            if (currency != curr) {
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-input-currencies-not-equal");
            }
        }

        // Check for negative or overflow input values
        nValueIn += coin.out.nValue;
        if (!MoneyRange(coin.out.nValue) || !MoneyRange(nValueIn)) {
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputvalues-outofrange");
        }
    }


    CAmount value_out = tx.GetValueOut();

    if (nValueIn < value_out) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-belowout", false,
            strprintf("value in (%s) < value out (%s)", FormatMoney(nValueIn), FormatMoney(value_out)));
    }

    // Tally transaction fees
    CAmount txfee_aux = nValueIn - value_out;

    if (currency != 0) {
        //convert fee into currency 0 
        if (currency == 1) {
            if (block != NULL) {
                txfee_aux = (__int128_t)txfee_aux * (__int128_t)COIN / (__int128_t)block->GetPriceinCurrency(0);
            } else 
            {
                CAmount secondprice;
                CAmount thirdprice;
                txfee_aux = (__int128_t)txfee_aux * (__int128_t)COIN / (__int128_t)GetCachedPriceInformation(30 * 60 * 1000, secondprice, thirdprice);
            }
        } else
        if (currency == 2) {
            if (block != NULL) {
                txfee_aux = ((__int128_t)txfee_aux * (__int128_t)block->GetPriceinCurrency(2) / (__int128_t)COIN ) * (__int128_t)COIN / (__int128_t)block->GetPriceinCurrency(0);
            } else 
            {
                CAmount secondprice;
                CAmount thirdprice;
                CAmount firstprice = GetCachedPriceInformation(30 * 60 * 1000, secondprice, thirdprice);
                txfee_aux = ((__int128_t)txfee_aux * (__int128_t)thirdprice / (__int128_t)COIN ) * (__int128_t)COIN / (__int128_t)firstprice;
            }
        }
    }

    if (!MoneyRange(txfee_aux)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-outofrange");
    }

    txfee = txfee_aux;
    return true;
}
