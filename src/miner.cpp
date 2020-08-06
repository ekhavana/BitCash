// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <miner.h>

#include <amount.h>
#include <boost/algorithm/string.hpp>
#include <cctype>
#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/tx_verify.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <rpc/blockchain.h>
#include "cuckoo/miner.h"
#include <hash.h>
#include <validation.h>
#include <net.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <timedata.h>
#include <tuple>
#include <util.h>
#include <utility>
#include <utilmoneystr.h>
#include <validationinterface.h>
#include <wallet/wallet.h>

#include <algorithm>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <queue>
#include <utility>

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include "RSJparser.tcc"
#include <curl/curl.h>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
 
std::string getdocumentwithcurl(std::string url)
{
  CURL *curl;
  CURLcode res;
  std::string readBuffer = "";

//std::cout << url << std::endl;
 
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    /* example.com is redirected, so we tell libcurl to follow redirection */ 
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
 
    /* Perform the request, res will get the return code */ 
    res = curl_easy_perform(curl);
    /* Check for errors */ 
/*    if(res != CURLE_OK) {        
    std::cout << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    }    
     std::cout << readBuffer << std::endl;*/
    /* always cleanup */ 
    curl_easy_cleanup(curl);
    return readBuffer;
  }
}

// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest fee rate of a transaction combined with all
// its ancestors.

bool gpuminingfailed = false;
bool trygpumining = true;

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockWeight = 0;

const int MAX_NONCE = 0xfffff;
CAmount minedcoins=0;
bool triedoneproofofwork = false;
bool addednewpriceinfo = false;

bool AddPriceInformation(CBlockHeader *pblock);

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams).nBits;

    if (pblock->nTime < consensusParams.DEACTIVATEPRICESERVERS) {

        if (pblock->nTime > consensusParams.STABLETIME && 
            (!(pblock->nPriceInfo.priceTime == pblock->nTime || (pblock->nPriceInfo.priceTime > pblock->nTime && pblock->nPriceInfo.priceTime <= pblock->nTime + MAX_PRICETIME_DIFFERENCE / 3) 
                                                         || (pblock->nPriceInfo.priceTime < pblock->nTime && pblock->nPriceInfo.priceTime + MAX_PRICETIME_DIFFERENCE / 3 >= pblock->nTime)))) {
            AddPriceInformation(pblock);
            addednewpriceinfo = true;
        } else
        if (pblock->nTime > consensusParams.STABLETIME && 
            (!(pblock->nPriceInfo2.priceTime == pblock->nTime || (pblock->nPriceInfo2.priceTime > pblock->nTime && pblock->nPriceInfo2.priceTime <= pblock->nTime + MAX_PRICETIME_DIFFERENCE / 3) 
                                                         || (pblock->nPriceInfo2.priceTime < pblock->nTime && pblock->nPriceInfo2.priceTime + MAX_PRICETIME_DIFFERENCE / 3 >= pblock->nTime)))) {
            AddPriceInformation(pblock);
            addednewpriceinfo = true;
        } else
        if (pblock->nTime > consensusParams.STABLETIME && 
            (!(pblock->nPriceInfo3.priceTime == pblock->nTime || (pblock->nPriceInfo3.priceTime > pblock->nTime && pblock->nPriceInfo3.priceTime <= pblock->nTime + MAX_PRICETIME_DIFFERENCE / 3) 
                                                         || (pblock->nPriceInfo3.priceTime < pblock->nTime && pblock->nPriceInfo3.priceTime + MAX_PRICETIME_DIFFERENCE / 3 >= pblock->nTime)))) {
            AddPriceInformation(pblock);
            addednewpriceinfo = true;
        }
    }

    if (pblock->nTime > consensusParams.X16RTIME)
       pblock->nVersion |= hashx16Ractive;


    if (pblock->nTime > consensusParams.X16RV2TIME)
       pblock->nVersion |= hashx16rv2active;

    if (pblock->nTime > consensusParams.GPUMINERTIME)
       pblock->nVersion |= gpumineractive;

    if (pblock->nTime > consensusParams.X25XTIME)
       pblock->nVersion |= hashx25Xactive;

    return nNewTime - nOldTime;
}


BlockAssembler::Options::Options() {
    blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
}

BlockAssembler::BlockAssembler(const CChainParams& params, const Options& options) : chainparams(params)
{
    blockMinFeeRate = options.blockMinFeeRate;
    // Limit weight to between 4K and MAX_BLOCK_WEIGHT-4K for sanity:
    nBlockMaxWeight = std::max<size_t>(4000, std::min<size_t>(MAX_BLOCK_WEIGHT - 4000, options.nBlockMaxWeight));
}

static BlockAssembler::Options DefaultOptions(const CChainParams& params)
{
    // Block resource limits
    // If -blockmaxweight is not given, limit to DEFAULT_BLOCK_MAX_WEIGHT
    BlockAssembler::Options options;
    options.nBlockMaxWeight = gArgs.GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
    if (gArgs.IsArgSet("-blockmintxfee")) {
        CAmount n = 0;
        ParseMoney(gArgs.GetArg("-blockmintxfee", ""), n);
        options.blockMinFeeRate = CFeeRate(n);
    } else {
        options.blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }
    return options;
}

BlockAssembler::BlockAssembler(const CChainParams& params) : BlockAssembler(params, DefaultOptions(params)) {}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;
    fIncludeWitness = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}

/** Converts the nValue to other currencies according to the price information of the block and the currency of the transaction outputs */
void BlockAssembler::ConvertCurrenciesForBlockTemplate()
{
    int i;
    int j;

    CAmount pricerate = pblock->GetPriceinCurrency(0);//dollar->bitcash /
    CAmount pricerate2 = pblock->GetPriceinCurrency(1);//bitcash->dollar *
    CAmount pricerate3 = pblock->GetPriceinCurrency(2);//Gold->Dollar *

    for (i = 0;i < pblock->vtx.size(); i++) {

        CMutableTransaction tx(*pblock->vtx[i]);

        int inputcurrency = 0;//Currency of transaction inputs

            //only one input currency is allowed for all inputs
            for (j = 0;j < tx.vin.size(); j++) {                
                if (tx.vin[j].isnickname) continue;   
                CCoinsViewCache inputs(pcoinsTip.get());
                const Coin& coin = inputs.AccessCoin(tx.vin[j].prevout);                
                if (coin.IsCoinBase()) {
                    inputcurrency = 0;                    
                } else {
	                inputcurrency = coin.out.currency;
                }
                break;

            }

        for (j = 0;j < tx.vout.size(); j++) {
            if (tx.vout[j].currency != inputcurrency) {
                if (inputcurrency == 0 && tx.vout[j].currency == 1) {
                    //std::cout << "Input BitCash: " << FormatMoney(tx.vout[j].nValueBitCash) << std::endl;
                    //Convert BitCash into Dollars
                    tx.vout[j].nValue = (__int128_t)tx.vout[j].nValueBitCash * (__int128_t)pricerate2 / (__int128_t)COIN;
                    //std::cout << "Converted to Dollar: " << FormatMoney(tx.vout[j].nValue) << std::endl;
                } else
                if (inputcurrency == 1 && tx.vout[j].currency == 0) {
                    //std::cout << "Input Dollar: " << FormatMoney(tx.vout[j].nValueBitCash) << std::endl;
                    //Convert Dollars into BitCash
                    tx.vout[j].nValue = (__int128_t)tx.vout[j].nValueBitCash * (__int128_t)COIN / (__int128_t)pricerate;
                    //std::cout << "Converted to BitCash: " << FormatMoney(tx.vout[j].nValue) << std::endl;
                } else
                if (inputcurrency == 0 && tx.vout[j].currency == 2) {
                    //std::cout << "Input BitCash: " << FormatMoney(tx.vout[j].nValueBitCash) << std::endl;
                    //Convert BitCash into Gold
                    tx.vout[j].nValue = ((__int128_t)tx.vout[j].nValueBitCash * (__int128_t)pricerate2 / (__int128_t)COIN) * (__int128_t)COIN / (__int128_t)pricerate3;
                    //std::cout << "Converted to Dollar: " << FormatMoney(tx.vout[j].nValue) << std::endl;
                } else
                if (inputcurrency == 2 && tx.vout[j].currency == 0) {
                    //std::cout << "Input Dollar: " << FormatMoney(tx.vout[j].nValueBitCash) << std::endl;
                    //Convert Gold into BitCash
                    tx.vout[j].nValue = ((__int128_t)tx.vout[j].nValueBitCash * (__int128_t)COIN / (__int128_t)pricerate) * (__int128_t)pricerate3 / (__int128_t)COIN;
                    //std::cout << "Converted to BitCash: " << FormatMoney(tx.vout[j].nValue) << std::endl;
                } else
                if (inputcurrency == 1 && tx.vout[j].currency == 2) {
                    //std::cout << "Input BitCash: " << FormatMoney(tx.vout[j].nValueBitCash) << std::endl;
                    //Convert Dollar into Gold
                    tx.vout[j].nValue = (__int128_t)tx.vout[j].nValueBitCash * (__int128_t)COIN / (__int128_t)pricerate3;
                    //std::cout << "Converted to Dollar: " << FormatMoney(tx.vout[j].nValue) << std::endl;
                } else
                if (inputcurrency == 2 && tx.vout[j].currency == 1) {
                    //std::cout << "Input Dollar: " << FormatMoney(tx.vout[j].nValueBitCash) << std::endl;
                    //Convert Gold into Dollar
                    tx.vout[j].nValue = (__int128_t)tx.vout[j].nValueBitCash * (__int128_t)pricerate3 / (__int128_t)COIN;
                    //std::cout << "Converted to BitCash: " << FormatMoney(tx.vout[j].nValue) << std::endl;
                }
            } else tx.vout[j].nValue = tx.vout[j].nValueBitCash;
        } 
	pblock->vtx[i] = MakeTransactionRef(std::move(tx));
    }
}

/** Converts the nValue to other currencies according to the price information of the block and the currency of the transaction outputs */
void BlockAssembler::CalculateFeesForBlock()
{
    int i;
    int j;

    CAmount pricerate = pblock->GetPriceinCurrency(0);

    nFees = 0;

    for (i = 1;i < pblock->vtx.size(); i++) {//start from transaction 1 not 0, because transaction 0 is the coinbase placeholder

        CTransaction tx(*pblock->vtx[i]);

        int currency = 0;//Currency of transaction inputs
        CAmount nValueIn = 0;

        for (j = 0;j < tx.vin.size(); j++) {                
            if (tx.vin[j].isnickname) continue;   
            CCoinsViewCache inputs(pcoinsTip.get());
            const Coin& coin = inputs.AccessCoin(tx.vin[j].prevout);                
            if (coin.IsCoinBase()) {
               currency = 0;
            } else
                currency = coin.out.currency;
            nValueIn += coin.out.nValue;
        }
        CAmount value_out = tx.GetValueOut();

        // Tally transaction fees
        CAmount txfee_aux = nValueIn - value_out;
        if (currency != 0) {
            //convert fee into currency 0 
            if (currency == 1) {
                txfee_aux = (__int128_t)txfee_aux * (__int128_t)COIN / (__int128_t)pblock->GetPriceinCurrency(0);
            } else
            if (currency == 2) {
                txfee_aux = ((__int128_t)txfee_aux * (__int128_t)pblock->GetPriceinCurrency(2) / (__int128_t)COIN ) * (__int128_t)COIN / (__int128_t)pblock->GetPriceinCurrency(0);
            }
        }
        nFees += txfee_aux;
    }
}


std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlockWithScriptPubKey(const CScript& scriptPubKeyIn, bool fMineWitnessTx)
{
    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK2(cs_main, mempool.cs);
    AddPriceInformation(pblock);
    CBlockIndex* pindexPrev = chainActive.Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;

    pblock->nTime = GetAdjustedTime();
    pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
    if (pblock->nTime > chainparams.GetConsensus().STABLETIME)
       pblock->nVersion |= stabletimeactive;
    if (pblock->nTime > chainparams.GetConsensus().X16RTIME)
       pblock->nVersion |= hashx16Ractive;
    if (pblock->nTime > chainparams.GetConsensus().X16RV2TIME)
       pblock->nVersion |= hashx16rv2active;
    if (pblock->nTime > chainparams.GetConsensus().GPUMINERTIME)
       pblock->nVersion |= gpumineractive;
    if (pblock->nTime > chainparams.GetConsensus().X25XTIME)
       pblock->nVersion |= hashx25Xactive;


    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = gArgs.GetArg("-blockversion", pblock->nVersion);

    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    // Decide whether to include witness transactions
    // This is only needed in case the witness softfork activation is reverted
    // (which would require a very deep reorganization) or when
    // -promiscuousmempoolflags is used.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
//    fIncludeWitness = IsWitnessEnabled(pindexPrev, chainparams.GetConsensus()) && fMineWitnessTx;

    fIncludeWitness=true;//always include Witness transactions

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated);

    nLastBlockTx = nBlockTx;
    nLastBlockWeight = nBlockWeight;

    //fix problem with nickname transaction which are already mined in the mempool->erase transaction which do not pass CheckTransaction from the block template
    for (unsigned int i = pblock->vtx.size()-1; i>0; i--) {//start from transaction 1 not 0, because transaction 0 is the coinbase placeholder

        CTransaction tx(*pblock->vtx[i]);

        CValidationState state;

        if (!CheckTransaction(tx, state)){
            LogPrintf("CreateNewBlockWithScriptPubKey CheckTransaction rejected the transaction: msg: %s Txid: %s\n", FormatStateMessage(state), tx.GetHash().ToString());
            pblock->vtx.erase( pblock->vtx.begin() + i);
        }

    }

    CalculateFeesForBlock();

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(2);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
    coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    coinbaseTx.vout[0].nValueBitCash = coinbaseTx.vout[0].nValue;

    //Pay to the development fund....
    coinbaseTx.vout[1].scriptPubKey = GetScriptForRawPubKey(CPubKey(ParseHex(Dev1scriptPubKey)));
    coinbaseTx.vout[1].nValue = GetBlockSubsidyDevs(nHeight, chainparams.GetConsensus());
    coinbaseTx.vout[1].nValueBitCash = coinbaseTx.vout[1].nValue;

    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;

    coinbaseTx.hashashinfo = true;

    CHashWriter ss(SER_GETHASH, 0);
        ss << pblock->nPriceInfo;
        ss << pblock->priceSig;
        ss << pblock->nPriceInfo2;
        ss << pblock->priceSig2;
        ss << pblock->nPriceInfo3;
        ss << pblock->priceSig3;
    coinbaseTx.hashforpriceinfo = ss.GetHash();

    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    pblocktemplate->vTxFees[0] = -nFees;
    
//    LogPrintf("CreateNewBlock(): block weight: %u txs: %u fees: %ld sigops %d C nVersion %d\n", GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost, coinbaseTx.nVersion);

    auto pow = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());

    // Fill in header    

    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
    pblock->nBits          = pow.nBits;
    pblock->nNonce         = 0;
    pblock->nEdgeBits = pow.nEdgeBits;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    ConvertCurrenciesForBlockTemplate();

    pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
    }

    return std::move(pblocktemplate);
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(interfaces::Wallet* iwallet,
    CWallet* wallet,
    bool useinterface, bool fMineWitnessTx, bool includeextranonce)
{
    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction  
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK2(cs_main, mempool.cs);
    AddPriceInformation(pblock);
    CBlockIndex* pindexPrev = chainActive.Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;

    pblock->nTime = GetAdjustedTime();
    pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
    if (pblock->nTime > chainparams.GetConsensus().STABLETIME)
       pblock->nVersion |= stabletimeactive;
    if (pblock->nTime > chainparams.GetConsensus().X16RTIME)
       pblock->nVersion |= hashx16Ractive;
    if (pblock->nTime > chainparams.GetConsensus().X16RV2TIME)
       pblock->nVersion |= hashx16rv2active;
    if (pblock->nTime > chainparams.GetConsensus().GPUMINERTIME)
       pblock->nVersion |= gpumineractive;
    if (pblock->nTime > chainparams.GetConsensus().X25XTIME)
       pblock->nVersion |= hashx25Xactive;


    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = gArgs.GetArg("-blockversion", pblock->nVersion);

    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    // Decide whether to include witness transactions
    // This is only needed in case the witness softfork activation is reverted
    // (which would require a very deep reorganization) or when
    // -promiscuousmempoolflags is used.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
//    fIncludeWitness = IsWitnessEnabled(pindexPrev, chainparams.GetConsensus()) && fMineWitnessTx;

    fIncludeWitness=true;//always include Witness transactions

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated);

    nLastBlockTx = nBlockTx;
    nLastBlockWeight = nBlockWeight;

    CalculateFeesForBlock();

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(2);
   
    coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    coinbaseTx.vout[0].nValueBitCash = coinbaseTx.vout[0].nValue;
    if (useinterface){
        iwallet->FillTxOutForTransaction(coinbaseTx.vout[0],iwallet->GetCurrentAddressPubKey(),"", 0, false, false, CPubKey(), coinbaseTx.nVersion >= 6, coinbaseTx.nVersion >= 7, false, CKey());
    } else
    {
        wallet->FillTxOutForTransaction(coinbaseTx.vout[0],wallet->GetCurrentAddressPubKey(),"", 0, false, false, CPubKey(), coinbaseTx.nVersion >= 6, coinbaseTx.nVersion >= 7, false, CKey());
    }
    //Pay to the development fund....
    coinbaseTx.vout[1].scriptPubKey = GetScriptForRawPubKey(CPubKey(ParseHex(Dev1scriptPubKey)));
    coinbaseTx.vout[1].nValue = GetBlockSubsidyDevs(nHeight, chainparams.GetConsensus());
    coinbaseTx.vout[1].nValueBitCash = coinbaseTx.vout[1].nValue;

    if (includeextranonce) {
        coinbaseTx.vin[0].scriptSig = CScript() << nHeight << ParseHex("FFBBAAEE003344BBFFBBAAEE003344BB") << OP_0; 
    } else {
        coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;    
    }

    coinbaseTx.hashashinfo = true;

    CHashWriter ss(SER_GETHASH, 0);
        ss << pblock->nPriceInfo;
        ss << pblock->priceSig;
        ss << pblock->nPriceInfo2;
        ss << pblock->priceSig2;
        ss << pblock->nPriceInfo3;
        ss << pblock->priceSig3;
    coinbaseTx.hashforpriceinfo = ss.GetHash();

    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    pblocktemplate->vTxFees[0] = -nFees;

//    LogPrintf("CreateNewBlock(): block weight: %u txs: %u fees: %ld sigops %d C nVersion %d\n", GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost, coinbaseTx.nVersion);

    auto pow = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());

    // Fill in header   
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
    pblock->nBits          = pow.nBits;
    pblock->nNonce         = 0;
    pblock->nEdgeBits = pow.nEdgeBits;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    ConvertCurrenciesForBlockTemplate();

    pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
    }

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
    stream << pblock->vtx[0];
    std::string str=HexStr(stream.begin(),stream.end());
    std::size_t found = str.find("ffbbaaee003344bbffbbaaee003344bb");
    std::string str1=str;
    std::string str2="";
    if (found!=std::string::npos) {
       str1=str.substr(0,found+32-16);
       str2=str.substr(found+32,str.size()-found+32);
    }
    pblocktemplate->coinb1=str1;
    pblocktemplate->coinb2=str2;
    pblocktemplate->blockheight=nHeight;
    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}


std::set<std::string> exchanges;
std::set<std::string> exchangesall;

const int numexchangeinfos = 5;
std::string exchangeinfos[numexchangeinfos] = {
  {"https://wallet.choosebitcash.com/pricesources.txt"},
  {"https://webwallet.choosebitcash.com/priceinfo/pricesources.txt"},
  {"https://cadkas.com/priceinfo/pricesources.txt"},
  {"https://price.choosebitcash.com/priceinfo/pricesources.txt"},
  {"https://www.peerq.com/priceinfo/pricesources.txt"}
};

const int numwebsites = 5;
std::string pricewebsites[numwebsites] = {
  {"https://wallet.choosebitcash.com/getpriceinfo.php"},
  {"https://webwallet.choosebitcash.com/priceinfo/getpriceinfo.php"},
  {"https://cadkas.com/priceinfo/getpriceinfo.php"},
  {"https://price.choosebitcash.com/priceinfo/getpriceinfo.php"},
  {"https://www.peerq.com/priceinfo/getpriceinfo.php"}
};

//These are the new info points, which return all available price information at once
const int numallexchangeinfos = 5;
std::string allexchangeinfos[numallexchangeinfos] = {
  {"https://wallet.choosebitcash.com/priceallsources.txt"},
  {"https://webwallet.choosebitcash.com/priceinfo/priceallsources.txt"},
  {"https://cadkas.com/priceinfo/priceallsources.txt"},
  {"https://price.choosebitcash.com/priceinfo/priceallsources.txt"},
  {"https://www.peerq.com/priceinfo/priceallsources.txt"}
};

const int numallwebsites = 5;
std::string allpricewebsites[numallwebsites] = {
  {"https://wallet.choosebitcash.com/getallpriceinfo.php"},
  {"https://webwallet.choosebitcash.com/priceinfo/getallpriceinfo.php"},
  {"https://cadkas.com/priceinfo/getallpriceinfo.php"},
  {"https://price.choosebitcash.com/priceinfo/getallpriceinfo.php"},
  {"https://www.peerq.com/priceinfo/getallpriceinfo.php"}
};



CAmount pricecache = COIN;
CAmount pricecache2 = COIN;
CAmount pricecache3 = COIN;
bool haspriceinfo = false;
uint64_t pricetime = 0;

void GetExchangesListFromWebserver()
{
    //we already know these exchanges
    for (int i = 0; i < numwebsites; i++) {
        exchanges.insert(pricewebsites[i]);
    }

//std::cout << "count " << exchanges.size() << std::endl;

    //download a list of possible other exchanges
    for (int i = 0; i < numexchangeinfos; i++) {
        std::string server = exchangeinfos[i];
        try {
	    std::string priceinfo = getdocumentwithcurl(server);

//std::cout << "c.priceinfo " << c.priceinfo << std::endl;

	    std::istringstream stream(priceinfo);
	    std::string line;
	    while(std::getline(stream, line)) { 
                exchanges.insert(boost::algorithm::trim_copy(line));
	    }

        }
        catch (std::exception& e)
        {
        }
    }
//std::cout << "count " << exchanges.size() << std::endl;

//
// also update the list which price sources which will return all available price information at once
//
    //we already know these exchanges
    for (int i = 0; i < numallwebsites; i++) {
        exchangesall.insert(allpricewebsites[i]);
    }

//std::cout << "count " << exchangesall.size() << std::endl;

    //download a list of possible other exchanges
    for (int i = 0; i < numallexchangeinfos; i++) {
        std::string server = allexchangeinfos[i];
        try {
	    std::string priceinfo = getdocumentwithcurl(server);

//std::cout << "c.priceinfo " << c.priceinfo << std::endl;

	    std::istringstream stream(priceinfo);
	    std::string line;
	    while(std::getline(stream, line)) { 
                exchangesall.insert(boost::algorithm::trim_copy(line));
	    }

        }
        catch (std::exception& e)
        {
        }
    }
//std::cout << "count " << exchangesall.size() << std::endl;
}

bool parseinstaswapreplystarttransaction(std::string replystr, std::string &errorstr, std::string &transidstr, std::string &depositwalletstr, std::string &receivingamoutstr)
{
    RSJresource  json (replystr);

    if (json["apiInfo"].as<std::string>("")=="OK")
    {
        transidstr = json["response"]["TransactionId"].as<std::string>("");
        depositwalletstr = json["response"]["depositWallet"].as<std::string>("");
        receivingamoutstr = json["response"]["receivingAmount"].as<std::string>("");
        return true;
    } else
    {
        errorstr = json["response"].as<std::string>("");
        return false;
    }    
}


bool parseinstaswapreplygetamount(std::string replystr, std::string &errorstr, std::string &amountstr, std::string &minstr, std::string &maxdigitsstr)
{
    RSJresource  json (replystr);

    if (json["apiInfo"].as<std::string>("")=="OK")
    {
        amountstr = json["response"]["getAmount"].as<std::string>("");
        minstr = json["response"]["min"].as<std::string>("");
        maxdigitsstr = json["response"]["maxDigitsAfterDecimal"].as<std::string>("");
        return true;
    } else
    {
        errorstr = json["response"].as<std::string>("");
        return false;
    }    
}

CAmount GetPriceInformationFromWebserver(std::string server, std::string &price, std::string &signature, CAmount &secondprice, CAmount &thirdprice)
{
    secondprice = 0;
    thirdprice = 0;
    try
    {
        std::string priceinfo = getdocumentwithcurl(server);

        RSJresource  json (priceinfo);

        if (time(nullptr) < Params().GetConsensus().MASTERKEYDUMMY) {
            price = json["priceinfo"].as<std::string>("");
            signature = json["signature"].as<std::string>("");
        } else
        if (time(nullptr) < Params().GetConsensus().GOLDTIME) {
            price = json["priceinfo2"].as<std::string>("");
            signature = json["signature2"].as<std::string>("");
        } else
        {
            price = json["priceinfo3"].as<std::string>("");
            signature = json["signature3"].as<std::string>("");
        }
    }
    catch (std::exception& e)
    {
        return 0;
    }
    if (IsHex(price) && IsHex(signature)) {
        std::vector<unsigned char> txData(ParseHex(price));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            CPriceInfo nPriceInfo;
            ssData >> nPriceInfo;

            std::vector<unsigned char> priceSig;//Signature for nPriceInfo
            priceSig = ParseHex(signature);
            int privkeyusednr;

            if (CheckPriceInfo(nPriceInfo, priceSig, privkeyusednr)) {
                if (nPriceInfo.priceCount >= 3) {
                    thirdprice = nPriceInfo.prices[2];
                }
                secondprice = nPriceInfo.prices[1];
                return nPriceInfo.prices[0];
            } else {
                return 0;
            }

        } catch (const std::exception&) {
            // Fall through.
            return 0;
        }    
    } else return 0;

    return 0;
}

CAmount GetOnePriceInformation(std::string &price, std::string &signature, CAmount &secondprice, CAmount &thirdprice)
{   
    CAmount res = 0;
    int size = exchanges.size();
    int count = 0;

    while (res <= 0 && count < size*4) {
        int i = rand() % size;
        //get the i. element of exchanges
        std::set<std::string>::iterator it = exchanges.begin();
        std::advance(it, i);
        std::string ex = *it;

        res = GetPriceInformationFromWebserver(ex, price, signature, secondprice, thirdprice);
        count++;
    }

    return res;   
}

int GetPriceServerCount()
{
    return exchanges.size();
}

std::string GetPriceServerName(int i)
{   
    std::string outstr = "";
    
    if (i >= 0 && i < exchanges.size())
    {
        //get the i. element of exchanges
	    std::set<std::string>::iterator it = exchanges.begin();
	    std::advance(it, i);

        outstr = *it;

    } else outstr = "FAILED.";

    return outstr;   
}

int GetPriceServerAllCount()
{
    return exchangesall.size();
}

std::string GetPriceServerAllName(int i)
{   
    std::string outstr = "";
    
    if (i >= 0 && i < exchangesall.size())
    {
        //get the i. element of exchanges
	    std::set<std::string>::iterator it = exchangesall.begin();
	    std::advance(it, i);

        outstr = *it;

    } else outstr = "FAILED.";

    return outstr;   
}

std::string CheckPriceServer(int i)
{   
    std::string outstr = "";
    std::string price;
    std::string signature;
    
    if (i >= 0 && i < exchanges.size())
    {
        //get the i. element of exchanges
	    std::set<std::string>::iterator it = exchanges.begin();
	    std::advance(it, i);
        std::string ex = *it;

        int64_t nTime1 = GetTimeMicros();
        std::string price, signature;
        CAmount secondprice;
        CAmount thirdprice;
        GetPriceInformationFromWebserver(ex, price, signature, secondprice, thirdprice);

        bool found = false;
        CAmount price1, price2;
        if (IsHex(price) && IsHex(signature)) { 
            std::vector<unsigned char> txData(ParseHex(price));
            CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
            try {
                CPriceInfo nPriceInfo;
                ssData >> nPriceInfo;

                std::vector<unsigned char> priceSig;//Signature for nPriceInfo
                priceSig = ParseHex(signature);
                int privkeyusednr;

                if (CheckPriceInfo(nPriceInfo, priceSig, privkeyusednr)) {
                    price1 = nPriceInfo.prices[0];
                    price2 = nPriceInfo.prices[1];
                    found = true;
                }
            } catch (const std::exception&) {
                // Fall through.
            }    
        } 


        if (!found)
        {
            outstr += "FAILED ( " + std::to_string( ( GetTimeMicros()- nTime1 ) / 1000 ) + "ms ).";
        } else
        {
            outstr += "SUCCESSFUL ( " + FormatMoney(price1) + "; " + FormatMoney(price2) + "; " + std::to_string( ( GetTimeMicros()- nTime1 ) / 1000 ) + "ms ).";
        }
    } else outstr = "FAILED.";

    return outstr;   
}

std::string CheckPriceServerAll(int i)
{   
    std::string outstr = "";
    std::string price;
    std::string signature;
    
    if (i >= 0 && i < exchangesall.size())
    {
        //get the i. element of exchanges
	    std::set<std::string>::iterator it = exchangesall.begin();
	    std::advance(it, i);
        std::string ex = *it;

        int64_t nTime1 = GetTimeMicros();
        std::string price, signature, signature2, signature3;
        std::string secondprice;
        std::string thirdprice;
        GetAllPriceInformationFromWebserver(ex, price, signature, secondprice, signature2, thirdprice, signature3);

        bool found = false;
        CAmount price1, price2;
        if (IsHex(price) && IsHex(signature)) { 
            std::vector<unsigned char> txData(ParseHex(price));
            CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
            try {
                CPriceInfo nPriceInfo;
                ssData >> nPriceInfo;

                std::vector<unsigned char> priceSig;//Signature for nPriceInfo
                priceSig = ParseHex(signature);
                int privkeyusednr;

                if (CheckPriceInfo(nPriceInfo, priceSig, privkeyusednr)) {
                    price1 = nPriceInfo.prices[0];
                    price2 = nPriceInfo.prices[1];
                    found = true;
                }
            } catch (const std::exception&) {
                // Fall through.
            }    
        } 


        if (!found)
        {
            outstr += "FAILED ( " + std::to_string( ( GetTimeMicros()- nTime1 ) / 1000 ) + "ms ).";
        } else
        {
            outstr += "SUCCESSFUL ( " + FormatMoney(price1) + "; " + FormatMoney(price2) + "; " + std::to_string( ( GetTimeMicros()- nTime1 ) / 1000 ) + "ms ).";
        }
    } else outstr = "FAILED.";

    return outstr;   
}

//old model, request one price information from each server
CAmount GetPriceInformationFromDifferentServers(std::string &price, std::string &signature, std::string &price2, std::string &signature2, std::string &price3, std::string &signature3)
{   
    CAmount res = 0;
    int size = exchanges.size();
    int count = 0;
    int i1 = 0;
    int i2 = 0;

    while (res <= 0 && count < size*4) {
        int i = rand() % size;
        i1 = i;
        //get the i. element of exchanges
    	std::set<std::string>::iterator it = exchanges.begin();
	    std::advance(it, i);
        std::string ex = *it;

        
        CAmount secondprice;
        CAmount thirdprice;
        res = GetPriceInformationFromWebserver(ex, price, signature, secondprice, thirdprice);

        count++;
    }

    count = 0;
    res = 0;
    while (res <= 0 && count < size*4) {
       int i = 0;
        do {
            i = rand() % size;
        } while (i == i1);
        i2 = i;

        //get the i. element of exchanges
	std::set<std::string>::iterator it = exchanges.begin();
	std::advance(it, i);
        std::string ex = *it;

        CAmount secondprice;
        CAmount thirdprice;
        res = GetPriceInformationFromWebserver(ex, price2, signature2, secondprice, thirdprice);

        count++;
    }

    count = 0;
    res = 0;
    while (res <= 0 && count < size*4) {
       int i = 0;
        do {
            i = rand() % size;
        } while (i == i1 || i == i2);

        //get the i. element of exchanges
	std::set<std::string>::iterator it = exchanges.begin();
	std::advance(it, i);
        std::string ex = *it;

        CAmount secondprice;
        CAmount thirdprice;
        res = GetPriceInformationFromWebserver(ex, price3, signature3, secondprice, thirdprice);

        count++;
    }
    return res;   
}

bool GetAllPriceInformationFromWebserver(std::string server, std::string &price, std::string &signature, std::string &price2, std::string &signature2, std::string &price3, std::string &signature3)
{
    std::set<int> privatekeyused;
    try
    {
        std::string priceinfo = getdocumentwithcurl(server);
        RSJresource  json (priceinfo);

        std::string tempprice;
        std::string tempsignature;
        unsigned int pricecount = 0;
        for (unsigned int i = 0; i < json.as_array().size(); i++)
        {
            if (time(nullptr) < Params().GetConsensus().GOLDTIME) {
                tempprice = json[i]["priceinfo2"].as<std::string>("");
                tempsignature = json[i]["signature2"].as<std::string>("");
            } else
            {
                tempprice = json[i]["priceinfo3"].as<std::string>("");
                tempsignature = json[i]["signature3"].as<std::string>("");
            }

            bool isokay = true;
            if (IsHex(tempprice) && IsHex(tempsignature)) { 
                std::vector<unsigned char> txData(ParseHex(tempprice));
                CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
                try {
                    CPriceInfo nPriceInfo;
                    ssData >> nPriceInfo;

                    std::vector<unsigned char> priceSig;//Signature for nPriceInfo
                    priceSig = ParseHex(tempsignature);
                    int privkeyusednr;

                    if (CheckPriceInfo(nPriceInfo, priceSig, privkeyusednr)) {
                        std::set<int>::iterator it = privatekeyused.find(privkeyusednr);
	                    if(it != privatekeyused.end()) {
                            isokay = false;
                        } else {
                            privatekeyused.insert(privkeyusednr);
                            isokay = true;
                        }
                    } else isokay = false;
                } catch (const std::exception&) {
                    // Fall through.
                    isokay = false;
                }    
            } else isokay = false;

            if (isokay) {
                if (pricecount == 0) {
                    price = tempprice;
                    signature = tempsignature;
                } else
                if (pricecount == 1) {
                    price2 = tempprice;
                    signature2 = tempsignature;
                } else
                if (pricecount == 2) {
                    price3 = tempprice;
                    signature3 = tempsignature;
                }
                pricecount ++;
                if (pricecount == 3) {
                    return true;
                }
            }
        }
    }
    catch (std::exception& e)
    {
        return false;
    }

    return false;
}


//only one https:// request will return all available price information
bool GetPriceInformation(std::string &price, std::string &signature, std::string &price2, std::string &signature2, std::string &price3, std::string &signature3)
{   
    bool res = false;
    int size = exchangesall.size();
    int count = 0;
    int i1 = 0;
    int i2 = 0;

    while (res == false && count < size*4) {
        int i = rand() % size;
        i1 = i;
        //get the i. element of exchanges
    	std::set<std::string>::iterator it = exchangesall.begin();
	    std::advance(it, i);
        std::string ex = *it;

        res = GetAllPriceInformationFromWebserver(ex, price, signature, price2, signature2, price3, signature3);
        count++;
    }

    return res;   
}

bool BlockAssembler::AddPriceInformation(CBlock *pblock)
{
    std::string price = "";
    std::string signature = "";
    std::string price2 = "";
    std::string signature2 = "";
    std::string price3 = "";
    std::string signature3 = "";

    if (GetAdjustedTime() > chainparams.GetConsensus().DEACTIVATEPRICESERVERS)
    {
        pblock->nPriceInfo.priceTime = 0;
        pblock->nPriceInfo.priceCount = 3;
        pblock->nPriceInfo.prices[0] = 0.02 * COIN;
        pblock->nPriceInfo.prices[1] = 0.02 * COIN;
        pblock->nPriceInfo.prices[2] = 2000 * COIN;
        pblock->priceSig.clear();
        pblock->nPriceInfo2.priceTime = 0;
        pblock->nPriceInfo2.priceCount = 3;
        pblock->nPriceInfo2.prices[0] = 0.02 * COIN;
        pblock->nPriceInfo2.prices[1] = 0.02 * COIN;
        pblock->nPriceInfo2.prices[2] = 2000 * COIN;
        pblock->priceSig2.clear();
        pblock->nPriceInfo3.priceTime = 0;
        pblock->nPriceInfo3.priceCount = 3;
        pblock->nPriceInfo3.prices[0] = 0.02 * COIN;
        pblock->nPriceInfo3.prices[1] = 0.02 * COIN;
        pblock->nPriceInfo3.prices[2] = 2000 * COIN;
        pblock->priceSig3.clear();
        return true;
    }

    if (!GetPriceInformation(price, signature, price2, signature2, price3, signature3)) {
        //fall back to old modell
        if (GetPriceInformationFromDifferentServers(price, signature, price2, signature2, price3, signature3) <= 0) {
            return 0;
        }
    }

    if (IsHex(price)) { 
        std::vector<unsigned char> txData(ParseHex(price));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> pblock->nPriceInfo;
        } catch (const std::exception&) {
            // Fall through.
            return false;
        }    
    } else return false;

    pblock->priceSig = ParseHex(signature);

    if (IsHex(price2)) { 
        std::vector<unsigned char> txData(ParseHex(price2));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> pblock->nPriceInfo2;
        } catch (const std::exception&) {
            // Fall through.
            return false;
        }    
    } else return false;

    pblock->priceSig2 = ParseHex(signature2);

    if (IsHex(price3)) { 
        std::vector<unsigned char> txData(ParseHex(price3));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> pblock->nPriceInfo3;
        } catch (const std::exception&) {
            // Fall through.
            return false;
        }    
    } else return false;

    pblock->priceSig3 = ParseHex(signature3);
    return true;
}

bool AddPriceInformation(CBlockHeader *pblock)
{
    std::string price = "";
    std::string signature = "";
    std::string price2 = "";
    std::string signature2 = "";
    std::string price3 = "";
    std::string signature3 = "";

    if (!GetPriceInformation(price, signature, price2, signature2, price3, signature3)) {
        //fall back to old modell
        if (GetPriceInformationFromDifferentServers(price, signature, price2, signature2, price3, signature3) <= 0) {
            return 0;
        }
    }

    if (IsHex(price)) { 
        std::vector<unsigned char> txData(ParseHex(price));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> pblock->nPriceInfo;
        } catch (const std::exception&) {
            // Fall through.
            return false;
        }    
    } else return false;

    pblock->priceSig = ParseHex(signature);

    if (IsHex(price2)) { 
        std::vector<unsigned char> txData(ParseHex(price2));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> pblock->nPriceInfo2;
        } catch (const std::exception&) {
            // Fall through.
            return false;
        }    
    } else return false;

    pblock->priceSig2 = ParseHex(signature2);

    if (IsHex(price3)) { 
        std::vector<unsigned char> txData(ParseHex(price3));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> pblock->nPriceInfo3;
        } catch (const std::exception&) {
            // Fall through.
            return false;
        }    
    } else return false;

    pblock->priceSig3 = ParseHex(signature3);
    return true;
}

CAmount GetCachedPriceInformation(uint64_t cachetime, CAmount &secondpricereturn, CAmount &thirdpricereturn)
{
    std::string price, signature;
    CAmount secondprice = 0;
    CAmount thirdprice = 0;
    if (!haspriceinfo || GetTimeMillis() > pricetime + cachetime) {        
        CAmount tempprice = GetOnePriceInformation(price, signature, secondprice, thirdprice);
        if (tempprice != 0) {
      	    pricecache = tempprice;
            pricecache2 = secondprice;
            pricecache3 = thirdprice;
            pricetime = GetTimeMillis();        
            haspriceinfo = true;
        }
    }
    if (!haspriceinfo) {
        pricecache = GetBlockPrice(0);
        secondpricereturn = GetBlockPrice(1);
        thirdpricereturn = GetBlockPrice(2);
    }
    secondpricereturn = pricecache2;
    thirdpricereturn = pricecache3;
    return pricecache;
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature witness (in case segwit transactions are added to mempool before
//   segwit activation)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    for (const CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && it->GetTx().HasWitness())
            return false;
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
        indexed_modified_transaction_set &mapModifiedTx)
{
    int nDescendantsUpdated = 0;
    for (const CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc))
                continue;
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCostWithAncestors -= it->GetSigOpCost();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
    return nDescendantsUpdated;
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    return mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it);
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty())
    {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
                SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        bool isnick = iter->IsNicknameTx();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
            isnick = modit->IsNicknameTx();
        }        

        if (!isnick && packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, sortedEntries);

        for (size_t i=0; i<sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

static bool ProcessBlockFound(const CBlock* pblock, const CChainParams& chainparams)
{
    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BitCashMiner: generated block is stale");
    }

    // Inform about the new block
//    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
    if (!ProcessNewBlock(chainparams, shared_pblock, true, nullptr, false))
        return error("BitCashMiner: ProcessNewBlock, block not accepted");

    return true;
}

struct MinerContext {
    std::atomic<bool>& alive;
    int pow_threads;
    int threads_number;
    int nonces_per_thread;
    const CChainParams& chainparams;
    interfaces::Wallet* iwallet;
    CWallet* wallet;
    bool useinterface;
    ctpl::thread_pool* pool;
    int gpuid;
    bool selectgpucpu;
    bool gpumining;
};

bool mineriswaitingforblockdownload = false;
void MinerWorker(int thread_id, MinerContext& ctx)
{
    auto start_nonce = thread_id * ctx.nonces_per_thread;
    unsigned int nExtraNonce = 0;
    while (ctx.alive) {

        if (ctx.chainparams.MiningRequiresPeers()) {
            // Busy-wait for the network to come online so we don't waste
            // time mining n an obsolete chain. In regtest mode we expect
            // to fly solo.
            if (!g_connman) {
                throw std::runtime_error(
                        "Peer-to-peer functionality missing or disabled");
            }

            mineriswaitingforblockdownload = true;
            do {
                bool fvNodesEmpty =
                    g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0;

                if (!fvNodesEmpty && !IsInitialBlockDownload())
                    break;

                g_connman->ResetMiningStats();
                MilliSleep(1000);
            } while (ctx.alive);
            mineriswaitingforblockdownload = false;
        }

        if(g_connman) {
            g_connman->InitMiningStats();
        }

        //
        // Create new block
        //
        unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
        CBlockIndex* pindexPrev = chainActive.Tip();

        std::unique_ptr<CBlockTemplate> pblocktemplate{
            BlockAssembler(Params()).CreateNewBlock(ctx.iwallet,ctx.wallet,ctx.useinterface)};

        if (!pblocktemplate.get()) {
            LogPrintf(
                    "Error in BitCash Miner: Keypool ran out, please call "
                    "keypoolrefill before restarting the mining thread\n");
            return;
        }

        CBlock* pblock = &pblocktemplate->block;
        assert(pblock);

        pblock->nNonce = start_nonce;
        IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

/*        LogPrintf(
                "%d: Running BitCash Miner with %u transactions "
                "in block (%u bytes)\n",
            thread_id,
            pblock->vtx.size(),
            ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));*/

        //
        // Search
        //
        int64_t nStart = GetTimeMillis();
        int graphs_checked = 0;
        int cycles_found = 0;
        arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
        uint256 hash;
        std::set<uint32_t> cycle;

        // Check if something found
        graphs_checked++;
        bool cycle_found = false;           

        unsigned int nMaxTries = 1000;
        while (nMaxTries > 0 && ctx.alive) {                    
            cycles_found++;
            if (CheckProofOfWork(pblock->GetHash(), pblock->nBits, Params().GetConsensus()))
            {                        
                LogPrintf("%d: BitCash Miner:\n", thread_id);
                LogPrintf(
                    "\n\n\nproof-of-work found within %8.3f seconds \n"
                    "\tblock hash: %s\n\tnonce: %d\n\ttarget: %s\n\n\n",
                static_cast<double>(GetTimeMillis() - nStart) / 1e3,
                pblock->GetHash().GetHex(),
                pblock->nNonce,
                hashTarget.GetHex());                    

                if (ProcessBlockFound(pblock, ctx.chainparams)) minedcoins+=19350000000;

                // In regression test mode, stop mining after a block is found.
                if (ctx.chainparams.MineBlocksOnDemand())
                    throw boost::thread_interrupted();

                break;
            }
            graphs_checked++;
            ++pblock->nNonce;
            --nMaxTries;
            if (pblock->nNonce % ctx.nonces_per_thread == 0) {
                pblock->nNonce += ctx.nonces_per_thread * (ctx.threads_number - 1);
            }
            triedoneproofofwork = true;

            if(cycle_found) {
                cycles_found++;
            }

            if (ctx.alive && g_connman) {
                g_connman->AddCheckedGraphs(graphs_checked);
                graphs_checked=0;
                g_connman->AddFoundCycles(cycles_found);
                cycles_found=0;          
            }


            // Check for stop or if block needs to be rebuilt
            if (!ctx.alive) {
                break;
            }

            // Regtest mode doesn't require peers
            if ((!g_connman || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0) &&
                    ctx.chainparams.MiningRequiresPeers()) {
                break;
            }

            if (pblock->nNonce >= MAX_NONCE) {
               break;
            }

            if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast &&
                    (GetTimeMillis() - nStart) / 1e3 > /*ctx.chainparams.MininBlockStaleTime()*/35) {
                break;
            }

            if (pindexPrev != chainActive.Tip()) {
                LogPrintf("%d: Active chain tip changed. Breaking block lookup\n", thread_id);
                break;
            }

            addednewpriceinfo = false;
            // Update nTime every few seconds
            if (UpdateTime(pblock, ctx.chainparams.GetConsensus(), pindexPrev) < 0) {
                // Recreate the block if the clock has run backwards,
                // so that we can use the correct time.
                break;
            }

            if (addednewpriceinfo) {
                CHashWriter ss(SER_GETHASH, 0);
                    ss << pblock->nPriceInfo;
                    ss << pblock->priceSig;
                    ss << pblock->nPriceInfo2;
                    ss << pblock->priceSig2;
                    ss << pblock->nPriceInfo3;
                    ss << pblock->priceSig3;

                CMutableTransaction tx(*pblock->vtx[0]);
                tx.hashforpriceinfo = ss.GetHash();
	            pblock->vtx[0] = MakeTransactionRef(std::move(tx));
            }

            if (ctx.chainparams.GetConsensus().fPowAllowMinDifficultyBlocks) {
                // Changing pblock->nTime can change work required on testnet:
                hashTarget.SetCompact(pblock->nBits);
            }

            pblock->nNonce++;

            if (pblock->nNonce % ctx.nonces_per_thread == 0) {
                pblock->nNonce += ctx.nonces_per_thread * (ctx.threads_number - 1);
            }
        }

        if (ctx.alive && g_connman) {
            g_connman->AddCheckedGraphs(graphs_checked);
            g_connman->AddFoundCycles(cycles_found);
          
        }
    }

    LogPrintf("BitCash Miner pool #%d terminated\n", thread_id);
}

bool isgpumining= true;

void static BitCashMiner(
        interfaces::Wallet* iwallet, 
        CWallet* wallet, 
        bool useinterface,
        const CChainParams& chainparams,
        int pow_threads,
        int bucket_size,
        int bucket_threads,        
        bool selectgpucpu,
        int gpuid
        )
{
//    assert(coinbase_script);
    RenameThread("bitcash-miner");

    if (bucket_threads < 1) {
        bucket_threads = 1;
    }

    if (bucket_size == 0) {
        bucket_size = MAX_NONCE / bucket_threads;
    }

    ctpl::thread_pool pool(bucket_threads + bucket_threads * pow_threads);
    std::atomic<bool> alive{true};

    try {
        LogPrintf("Running BitCash Miner with %d pow threads, %d nonces per bucket and %d buckets in parallel.\n", pow_threads, bucket_size, bucket_threads);

        for (int t = 0; t < bucket_threads; t++) {
            MinerContext ctx{
                alive,
                pow_threads,
                bucket_threads,
                bucket_size,
                chainparams,
                iwallet,
                wallet,
                useinterface,
                &pool,
                gpuid+t,
                selectgpucpu,
                isgpumining
            };

            pool.push(MinerWorker, ctx);
        }

        while (true) {
            boost::this_thread::interruption_point();
        }
    } catch (const boost::thread_interrupted&) {
        LogPrintf("BitCash Miner terminated\n");
        alive = false;
        pool.stop();

        throw;
    } catch (const std::runtime_error& e) {
        LogPrintf("BitCash Miner runtime error: %s\n", e.what());
        gArgs.ForceSetArg("-mine", 0);
        pool.stop();

        return;
    }
}

void GenerateBitCash(interfaces::Wallet* iwallet, CWallet* wallet, bool useinterface, bool mine, int pow_threads, int bucket_size, int bucket_threads, const CChainParams& chainparams, int gpuid, bool selectgpucpu, bool gpumining)
{
    static boost::thread* minerThread = nullptr;

    triedoneproofofwork = false;

    if (pow_threads < 0) {
        if (trygpumining) {
            //1 Thread for GPU mining
            pow_threads = 1;
            if (bucket_threads<=0) bucket_threads = 1;
        } else {
            pow_threads = std::thread::hardware_concurrency() / 2;
            bucket_threads = 2;
        }
    }

    if (minerThread != nullptr) {
        minerThread->interrupt();
        delete minerThread;
        minerThread = nullptr;
    }

    if (pow_threads == 0 || bucket_threads == 0 || !mine) {
        if(g_connman) {
            g_connman->ResetMiningStats();
        }
        return;
    }

        gpuminingfailed = false;
        isgpumining = gpumining;
        minerThread = new boost::thread(
            &BitCashMiner,
            iwallet,
            wallet,
            useinterface,
            chainparams,
            pow_threads,
            bucket_size,
            bucket_threads, selectgpucpu, gpuid);
}


