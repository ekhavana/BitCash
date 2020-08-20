// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include "cuckoo/cuckoo.h"
#include "cuckoo/miner.h"

#include <streams.h>
#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>
#include "core_io.h"

#include <assert.h>

#include <chainparamsseeds.h>
//#include <boost/filesystem.hpp>
//#include <boost/filesystem/fstream.hpp>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, uint8_t nEdgeBits, int32_t nVersion, const CAmount& genesisReward,const char *priceinfo, const char*signature)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vin[0].isnickname=false;
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].nValueBitCash = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nEdgeBits = nEdgeBits;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));

    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);

/*    genesis.nPriceInfo.priceTime=0;
    genesis.nPriceInfo.priceCount=1;
    genesis.nPriceInfo.prices[0]=0;
    genesis.priceSig.clear();*/

    if (IsHex(priceinfo)) { 
        std::vector<unsigned char> txData(ParseHex(priceinfo));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> genesis.nPriceInfo;
        } catch (const std::exception&) {
            // Fall through.
        }    
    }
    genesis.priceSig = DecodeBase64(signature);

    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, uint8_t nEdgeBits, int32_t nVersion, const CAmount& genesisReward,const char *priceinfo, const char*signature)
{
    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nEdgeBits, nVersion, genesisReward, priceinfo, signature);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

std::string GetCycleStr(std::set<uint32_t> cycle);

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 4200000;
        consensus.nSubsidyFirstInterval = 1065535;
//        consensus.BIP16Exception = uint256S("0x00000000000002dc756eebf4f49723ed8d30cc28a5f108eb94b1ba88ac4f9c22");
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1; 
        consensus.BIP66Height = 1; 
//        consensus.sEdgeBitsAllowed = {16,17,18,19,20,21,22,23,24,25,26, 27, 28, 29, 30, 31};
        consensus.sEdgeBitsAllowed = {27};
        consensus.powLimit = Consensus::PoWLimit{
            uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),
            *consensus.sEdgeBitsAllowed.begin()};
        consensus.nPowTargetTimespan = 24 * 60 * 60; // one day for nBits adjustment->leftover from BitCoin->without any meaning for BitCash

        consensus.nPowTargetSpacing = 1 * 60;
        consensus.nEdgeBitsTargetThreshold = 4;      // adjust nEdgeBits if block time is 4x more/less than expected
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.nCuckooProofSize = 42;
        consensus.X16RTIME = 1550923200;//Time of X16R fork
        consensus.NONPRIVACY = 1552176000;//Time of nonprivacy
        consensus.STABLETIME = 1561204800;//Time of Stable coin fork
        consensus.MASTERKEYDUMMY = 1563876000;//Time of removal of Master key
        consensus.X16RV2TIME = 1569664800;//Time of X16RV2 fork
        consensus.GPUMINERTIME = 1570356000;//Time of fork which activates the GPU miner again
        consensus.GOLDTIME = 1573984800;//Time of BitCash gold activation
        consensus.X25XTIME = 1583056800;//Time of X25X fork
        consensus.DEACTIVATEDOLLAR = 1593331200;//Time of fork to deactivate new Dollar transactions
        consensus.DEACTIVATEPRICESERVERS = 1598083200;//Time of fork to deactivate the price servers
 

        // The best chain should have at least this much work.                                                
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00"); //506067

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xf8;
        pchMessageStart[1] = 0xc2;
        pchMessageStart[2] = 0xb4;
        pchMessageStart[3] = 0xe3;
        nDefaultPort = 5723;
        nPruneAfterHeight = 100000;

//        genesis = CreateGenesisBlock(1526220501, 2083236893, 0x1f00ffff, 1, 50 * COIN);
/*        genesis = CreateGenesisBlock(1231006505, 2083236893, 0x1f00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));*/

        genesis = CreateGenesisBlock(1531846964, 0x2e, 0x207fffff, 27, 1, 21500 * MILLICOIN,
/*"6d54335c0100ca9a3b00000000",
   "3045022100abcb23f82ef8904c8dad6825756ab6c98b939af098edc92dd5c66f5cc454dcc40220564c44c065110e52c50b1431bbfe6b77ea487424990552f748304299aa5054d7");*/
 
/*"9752335c0100ca9a3b00000000",
  "MEUCIQC/Gg/srvd/Bv6G7xB500a8sIbYbM1B6pjaeb4IIEzfqAIgEOiIwJ09GubGqcMsPS0HGoe2x3UA7WRFPBOZSJFd0ZM=");*/

"a34b335c0124bd7c0100000000", "MEQCIEiAA0H7qfQ0Y5A9vHP96pUz8SLQXxW1vsMf+39Wrs+6AiA56R3hoKHjlfivLWtu2QXXW7A6dCxLeBrKjnUqEvfnig==");


// ******* BEGIN GENERATE GENESIS BLOCK ********
/*
        genesis.nNonce=0;
        bool cycle_found = false;
        std::set<uint32_t> cycle;
        ctpl::thread_pool pool{1};
        int nMaxTries=10000;
        bool trygpumining=false;
        bool gpuminingfailed;
        static const int nInnerLoopCount = 0x10000;
            std::cout << "Start loop " << std::endl;
        while (nMaxTries > 0
                && genesis.nNonce < nInnerLoopCount
                && !cuckoo::FindProofOfWorkAdvanced(
                    genesis.GetHash(),
                    genesis.nBits,
                    genesis.nEdgeBits,
                    cycle,
                    consensus,
                    1,
                    cycle_found,
                    &pool)) {

            std::cout << "One loop " << std::endl;
            ++genesis.nNonce;
            --nMaxTries;
        }

        if (cycle_found)
        std::cout << "cycle found " << std::endl;else
        std::cout << "cycle not found " << std::endl;

    std::stringstream cycleStr;
    auto it = cycle.begin();

    while (it != cycle.end()) {
        std::cout << "0x" << std::hex << *it;
        if(++it != cycle.end()) {
	    std::cout << ", ";
        }
    } ;

//    return cycleStr.str();
        std::cout << std::endl << "Genesis Block Nonce: 0x" << genesis.nNonce << std::endl;
        genesis.sCycle = cycle;

        std::cout << "nCuckooProofSize:" << genesis.sCycle.size() << std::endl;
        consensus.hashGenesisBlock = genesis.GetHash();

if (cuckoo::VerifyProofOfWork(genesis.GetHash(), genesis.nBits, genesis.nEdgeBits, genesis.sCycle, consensus))
        std::cout << "Verfified Generate" << std::endl;else
        std::cout << "Not verfified" << std::endl;
        std::cout << "bitcash new hashMerkleRoot hash: " << genesis.hashMerkleRoot.ToString() << std::endl;
        std::cout << std::endl<< "bitcash new hashGenesisBlock hash: " << genesis.GetHash().ToString() << std::endl;
*/
// ****** END GENERATE GENESIS BLOCK *******
/*
copy cycles
copy nonce
copy hash
copy merkle root
*/
          genesis.sCycle = {
0x57009b, 0x7cefa0, 0xb9b28d, 0xc72ca1, 0xe16c3f, 0xf896d6, 0x10d06c1, 0x1111b27, 0x124fc44, 0x173981c, 0x193f487, 0x198020e, 0x199dc9c, 0x1b2b177, 0x1cf746a, 0x1dc2619, 0x2a76ba7, 0x2d0f91d, 0x2d4a01b, 0x2ef1e92, 0x31f4be6, 0x3b98dde, 0x3c1cca7, 0x3df56e6, 0x3f01460, 0x43b1c72, 0x47353d1, 0x4766e32, 0x4b0bf11, 0x549081c, 0x57b2165, 0x5cc8228, 0x60b4492, 0x6471b3b, 0x6d038e3, 0x6d99555, 0x6f33761, 0x73eb599, 0x7680b1e, 0x7c9da1e, 0x7d2eb1f, 0x7e194cc
          };

        consensus.hashGenesisBlock = genesis.GetHash();

  /*if (cuckoo::VerifyProofOfWork(genesis.GetHash(), genesis.nBits, genesis.nEdgeBits, genesis.sCycle, consensus))
        std::cout << "Verfified" << std::endl;else
        std::cout << "Not verfified" << std::endl;

        std::cout << "Genesis block: " << genesis.ToString() << std::endl;*/
/*
        std::cout << "bitcash new hashMerkleRoot hash: " << genesis.hashMerkleRoot.ToString() << std::endl;
        std::cout << "Genesis Block Nonce: 0x" << genesis.nNonce << std::endl;
        std::cout << std::endl<< "bitcash new hashGenesisBlock hash: " << genesis.GetHash().ToString() << std::endl;
*/
//        std::cout << std::endl<< "bitcash new hashGenesisBlock hash: " << genesis.GetHash().ToString() << std::endl;
                                                         
        assert(consensus.hashGenesisBlock == uint256S("0x7d57d87ff3c15a521530af60edee1887fba9c193eb518face926785c4cd8f4f1"));
        assert(genesis.hashMerkleRoot == uint256S("0xaac1dca23e43e68fe32133292a4698f42f60a48e5321bef9bda0c95f3580b667"));


        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("dnsseed.choosebitcash.com");
        vSeeds.emplace_back("dnsseed2.choosebitcash.com");  

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,230);//new BitCash address format
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,235);
        base58Prefixes[PUBKEY_ADDRESSNONPRIVATE] = std::vector<unsigned char>(1,130);//format for exchanges, mobile and lite wallet to receive without stealth addresses
        base58Prefixes[SCRIPT_ADDRESSNONPRIVATE] = std::vector<unsigned char>(1,135);
        base58Prefixes[PUBKEY_ADDRESSVIEWKEY] = std::vector<unsigned char>(1,156);//format for mobile and lite wallet with a viewkey
        base58Prefixes[PUBKEY_ADDRESSTREZOR] = std::vector<unsigned char>(1,0);//old BitCash address format = Bitcoin format
        base58Prefixes[SCRIPT_ADDRESSTREZOR] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,60+128);
        base58Prefixes[SECRET_KEYBTC] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "bc";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        //checkpoints are the merkle roots of these blocks
        checkpointData = {
            {
                { 0, uint256S("0x7d57d87ff3c15a521530af60edee1887fba9c193eb518face926785c4cd8f4f1")},
                { 5000, uint256S("0x3220c952ad933f932c74c8a9ac1a90c2c67544f2884ffe2cb501b3d3bbafb1cc")},
                { 10000, uint256S("0x2c2e1fda318f953d4490d43045eb5027c46ee263efd32af10b129b3849533766")},
                { 20000, uint256S("0x7f6c97894c2b98c963fa879a546047545e213b56e95766f9ba06e7fdc50461bb")},
                { 30000, uint256S("0x3ca6f893ba77f94bcd7880828c80e25d8acae947091a8c703d4bbc888652a763")},
                { 40000, uint256S("0x1509b6323cf48a74838ee3f7b2cca7e02f5c44ef78fe9e9b2133eeb94f92a26a")},
        		{ 70000, uint256S("0xd7b3f34e0a7c5cf1b781dd8ce38f146a40bf58074fa7f0977137ac5397b215b4")},
		        { 100000, uint256S("0x4dcc1d2efb17b25b57d1d9430751b809338c68db9164fb09f221daf8e1ca26ce")},
                { 130000, uint256S("0xc00a7ab6ca796be8757246a353436031908943b1108dac52879e0506b09c3be0")},
                { 200000, uint256S("0x5cf251696d27e34b8b818b00b5613d2ca662677f924423872b58e88fafdd8b29")},
                { 250000, uint256S("0xa883a07a7dbae7286fc664edfe488c433325003efcdfd803f09e6a30021dd449")},
                { 300000, uint256S("0x6fc9ac59a21b8722df1bd650c05cff490d8e2d39814b1ea0bc9bc4892f3e99c8")},
                { 400000, uint256S("0xb4073b529c85feef2c7ba96eeaa33e06762944fb03b8eff7271df3370be1c651")},
                { 460000, uint256S("0xfccfad82c82e174d6ba6b384ee867883d52e2cc99a59a62f538139cee5df6af4")},
                { 600000, uint256S("0x270e3019eaf69f2dd266951394462c74f8ab8d23f661708771ae75ce76d80000")},
                { 700000, uint256S("0xf693e62650cc45cc4b6ec31d7455675209679494c0c2447db2d2b96f524fa103")},
                { 780000, uint256S("0xbb211ea277b37e80ea293757f7b14e1320fcbfd6279d8b6258f41bea2d9a6865")},
                { 899785, uint256S("0xf6a438ffa4b0391f16fc0caa29a0eb8aba4655fd92fec97241c8034d512ebf99")},
                { 899810, uint256S("0x8e8857c8934ba95117e3e6a54fb35d427ebcc5e205cd6fcdc2eea16c64f20473")},

            }
        };

        chainTxData = ChainTxData{
            // Data as of block 0000000000000000002d6cca6761c99b3c2e936f9a0e304b7c7651a993f461de (height 506081).
            1531846964, // * UNIX timestamp of last known number of transactions
            0,  // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the ChainStateFlushed debug.log lines)
            0.02         // * estimated number of transactions per second after that timestamp
        };

        /* disable fallback fee on mainnet */
        m_fallback_fee_enabled = true;
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 4200000;
        consensus.nSubsidyFirstInterval = 1065535;
//        consensus.BIP16Exception = uint256S("0x00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105");
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1; 
        consensus.BIP66Height = 1; 
        consensus.sEdgeBitsAllowed = {16,17,18,19,20, 21, 22, 23, 24, 25, 26};
        consensus.powLimit = Consensus::PoWLimit{

            uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),
            *consensus.sEdgeBitsAllowed.begin()};
        consensus.nPowTargetTimespan = 24 * 60 * 60; // one for nBits adjustment
        consensus.nEdgeBitsTargetThreshold = 4;      // adjust nEdgeBits if block time is twice more/less than expected
        consensus.nPowTargetSpacing = 1;//1 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.nCuckooProofSize = 42;
        consensus.X16RTIME = 1549108800;//Time of X16R fork
        consensus.NONPRIVACY = 1552176000;//Time of nonprivacy
        consensus.STABLETIME = 1559302200;//Time of Stable coin fork
        consensus.MASTERKEYDUMMY = 1561992300;//Time of removal of Master key
        consensus.X16RV2TIME = 1567776051;//Time of X16RV2 fork
        consensus.GPUMINERTIME = 1569872820;//Time of fork which activates the GPU miner again
        consensus.GOLDTIME = 1573984800;//Time of BitCash gold activation
        consensus.X25XTIME = 1581238003;//Time of X25X fork
        consensus.DEACTIVATEDOLLAR = 1592915126;//Time of fork to deactivate new Dollar transactions 
        consensus.DEACTIVATEPRICESERVERS = 1596702228;//Time of fork to deactivate the price servers

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00"); //1135275

        pchMessageStart[0] = 0x03;
        pchMessageStart[1] = 0x15;
        pchMessageStart[2] = 0x08;
        pchMessageStart[3] = 0x03;
        nDefaultPort = 15723;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1531846965, 0x1f, 0x207fffff, 16, 1, 21500 * MILLICOIN, "a34b335c0124bd7c0100000000", "MEQCIEiAA0H7qfQ0Y5A9vHP96pUz8SLQXxW1vsMf+39Wrs+6AiA56R3hoKHjlfivLWtu2QXXW7A6dCxLeBrKjnUqEvfnig==");

// ******* BEGIN GENERATE GENESIS BLOCK ********

/*        genesis.nNonce=0;
        bool cycle_found = false;
        std::set<uint32_t> cycle;
        ctpl::thread_pool pool{1};
        int nMaxTries=10000;
        static const int nInnerLoopCount = 0x10000;
            std::cout << "Start loop " << std::endl;
        while (nMaxTries > 0
                && genesis.nNonce < nInnerLoopCount
                && !cuckoo::FindProofOfWorkAdvanced(
                    genesis.GetHash(),
                    genesis.nBits,
                    genesis.nEdgeBits,
                    cycle,
                    consensus,
                    1,
                    cycle_found,
                    &pool)) {

            std::cout << "One loop " << std::endl;
            ++genesis.nNonce;
            --nMaxTries;
        }

        if (cycle_found)
        std::cout << "cycle found " << std::endl;else
        std::cout << "cycle not found " << std::endl;

    std::stringstream cycleStr;
    auto it = cycle.begin();

    while (it != cycle.end()) {
        std::cout << "0x" << std::hex << *it;
        if(++it != cycle.end()) {
	    std::cout << ", ";
        }
    } ;

//    return cycleStr.str();
        std::cout << std::endl << "Genesis Block Nonce: 0x" << genesis.nNonce << std::endl;
if         (genesis.nEdgeBits==16) 
        std::cout << "EdgeBits 16 " << std::endl;else
        std::cout << "EdgeBits NOT 16 " << std::endl;
        genesis.sCycle = cycle;

        std::cout << "nCuckooProofSize:" << genesis.sCycle.size() << std::endl;
        consensus.hashGenesisBlock = genesis.GetHash();

if (cuckoo::VerifyProofOfWork(genesis.GetHash(), genesis.nBits, genesis.nEdgeBits, genesis.sCycle, consensus))
        std::cout << "Verfified" << std::endl;else
        std::cout << "Not verfified" << std::endl;
        std::cout << "bitcash new hashMerkleRoot hash: " << genesis.hashMerkleRoot.ToString() << std::endl;
        std::cout << std::endl<< "bitcash new hashGenesisBlock hash: " << genesis.GetHash().ToString() << std::endl;
*/
// ****** END GENERATE GENESIS BLOCK ********

        genesis.sCycle = {
0xdf, 0x56e, 0xadc, 0xbd9, 0xc3b, 0xde4, 0x136a, 0x1a42, 0x1b98, 0x21f2, 0x2621, 0x2840, 0x2cdb, 0x3e02, 0x4b87, 0x560b, 0x57a0, 0x6027, 0x62cc, 0x6f8a, 0x75a9, 0x773a, 0x7cde, 0x8372, 0x8478, 0x8dda, 0x9f72, 0xa68b, 0xa6da, 0xae71, 0xb67f, 0xb973, 0xbad3, 0xbe06, 0xc0a5, 0xc171, 0xd185, 0xd39f, 0xde16, 0xe47c, 0xed87, 0xfe8b
        };

        consensus.hashGenesisBlock = genesis.GetHash();
//        std::cout << "TESTNET bitcash new hashGenesisBlock hash: " << genesis.GetHash().ToString() << std::endl;
/*
        std::cout << "bitcash new hashMerkleRoot hash: " << genesis.hashMerkleRoot.ToString() << std::endl;
        std::cout << "Genesis Block Nonce: 0x" << genesis.nNonce << std::endl;
        std::cout << std::endl<< "bitcash new hashGenesisBlock hash: " << genesis.GetHash().ToString() << std::endl;*/

        assert(consensus.hashGenesisBlock == uint256S("0x50d97c38f8a673c4758718d4900bef9192f0318a8aa9ee415eeb27c3c431ff43"));
        assert(genesis.hashMerkleRoot == uint256S("0xaac1dca23e43e68fe32133292a4698f42f60a48e5321bef9bda0c95f3580b667"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("testnet-seed.choosebitcash.com"); 

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[PUBKEY_ADDRESSNONPRIVATE] = std::vector<unsigned char>(1,55);//format for exchanges, mobile and lite wallet to receive without stealth addresses
        base58Prefixes[SCRIPT_ADDRESSNONPRIVATE] = std::vector<unsigned char>(1,60);
        base58Prefixes[PUBKEY_ADDRESSVIEWKEY] = std::vector<unsigned char>(1,66);//format for mobile and lite wallet with a viewkey
        base58Prefixes[PUBKEY_ADDRESSTREZOR] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESSTREZOR] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[SECRET_KEYBTC] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {0, uint256S("0x50d97c38f8a673c4758718d4900bef9192f0318a8aa9ee415eeb27c3c431ff43")},
            }
        };

        chainTxData = ChainTxData{
            // Data as of block 000000000000033cfa3c975eb83ecf2bb4aaedf68e6d279f6ed2b427c64caff9 (height 1260526)
            0,
            0,
            0
        };

        /* enable fallback fee on testnet */
        m_fallback_fee_enabled = true;
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nSubsidyFirstInterval = 1065535;
        consensus.sEdgeBitsAllowed = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
//        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1; 
        consensus.BIP66Height = 1; 
        consensus.powLimit = Consensus::PoWLimit{
            uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),
            *consensus.sEdgeBitsAllowed.begin()};
        consensus.nPowTargetTimespan = 60; // one minute for nBits adjustment
        consensus.nEdgeBitsTargetThreshold = 2;      // adjust nEdgeBits if block time is twice more/less than expected
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.nCuckooProofSize = 42;
        consensus.X16RTIME = 1549108800;//Time of X16R fork
        consensus.NONPRIVACY = 1551299320;//Time of nonprivacy
        consensus.STABLETIME = 1561204800;//Time of Stable coin fork
        consensus.MASTERKEYDUMMY = 1561992300;//Time of removal of Master key
        consensus.X16RV2TIME = 1567776051;//Time of X16RV2 fork
        consensus.GPUMINERTIME = 1569872820;//Time of fork which activates the GPU miner again
        consensus.GOLDTIME = 1573984800;//Time of BitCash gold activation
        consensus.X25XTIME = 1581238003;//Time of X25X fork
        consensus.DEACTIVATEDOLLAR = 1592915126;//Time of fork to deactivate new Dollar transactions 
        consensus.DEACTIVATEPRICESERVERS = 1596702228;//Time of fork to deactivate the price servers

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xf1;
        pchMessageStart[1] = 0xb3;
        pchMessageStart[2] = 0xd4;
        pchMessageStart[3] = 0xc3;
        nDefaultPort = 18534;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1546865679, 2, 0x207fffff, 24, 1, 21500 * MILLICOIN, "a34b335c0124bd7c0100000000", "MEQCIEiAA0H7qfQ0Y5A9vHP96pUz8SLQXxW1vsMf+39Wrs+6AiA56R3hoKHjlfivLWtu2QXXW7A6dCxLeBrKjnUqEvfnig==");
        consensus.hashGenesisBlock = genesis.GetHash();

//        std::cout << "REGTEST bitcash new hashGenesisBlock hash: " << genesis.GetHash().ToString() << std::endl;

        assert(consensus.hashGenesisBlock == uint256S("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[PUBKEY_ADDRESSNONPRIVATE] = std::vector<unsigned char>(1,55);//format for mobile and lite wallet to receive without stealth addresses
        base58Prefixes[SCRIPT_ADDRESSNONPRIVATE] = std::vector<unsigned char>(1,60);
        base58Prefixes[PUBKEY_ADDRESSVIEWKEY] = std::vector<unsigned char>(1,66);//format for mobile and lite wallet with a viewkey
        base58Prefixes[PUBKEY_ADDRESSTREZOR] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESSTREZOR] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[SECRET_KEYBTC] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bcrt";

        /* enable fallback fee on regtest */
        m_fallback_fee_enabled = true;
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

bool ExistParams() {
    return (globalChainParams != nullptr);
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
