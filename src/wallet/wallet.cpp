// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>

#include <boost/algorithm/string/case_conv.hpp> 
#include <rpc/blockchain.h>
#include <checkpoints.h>
#include <chain.h>
#include <wallet/coincontrol.h>
#include <consensus/consensus.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <fs.h>
#include <init.h>
#include <index/txindex.h>
#include <key.h>
#include <key_io.h>
#include <keystore.h>
#include <validation.h>
#include <miner.h>
#include <net.h>
#include <nicknames.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rpc/server.h>
#include <script/script.h>
#include <scheduler.h>
#include <timedata.h>
#include <txmempool.h>
#include <utilmoneystr.h>
#include <wallet/fees.h>

#include <algorithm>
#include <assert.h>
#include <future>

#include <boost/algorithm/string/replace.hpp>

#include <openssl/ecdsa.h>
#include <openssl/rand.h>
#include <openssl/obj_mac.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <base58.h>  

std::map<CScript, std::string> accountforscript;
extern std::map<CScript, CPubKey> stealthaddresses;
extern std::map<CScript, CPubKey> viewkeyforstealthaddresses;
extern std::map<std::string, std::string> reflines;

std::string MasterPrivatKey = "";//Insert Master Privat Key here and set hasMasterPrivatKey to true. getrawtransaction and decoderawtransaction will then return the decrpyted reference line and the real recipient
bool hasMasterPrivatKey = false;
bool hascheckedmasterkey = false;

std::string hexStr(unsigned char* data, int len)
{
    std::stringstream ss;
    ss << std::hex;
    for(int i=0;i<len;++i)
        ss << std::setw(2) << std::setfill('0') << (int)data[i];
    return ss.str();
}

/**
 * Create an 256 bit key and IV using the supplied key_data. salt can be added for taste.
 * Fills in the encryption and decryption ctx objects and returns 0 on success
 **/
int aes_init(unsigned char *key_data, int key_data_len, unsigned char *salt, EVP_CIPHER_CTX *e_ctx, 
             EVP_CIPHER_CTX *d_ctx)
{
  int i, nrounds = 5;
  unsigned char key[32], iv[32];
 
  /*
   * Gen key & IV for AES 256 CBC mode. A SHA256 digest is used to hash the supplied key material.
   * nrounds is the number of times the we hash the material. More rounds are more secure but
   * slower.
   */
  i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha256(), salt, key_data, key_data_len, nrounds, key, iv);
  if (i != 32) {
    printf("Key size is %d bits - should be 256 bits\n", i);
    return -1;
  }
 
  EVP_CIPHER_CTX_init(e_ctx);
  EVP_EncryptInit_ex(e_ctx, EVP_aes_256_cbc(), NULL, key, iv);
  EVP_CIPHER_CTX_init(d_ctx);
  EVP_DecryptInit_ex(d_ctx, EVP_aes_256_cbc(), NULL, key, iv);
 
  return 0;
}
 
/*
 * Encrypt *len bytes of data
 * All data going in & out is considered binary (unsigned char[])
 */
unsigned char *aes_encrypt(EVP_CIPHER_CTX *e, unsigned char *plaintext, int *len)
{
  /* max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1 bytes */
  int c_len = *len + AES_BLOCK_SIZE, f_len = 0;
  unsigned char *ciphertext = (unsigned char *)malloc(c_len);
 
  /* allows reusing of 'e' for multiple encryption cycles */
  EVP_EncryptInit_ex(e, NULL, NULL, NULL, NULL);
 
  /* update ciphertext, c_len is filled with the length of ciphertext generated,
    *len is the size of plaintext in bytes */
  EVP_EncryptUpdate(e, ciphertext, &c_len, plaintext, *len);
 
  /* update ciphertext with the final remaining bytes */
  EVP_EncryptFinal_ex(e, ciphertext+c_len, &f_len);
 
  *len = c_len + f_len;
  return ciphertext;
}
 
/*
 * Decrypt *len bytes of ciphertext
 */
unsigned char *aes_decrypt(EVP_CIPHER_CTX *e, unsigned char *ciphertext, int *len)
{
  /* plaintext will always be equal to or lesser than length of ciphertext*/
  int p_len = *len, f_len = 0;
  unsigned char *plaintext = (unsigned char *)malloc(p_len);
 
  EVP_DecryptInit_ex(e, NULL, NULL, NULL, NULL);
  EVP_DecryptUpdate(e, plaintext, &p_len, ciphertext, *len);
  EVP_DecryptFinal_ex(e, plaintext+p_len, &f_len);
 
  *len = p_len + f_len;
  return plaintext;
}
 
//Calculate Encryption Key for AES encryption from Private und Public Keys
std::string CWallet::CalculateEncryptionKey(CPubKey pubkey,CKey privkey) const
{
    std::string secretstr="";

    //convert private key to BIGNUM
    BIGNUM *priv_bn = BN_new();
    BN_bin2bn(privkey.begin(), privkey.size(), priv_bn);

    //convert public key to EC_KEY
    EC_KEY *pkey;
    pkey = EC_KEY_new_by_curve_name(NID_secp256k1);

    const unsigned char* pbegin = pubkey.begin();
    o2i_ECPublicKey(&pkey, &pbegin, pubkey.size());
    //convert public key to EC_POINT
    const EC_POINT *pub0=EC_KEY_get0_public_key(pkey);

    // create group
    EC_GROUP *ecgrp = EC_GROUP_new_by_curve_name( NID_secp256k1 );
 
    //calculate encryption key for the AES encryption
    EC_POINT *secret = EC_POINT_new( ecgrp );
    EC_POINT_mul( ecgrp, secret, NULL, pub0, priv_bn, NULL );

    BIGNUM *x = BN_new();
    BIGNUM *y = BN_new();
    if (EC_POINT_get_affine_coordinates_GFp(ecgrp, secret, x, y, NULL)) {
        secretstr=(std::string)BN_bn2hex(x)+(std::string)BN_bn2hex(y);
    }

    BN_free( x );BN_free( y );
    EC_GROUP_free( ecgrp ); BN_free( priv_bn ); EC_POINT_free( secret ); 
    EC_KEY_free(pkey);

    return secretstr;
}

std::string MasterPubKey        =   "0308f4619d0ecea2cbfe31cb0ae6d62979f2b2f10d60d8f64c7fee84fb634bda89";
std::string MasterPubKeyDummy   =  "0496b538e853519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a604f8141781e62294721166bf621e73a82cbf2342c858ee";
//public key from the transaction of block 1 of the Bitcoin blockchain (the assumtion is that nobody knows the private key for this public key)

std::string getmasterpubkeystr(bool usemasterkeydummy)
{
    if (usemasterkeydummy)
        return MasterPubKeyDummy;
    else
        return MasterPubKey;
}

//Calculate one time destination (stealth address)
CPubKey CalculateOnetimeDestPubKey(CPubKey pubkey,CKey privkey, bool iscreatetransaction, bool usemasterkeydummy)
{
    CPubKey onetime;

    CPubKey masterpubkey(ParseHex(getmasterpubkeystr(usemasterkeydummy)));

    //convert private key to BIGNUM
    BIGNUM *priv_bn = BN_new();
    BN_bin2bn(privkey.begin(), privkey.size(), priv_bn);

    //convert public key to EC_KEY
    EC_KEY *pkey;
    pkey = EC_KEY_new_by_curve_name(NID_secp256k1);

    const unsigned char* pbegin = pubkey.begin();
    o2i_ECPublicKey(&pkey, &pbegin, pubkey.size());
    //convert public key to EC_POINT
    const EC_POINT *pub0=EC_KEY_get0_public_key(pkey);

    //convert master public key to EC_KEY
    EC_KEY *mpkey;
    mpkey = EC_KEY_new_by_curve_name(NID_secp256k1);

    const unsigned char* mpbegin = masterpubkey.begin();
    o2i_ECPublicKey(&mpkey, &mpbegin, masterpubkey.size());
    //convert master public key to EC_POINT
    const EC_POINT *mpub0=EC_KEY_get0_public_key(mpkey);


    // create group
    EC_GROUP *ecgrp = EC_GROUP_new_by_curve_name( NID_secp256k1 );
 
    //calculate shared Secred 
    EC_POINT *secret = EC_POINT_new( ecgrp );
    EC_POINT_mul( ecgrp, secret, NULL, mpub0, priv_bn, NULL );

    //convert shared Secred to Bignum
    BIGNUM *secrectBN = BN_new();
    BN_CTX *ctx;
    ctx = BN_CTX_new();
    secrectBN = EC_POINT_point2bn(ecgrp, secret, POINT_CONVERSION_COMPRESSED, secrectBN, ctx);

    BN_mask_bits(secrectBN, 248);

    //Calculate one time pubkey
    EC_POINT *onet = EC_POINT_new( ecgrp );
//    EC_POINT_mul( ecgrp, onet, NULL, pub0, secrectBN, NULL );
    EC_POINT_mul( ecgrp, onet, secrectBN, NULL, NULL, NULL );
    EC_POINT_add(ecgrp, onet, onet, pub0, NULL);

    //set public key
    EC_KEY_set_conv_form(pkey, POINT_CONVERSION_COMPRESSED);
    EC_KEY_set_public_key(pkey,onet);   
    unsigned char* pbegino = (unsigned char*)onetime.begin();
    i2o_ECPublicKey(pkey, &pbegino);

    EC_POINT_free( onet );  
    BN_free( secrectBN );
    EC_GROUP_free( ecgrp ); BN_free( priv_bn ); EC_POINT_free( secret ); 
    EC_KEY_free(pkey);
    EC_KEY_free(mpkey);
    BN_CTX_free(ctx);

    return onetime;
}
 
//Calculate private Key to send the TxOut
CKey CalculateOnetimeDestPrivateKey(CKey privkeyfromAddress,CKey privkey, bool usemasterkeydummy)
{
    CKey onetime;

    CPubKey masterpubkey(ParseHex(getmasterpubkeystr(usemasterkeydummy)));

    //convert private key to BIGNUM
    BIGNUM *priv_bn = BN_new();
    BN_bin2bn(privkey.begin(), privkey.size(), priv_bn);

    //convert private key to BIGNUM
    BIGNUM *pri = BN_new();
    BN_bin2bn(privkeyfromAddress.begin(), privkeyfromAddress.size(), pri);

    //convert master public key to EC_KEY
    EC_KEY *mpkey;
    mpkey = EC_KEY_new_by_curve_name(NID_secp256k1);

    const unsigned char* mpbegin = masterpubkey.begin();
    o2i_ECPublicKey(&mpkey, &mpbegin, masterpubkey.size());
    //convert master public key to EC_POINT
    const EC_POINT *mpub0=EC_KEY_get0_public_key(mpkey);


    // create group
    EC_GROUP *ecgrp = EC_GROUP_new_by_curve_name( NID_secp256k1 );
 
    //calculate shared Secred 
    EC_POINT *secret = EC_POINT_new( ecgrp );
    EC_POINT_mul( ecgrp, secret, NULL, mpub0, priv_bn, NULL );

    //convert shared Secred to Bignum
    BIGNUM *secrectBN = BN_new();
    BN_CTX *ctx;
    ctx = BN_CTX_new();
    secrectBN = EC_POINT_point2bn(ecgrp, secret, POINT_CONVERSION_COMPRESSED, secrectBN, ctx);
    BN_mask_bits(secrectBN, 248);

    BN_add(pri, pri, secrectBN);
    char keystore[32];
    unsigned char *binary = (unsigned char*) &keystore;
    memset(keystore, 0, sizeof(keystore));

    binary+=(32-BN_num_bytes(pri));
    BN_bn2bin(pri, binary);
    std::vector<unsigned char> vec(keystore, keystore + 32);
    onetime.Set(vec.begin(),vec.end(),true);

    BN_free( pri );

    BN_free( secrectBN );
    EC_GROUP_free( ecgrp ); BN_free( priv_bn ); EC_POINT_free( secret ); 
    EC_KEY_free(mpkey);
    BN_CTX_free(ctx);

    return onetime;
}

//Decrypt Real Recipient with the help of the Master Private Key
CPubKey DecryptRealRecipient(CPubKey pubkey,CKey privkey,CPubKey onetimepubkey)
{
    CPubKey onetime;

    //convert private key to BIGNUM
    BIGNUM *priv_bn = BN_new();
    BN_bin2bn(privkey.begin(), privkey.size(), priv_bn);

    //convert public key to EC_KEY
    EC_KEY *pkey;
    pkey = EC_KEY_new_by_curve_name(NID_secp256k1);

    const unsigned char* pbegin = pubkey.begin();
    o2i_ECPublicKey(&pkey, &pbegin, pubkey.size());
    //convert public key to EC_POINT
    const EC_POINT *pub0=EC_KEY_get0_public_key(pkey);

    //convert public key to EC_KEY
    EC_KEY *pkey2;
    pkey2 = EC_KEY_new_by_curve_name(NID_secp256k1);

    pbegin = onetimepubkey.begin();
    o2i_ECPublicKey(&pkey2, &pbegin, onetimepubkey.size());
    //convert public key to EC_POINT
    const EC_POINT *otpub0=EC_KEY_get0_public_key(pkey2);


    // create group
    EC_GROUP *ecgrp = EC_GROUP_new_by_curve_name( NID_secp256k1 );
 
    //calculate shared Secred 
    EC_POINT *secret = EC_POINT_new( ecgrp );
    EC_POINT_mul( ecgrp, secret, NULL, pub0, priv_bn, NULL );

    //convert shared Secred to Bignum
    BIGNUM *secrectBN = BN_new();
    BN_CTX *ctx;
    ctx = BN_CTX_new();
    secrectBN = EC_POINT_point2bn(ecgrp, secret, POINT_CONVERSION_COMPRESSED, secrectBN, ctx);
    BN_mask_bits(secrectBN, 248);

    //Calculate real destination pubkey secret
    EC_POINT *onet3 = EC_POINT_new( ecgrp );
    EC_POINT_mul( ecgrp, onet3, secrectBN, NULL, NULL, NULL );

    EC_POINT_invert(ecgrp,onet3,NULL);
    EC_POINT_add(ecgrp, onet3, otpub0, onet3, NULL);
   

    //set public key
    EC_KEY_set_conv_form(pkey, POINT_CONVERSION_COMPRESSED);
    EC_KEY_set_public_key(pkey,onet3);   
    unsigned char* pbegino = (unsigned char*)onetime.begin();
    i2o_ECPublicKey(pkey, &pbegino);

    EC_POINT_free( onet3 );
    BN_free( secrectBN );
    EC_GROUP_free( ecgrp ); BN_free( priv_bn ); EC_POINT_free( secret ); 
    EC_KEY_free(pkey);
    EC_KEY_free(pkey2);
    BN_CTX_free(ctx);

    return onetime;
}

//encrypts the refline with the private key and public to calculate a shared secred; no length limit
std::string CWallet::EncryptRefLineTry(std::string referenceline, CPubKey pubkey, CKey privkey, bool saltisactive, unsigned int salt1, unsigned int salt2) const
{
    std::string key_data=CalculateEncryptionKey(pubkey,privkey);
    std::string outputline="";

    /* "opaque" encryption, decryption ctx structures that libcrypto uses to record
       status of enc/dec operations */
    EVP_CIPHER_CTX *en = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX *de = EVP_CIPHER_CTX_new();
 
    /* 8 bytes to salt the key_data during key generation. This is an example of
       compiled in salt. We just read the bit pattern created by these two 4 byte 
       integers on the stack as 64 bits of contigous salt material - 
       ofcourse this only works if sizeof(int) >= 4 */
    unsigned int salt[] = {54443, 54423};

    if (saltisactive) {
        salt[0] = salt1;
        salt[1] = salt2;
    }
 
    /* gen key and iv. init the cipher ctx object */
    if (aes_init((unsigned char*)key_data.c_str(), strlen(key_data.c_str()), (unsigned char *)&salt, en, de)) {
      printf("Couldn't initialize AES cipher\n");
      return outputline;
    }
 
    /* The enc/dec functions deal with binary data and not C strings. strlen() will 
       return length of the string without counting the '\0' string marker. We always
       pass in the marker byte to the encrypt/decrypt functions so that after decryption 
       we end up with a legal C string */
    int len=strlen(referenceline.c_str())+1;
    unsigned char *ciphertext;
 
    ciphertext = aes_encrypt(en, (unsigned char *)referenceline.c_str(), &len);
    outputline=EncodeBase58(ciphertext,ciphertext+len);

    free(ciphertext);

    EVP_CIPHER_CTX_free(en);
    EVP_CIPHER_CTX_free(de);
 
    return outputline;
}

//encrypts the refline with the private key and public to calculate a shared secred with length limit
std::string CWallet::EncryptRefLine(std::string referenceline, CPubKey pubkey, CKey privkey, bool saltisactive, unsigned int salt1, unsigned int salt2) const
{
   std::string outstr="";
   if (referenceline.length() <= 0) return referenceline;

   if (referenceline.length() > 1000) referenceline.resize(1000);
    do{
       outstr = EncryptRefLineTry(referenceline, pubkey, privkey, saltisactive, salt1, salt2);
       referenceline.resize(referenceline.length()-1);
    } while (outstr.length() > 1000);
    return outstr;
}

//decrypts the refline with the private key and public to calculate a shared secred
std::string CWallet::DecryptRefLine(std::string referenceline, CPubKey pubkey, CKey privkey, bool saltisactive, unsigned int salt1, unsigned int salt2) const
{
  
    if (referenceline.length() <= 0) return referenceline;

    std::string key_data=CalculateEncryptionKey(pubkey,privkey);
    std::string outputline="";

    /* "opaque" encryption, decryption ctx structures that libcrypto uses to record
       status of enc/dec operations */
    EVP_CIPHER_CTX *en = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX *de = EVP_CIPHER_CTX_new();

 
    /* 8 bytes to salt the key_data during key generation. This is an example of
       compiled in salt. We just read the bit pattern created by these two 4 byte 
       integers on the stack as 64 bits of contigous salt material - 
       ofcourse this only works if sizeof(int) >= 4 */
    unsigned int salt[] = {54443, 54423};

    if (saltisactive) {
        salt[0] = salt1;
        salt[1] = salt2;
    }
 
    /* gen key and iv. init the cipher ctx object */
    if (aes_init((unsigned char*)key_data.c_str(), strlen(key_data.c_str()), (unsigned char *)&salt, en, de)) {
      printf("Couldn't initialize AES cipher\n");
      return outputline;
    }
  
    std::vector<unsigned char> result;

    DecodeBase58(referenceline, result);

    unsigned char* ciphertext=(unsigned char*)&result[0];
    int len=result.size();

    outputline = (char*)aes_decrypt(de, ciphertext, &len);

    EVP_CIPHER_CTX_free(en);
    EVP_CIPHER_CTX_free(de);
 
    return outputline;

}

//decrypts a refline of a txout if we are the receiver of the transaction
std::string CWallet::DecryptRefLineTxOut(CTxOut out, bool saltisactive) const
{
    unsigned int salt1 = *(unsigned int*)&out.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&out.randomPubKey[5];

    if (reflines.count(out.referenceline) > 0) {
        return reflines.at(out.referenceline);
    }

    std::string outputline = out.referenceline;
    if (outputline.length()<=0) return outputline;

    CPubKey masterpubkey(ParseHex(getmasterpubkeystr(out.masterkeyisremoved)));

    CKey vchSecret;
    if (GetKey(out.randomPubKey.GetID(), vchSecret)){
        //Decrypt as the sender of the transaction
        outputline = DecryptRefLine(outputline, masterpubkey, vchSecret, saltisactive, salt1, salt2);
        SetRefLines(out.referenceline, outputline);
        return outputline;
    }

    CPubKey onetimedestpubkey;

    if (ExtractCompletePubKey(*this, out.scriptPubKey,onetimedestpubkey))
    {

        if (out.isnonprivate)
        {
            //Decrypt as sender of the transaction...
            CKey key;
            if (GetKey(onetimedestpubkey.GetID(), key)){
                char randprivkey[32];
                memcpy(&randprivkey,out.randomPrivatKey,32);
                DecryptPrivateKey((unsigned char*)&randprivkey, out.randomPubKey, key, saltisactive, salt1, salt2);

                std::vector<unsigned char> vec(randprivkey, randprivkey + 32);

                CKey privkey;
                privkey.Set(vec.begin(),vec.end(),true);
                    
                outputline = DecryptRefLine(outputline, masterpubkey, privkey, saltisactive, salt1, salt2);
                SetRefLines(out.referenceline, outputline);
            }
        } else
        {
            //Decrypt as receiver of the transaction...
            for (const std::pair<CTxDestination, CAddressBookData>& item : mapAddressBook) {
                if (GetNonPrivateForDestination(item.first)) continue;
                CPubKey pubkey=GetSecondPubKeyForDestination(item.first);
                CPubKey viewpubkey = GetViewPubKeyForDestination(item.first);
                bool hasviewkey = GetHasViewKeyForDestination(item.first);

                if (out.hasrecipientid) {
                    if (saltisactive) {
                        if (((out.recipientid1 != 0 || out.recipientid2 != 0) || (out.currencyisactive)) && (out.recipientid1 != pubkey[salt1 % 30 + 1] || out.recipientid2 != pubkey[salt2 % 30 + 1]))
                            continue;
                    } else {
                        if (((out.recipientid1 != 0 || out.recipientid2 != 0) || (out.currencyisactive)) && (out.recipientid1 != pubkey[10] || out.recipientid2 != pubkey[20])) continue;
                    }
                }

                CKey key;
                if (GetKey(pubkey.GetID(), key) || hasviewkey) {
                    char randprivkey[32];
                    memcpy(&randprivkey,out.randomPrivatKey,32);
                    if (hasviewkey) {
                        CKey viewkey;
                        if (!GetKey(viewpubkey.GetID(), viewkey)) {                            
                            //we do not store the view, so calculate it from the private key
                            viewkey = key.GetViewKeyForPrivateKey();
                        }
                        DecryptPrivateKey((unsigned char*)&randprivkey, out.randomPubKey, viewkey, saltisactive, salt1, salt2);
                    } else {
                        DecryptPrivateKey((unsigned char*)&randprivkey, out.randomPubKey, key, saltisactive, salt1, salt2);
                    }

                    std::vector<unsigned char> vec(randprivkey, randprivkey + 32);

                    CKey privkey;
                    privkey.Set(vec.begin(),vec.end(),true);
                    CPubKey destinationPubKey = CalculateOnetimeDestPubKey(pubkey, privkey, false, out.masterkeyisremoved);
                    if (onetimedestpubkey == destinationPubKey) {                 
                        outputline = DecryptRefLine(outputline, masterpubkey, privkey, saltisactive, salt1, salt2);
                        SetRefLines(out.referenceline, outputline);
                        break;          
                    }
                }
            }
        }  
    }

    return outputline;
}

//Gets the view key for transaction as sender
bool CWallet::GetViewKeyForAddressAsSender(CTxOut out, CKey& ViewKey) const
{
    if (GetKey(out.randomPubKey.GetID(), ViewKey)){
        return true;
    }
    return false;
}

//Decrypt real receiver address as sender
bool CWallet::GetRealAddressAsSender(CTxOut out, CPubKey& recipientpubkey, bool &hasviewkey, CPubKey &viewkey, bool saltisactive) const
{
    if (stealthaddresses.count(out.scriptPubKey) > 0) {
        recipientpubkey = stealthaddresses.at(out.scriptPubKey);
        if (viewkeyforstealthaddresses.count(out.scriptPubKey) > 0) {
            hasviewkey = true;
            viewkey = viewkeyforstealthaddresses.at(out.scriptPubKey);
        } else hasviewkey = false;

        return true;
    }

    CPubKey onetimedestpubkey;
    if (ExtractCompletePubKey(*this, out.scriptPubKey,onetimedestpubkey))
    {
        recipientpubkey=onetimedestpubkey;
        if (out.isnonprivate)
        {
            hasviewkey = false;
            return true;
        }

        CPubKey masterpubkey(ParseHex(getmasterpubkeystr(out.masterkeyisremoved)));

        CKey vchSecret;
        if (GetKey(out.randomPubKey.GetID(), vchSecret)){
           //Decrypt as the sender of the transaction    

            recipientpubkey = DecryptRealRecipient(masterpubkey,vchSecret,onetimedestpubkey);
            SetStealthAddress(out.scriptPubKey, recipientpubkey);
            if (viewkeyforstealthaddresses.count(out.scriptPubKey) > 0) {
                hasviewkey = true;
                viewkey = viewkeyforstealthaddresses.at(out.scriptPubKey);
            } else hasviewkey = false;
            return true;
        }
    }
    return false;
}

//Decrypt real receiver address as receiver
bool CWallet::GetRealAddressAsReceiver(CTxOut txout, CPubKey& recipientpubkey, bool &gethasviewkey, CPubKey &getviewkey, bool saltisactive) const
{
    unsigned int salt1 = *(unsigned int*)&txout.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&txout.randomPubKey[5];

    if (stealthaddresses.count(txout.scriptPubKey) > 0) {
        recipientpubkey = stealthaddresses.at(txout.scriptPubKey);
        if (viewkeyforstealthaddresses.count(txout.scriptPubKey) > 0) {
            gethasviewkey = true;
            getviewkey = viewkeyforstealthaddresses.at(txout.scriptPubKey);
        } else gethasviewkey = false;
        return true;
    }

    CPubKey onetimedestpubkey;

    if (ExtractCompletePubKey(*this, txout.scriptPubKey,onetimedestpubkey))
    {
        if (txout.isnonprivate)
        {
            gethasviewkey = false;
            recipientpubkey = onetimedestpubkey;
            return true;
        }

        for (const std::pair<CTxDestination, CAddressBookData>& item : mapAddressBook) {
            if (GetNonPrivateForDestination(item.first)) continue;
            CPubKey pubkey=GetSecondPubKeyForDestination(item.first);
            CPubKey viewpubkey = GetViewPubKeyForDestination(item.first);
            bool hasviewkey = GetHasViewKeyForDestination(item.first);

            if (txout.hasrecipientid) {
                if (saltisactive) {
                    if (((txout.recipientid1 != 0 || txout.recipientid2 != 0) || (txout.currencyisactive)) && (txout.recipientid1 != pubkey[salt1 % 30 + 1] || txout.recipientid2 != pubkey[salt2 % 30 + 1])) 
                        continue;
                } else {
                     if (((txout.recipientid1 != 0 || txout.recipientid2 != 0) || (txout.currencyisactive)) && (txout.recipientid1 != pubkey[10] || txout.recipientid2 != pubkey[20])) continue;
                }
            }

            CKey key;
            if (GetKey(pubkey.GetID(), key) || hasviewkey) {
                //std::cout << "have privkey; Pubkey in Address Book to check: " << HexStr(pubkey.begin(),pubkey.end()) << std::endl;

                char randprivkey[32];
                memcpy(&randprivkey,txout.randomPrivatKey,32);
                DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, key, saltisactive, salt1, salt2);

                CKey viewkey;
                if (hasviewkey) {
                    if (!GetKey(viewpubkey.GetID(), viewkey)) {                            
                        //we do not store the view, so calculate it from the private key
                        viewkey = key.GetViewKeyForPrivateKey();
                    }
                    DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, viewkey, saltisactive, salt1, salt2);
                } else {
                    DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, key, saltisactive, salt1, salt2);
                }
                std::vector<unsigned char> vec(randprivkey, randprivkey + 32);

                CKey privkey;
                privkey.Set(vec.begin(),vec.end(),true);
                CPubKey destinationPubKey = CalculateOnetimeDestPubKey(pubkey, privkey, false, txout.masterkeyisremoved);
                if (onetimedestpubkey == destinationPubKey) {
                    recipientpubkey = pubkey;
                    SetStealthAddress(txout.scriptPubKey, recipientpubkey);
                    if (hasviewkey) SetViewkeyStealthAddress(txout.scriptPubKey, viewpubkey);
                    gethasviewkey = hasviewkey;
                    getviewkey = viewpubkey;
                    accountforscript[txout.scriptPubKey] = item.second.name;
                    return true;
                }
            }
        }
    }
    return false;
}

//Decrypt the reference line if I know which private key belongs to the real (non stealth) receiver adddress 
std::string CWallet::DecryptRefLineTxOutWithOnePrivateKey(CTxOut out, CKey key, bool saltisactive) const
{
    unsigned int salt1 = *(unsigned int*)&out.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&out.randomPubKey[5];

    if (reflines.count(out.referenceline) > 0) {        
        return reflines.at(out.referenceline);
    }

    std::string outputline = out.referenceline;
    if (outputline.length() <=0 ) return outputline;

    CPubKey onetimedestpubkey;

    if (ExtractCompletePubKey(*this, out.scriptPubKey,onetimedestpubkey))
    {
        CPubKey pubkey = key.GetPubKey();

        if (out.hasrecipientid) {
           if (saltisactive) {
                if (((out.recipientid1 != 0 || out.recipientid2 != 0) || (out.currencyisactive)) && (out.recipientid1 != pubkey[salt1 % 30 + 1] || out.recipientid2 != pubkey[salt2 % 30 + 1])) {
                    return outputline;
                }
            } else {
                if (((out.recipientid1 != 0 || out.recipientid2 != 0) || (out.currencyisactive)) && (out.recipientid1 != pubkey[10] || out.recipientid2 != pubkey[20])) {
                    return outputline;
                }
            }
        }
        CPubKey masterpubkey(ParseHex(getmasterpubkeystr(out.masterkeyisremoved)));

        char randprivkey[32];
        memcpy(&randprivkey,out.randomPrivatKey,32);
        DecryptPrivateKey((unsigned char*)&randprivkey, out.randomPubKey, key, saltisactive, salt1, salt2);

        std::vector<unsigned char> vec(randprivkey, randprivkey + 32);
        
        CKey privkey;
        privkey.Set(vec.begin(),vec.end(),true);
        if (out.isnonprivate)
        {                
            if (key.VerifyPubKey(onetimedestpubkey)) {
                outputline = DecryptRefLine(outputline, masterpubkey, privkey, saltisactive, salt1, salt2);
                SetRefLines(out.referenceline, outputline);
            }
        } else
        {
            CPubKey destinationPubKey = CalculateOnetimeDestPubKey(pubkey, privkey, false, out.masterkeyisremoved);
            if (onetimedestpubkey == destinationPubKey) {
                outputline = DecryptRefLine(outputline, masterpubkey, privkey, saltisactive, salt1, salt2);
                SetRefLines(out.referenceline, outputline);
            }
        }
    }

    return outputline;
}

//Returns true if the txout has been received with the given private key, calculates the one time private key for the stealth address
bool CWallet::DoesTxOutBelongtoPrivKeyCalcOneTimePrivate(const CTxOut& txout, CKey key, CKey& otpk, bool saltisactive)
{
    unsigned int salt1 = *(unsigned int*)&txout.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&txout.randomPubKey[5];

    CPubKey onetimedestpubkey;
    
    if (ExtractCompletePubKey(*this, txout.scriptPubKey,onetimedestpubkey))
    {
        CPubKey pubkey=key.GetPubKey();

        if (txout.isnonprivate)
        {
            return key.VerifyPubKey(pubkey);
        }

        if (saltisactive) {
            if (txout.hasrecipientid) {
                if (((txout.recipientid1 != 0 || txout.recipientid2 != 0) || (txout.currencyisactive)) && (txout.recipientid1 != pubkey[salt1 % 30 + 1] || txout.recipientid2 != pubkey[salt2 % 30 + 1]))
                    return false;
            }
        } else {
            if (txout.hasrecipientid) {
                if (((txout.recipientid1 != 0 || txout.recipientid2 != 0) || (txout.currencyisactive)) && (txout.recipientid1 != pubkey[10] || txout.recipientid2 != pubkey[20])) 
                    return false;
            }
        }


        char randprivkey[32];
        memcpy(&randprivkey,txout.randomPrivatKey,32);
        DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, key, saltisactive, salt1, salt2);

        std::vector<unsigned char> vec(randprivkey, randprivkey + 32);

        CKey privkey;
        privkey.Set(vec.begin(),vec.end(),true);
        CPubKey destinationPubKey = CalculateOnetimeDestPubKey(pubkey, privkey, false, txout.masterkeyisremoved);
        if (onetimedestpubkey == destinationPubKey) {
            otpk = CalculateOnetimeDestPrivateKey(key, privkey, txout.masterkeyisremoved);
/*
            std::cout << "otpk: " << HexStr(otpk.begin(),otpk.end()) << std::endl;
            std::cout << "otpk pub: " << HexStr(otpk.GetPubKey().begin(),otpk.GetPubKey().end()) << std::endl;
            std::cout << "onetimedestpubkey: " << HexStr(onetimedestpubkey.begin(),onetimedestpubkey.end()) << std::endl;
  */
            if (otpk.IsValid())
            return true;
        } else {
            //Test if it is a address with a view key
            memcpy(&randprivkey,txout.randomPrivatKey,32);
            DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, key.GetViewKeyForPrivateKey(), saltisactive, salt1, salt2);

            std::vector<unsigned char> vec(randprivkey, randprivkey + 32);

            CKey privkey;
            privkey.Set(vec.begin(),vec.end(),true);
            CPubKey destinationPubKey = CalculateOnetimeDestPubKey(pubkey, privkey, false, txout.masterkeyisremoved);
            if (onetimedestpubkey == destinationPubKey) {
                otpk = CalculateOnetimeDestPrivateKey(key, privkey, txout.masterkeyisremoved);
    /*
                std::cout << "2.otpk: " << HexStr(otpk.begin(),otpk.end()) << std::endl;
                std::cout << "2.otpk pub: " << HexStr(otpk.GetPubKey().begin(),otpk.GetPubKey().end()) << std::endl;
                std::cout << "2.onetimedestpubkey: " << HexStr(onetimedestpubkey.begin(),onetimedestpubkey.end()) << std::endl;
 */
                if (otpk.IsValid())
                return true;
            }
        }
    }

    return false;
}

//encrypt private key using AES encryped with a shared secred
void CWallet::EncryptPrivateKey(unsigned char *privatekey, CPubKey pubkey, CKey privkey, bool saltisactive, unsigned int salt1, unsigned int salt2) const
{
    std::string key_data = CalculateEncryptionKey(pubkey,privkey);

    /* "opaque" encryption, decryption ctx structures that libcrypto uses to record
       status of enc/dec operations */
    EVP_CIPHER_CTX *en = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX *de = EVP_CIPHER_CTX_new();

 
    /* 8 bytes to salt the key_data during key generation. This is an example of
       compiled in salt. We just read the bit pattern created by these two 4 byte 
       integers on the stack as 64 bits of contigous salt material - 
       ofcourse this only works if sizeof(int) >= 4 */
    unsigned int salt[] = {54443, 54423};
    if (saltisactive) {
        salt[0] = salt1;
        salt[1] = salt2;
    }
 
    /* gen key and iv. init the cipher ctx object */
    if (aes_init((unsigned char*)key_data.c_str(), strlen(key_data.c_str()), (unsigned char *)&salt, en, de)) {
      printf("Couldn't initialize AES cipher\n");
      return;
    }
 
    /* The enc/dec functions deal with binary data and not C strings. strlen() will 
       return length of the string without counting the '\0' string marker. We always
       pass in the marker byte to the encrypt/decrypt functions so that after decryption 
       we end up with a legal C string */
    int len=32;
    unsigned char *ciphertext;
 
    ciphertext = aes_encrypt(en, privatekey, &len);
    memcpy(privatekey,ciphertext,32);

    free(ciphertext);

    EVP_CIPHER_CTX_free(en);
    EVP_CIPHER_CTX_free(de);
}

//decrypt private key using AES encryped with a shared secred
void CWallet::DecryptPrivateKey(unsigned char *privatekey, CPubKey pubkey, CKey privkey, bool saltisactive, unsigned int salt1, unsigned int salt2) const 
{
 
    std::string key_data = CalculateEncryptionKey(pubkey,privkey);

    /* "opaque" encryption, decryption ctx structures that libcrypto uses to record
       status of enc/dec operations */
    EVP_CIPHER_CTX *en = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX *de = EVP_CIPHER_CTX_new();

 
    /* 8 bytes to salt the key_data during key generation. This is an example of
       compiled in salt. We just read the bit pattern created by these two 4 byte 
       integers on the stack as 64 bits of contigous salt material - 
       ofcourse this only works if sizeof(int) >= 4 */
    unsigned int salt[] = {54443, 54423};

    if (saltisactive) {
        salt[0] = salt1;
        salt[1] = salt2;
    }
 
    /* gen key and iv. init the cipher ctx object */
    if (aes_init((unsigned char*)key_data.c_str(), strlen(key_data.c_str()), (unsigned char *)&salt, en, de)) {
      printf("Couldn't initialize AES cipher\n");
      return;
    }
  
    std::vector<unsigned char> result;

    int len=32;
    unsigned char *ciphertext;
 
    ciphertext = aes_decrypt(de, privatekey, &len);
                   
    memcpy(privatekey,ciphertext,32);

    free(ciphertext);

    EVP_CIPHER_CTX_free(en);
    EVP_CIPHER_CTX_free(de);
}

static CCriticalSection cs_wallets;
static std::vector<CWallet*> vpwallets GUARDED_BY(cs_wallets);

bool AddWallet(CWallet* wallet)
{
    LOCK(cs_wallets);
    assert(wallet);
    std::vector<CWallet*>::const_iterator i = std::find(vpwallets.begin(), vpwallets.end(), wallet);
    if (i != vpwallets.end()) return false;
    vpwallets.push_back(wallet);
    return true;
}

bool RemoveWallet(CWallet* wallet)
{
    LOCK(cs_wallets);
    assert(wallet);
    std::vector<CWallet*>::iterator i = std::find(vpwallets.begin(), vpwallets.end(), wallet);
    if (i == vpwallets.end()) return false;
    vpwallets.erase(i);
    return true;
}

bool HasWallets()
{
    LOCK(cs_wallets);
    return !vpwallets.empty();
}

std::vector<CWallet*> GetWallets()
{
    LOCK(cs_wallets);
    return vpwallets;
}

CWallet* GetWallet(const std::string& name)
{
    LOCK(cs_wallets);
    for (CWallet* wallet : vpwallets) {
        if (wallet->GetName() == name) return wallet;
    }
    return nullptr;
}

const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

const uint256 CMerkleTx::ABANDON_HASH(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

/** @defgroup mapWallet
 *
 * @{
 */

std::string COutput::ToString() const
{
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->tx->vout[i].nValue));
}

class CAffectedKeysVisitor : public boost::static_visitor<void> {
private:
    const CKeyStore &keystore;
    std::vector<CKeyID> &vKeys;

public:
    CAffectedKeysVisitor(const CKeyStore &keystoreIn, std::vector<CKeyID> &vKeysIn) : keystore(keystoreIn), vKeys(vKeysIn) {}

    void Process(const CScript &script) {
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (ExtractDestinations(script, type, vDest, nRequired)) {
            for (const CTxDestination &dest : vDest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const CKeyID &keyId) {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID &scriptId) {
        CScript script;
        if (keystore.GetCScript(scriptId, script))
            Process(script);
    }

    void operator()(const WitnessV0ScriptHash& scriptID)
    {
        CScriptID id;
        CRIPEMD160().Write(scriptID.begin(), 32).Finalize(id.begin());
        CScript script;
        if (keystore.GetCScript(id, script)) {
            Process(script);
        }
    }

    void operator()(const WitnessV0KeyHash& keyid)
    {
        CKeyID id(keyid);
        if (keystore.HaveKey(id)) {
            vKeys.push_back(id);
        }
    }

    template<typename X>
    void operator()(const X &none) {}
};

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return nullptr;
    return &(it->second);
}

CPubKey CWallet::GenerateNewKey(WalletBatch &batch, bool internal)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    CKey secret;

    // Create new metadata
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // use HD key derivation if HD was enabled during wallet creation
    if (IsHDEnabled()) {
        DeriveNewChildKey(batch, metadata, secret, (CanSupportFeature(FEATURE_HD_SPLIT) ? internal : false));
    } else {
        secret.MakeNewKey(fCompressed);
    }
    secret.DeleteFirstBit();//Need to limit range, so that addition of a 248 bit BN is possible without overflow

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed) {
        SetMinVersion(FEATURE_COMPRPUBKEY);
    }

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    mapKeyMetadata[pubkey.GetID()] = metadata;
    UpdateTimeFirstKey(nCreationTime);

    if (!AddKeyPubKeyWithDB(batch, secret, pubkey)) {
        throw std::runtime_error(std::string(__func__) + ": AddKey failed");
    }
    return pubkey;
}

void CWallet::DeriveNewChildKey(WalletBatch &batch, CKeyMetadata& metadata, CKey& secret, bool internal)
{
    // for now we use a fixed keypath scheme of m/0'/0'/k
    CKey key;                      //master key seed (256bit)
    CExtKey masterKey;             //hd master key
    CExtKey accountKey;            //key at m/0'
    CExtKey chainChildKey;         //key at m/0'/0' (external) or m/0'/1' (internal)
    CExtKey childKey;              //key at m/0'/0'/<n>'

    // try to get the master key
    if (!GetKey(hdChain.masterKeyID, key))
        throw std::runtime_error(std::string(__func__) + ": Master key not found");

    masterKey.SetMaster(key.begin(), key.size());

    // derive m/0'
    // use hardened derivation (child keys >= 0x80000000 are hardened after bip32)
    masterKey.Derive(accountKey, BIP32_HARDENED_KEY_LIMIT);

    // derive m/0'/0' (external chain) OR m/0'/1' (internal chain)
    assert(internal ? CanSupportFeature(FEATURE_HD_SPLIT) : true);
    accountKey.Derive(chainChildKey, BIP32_HARDENED_KEY_LIMIT+(internal ? 1 : 0));

    // derive child key at next index, skip keys already known to the wallet
    do {
        // always derive hardened keys
        // childIndex | BIP32_HARDENED_KEY_LIMIT = derive childIndex in hardened child-index-range
        // example: 1 | BIP32_HARDENED_KEY_LIMIT == 0x80000001 == 2147483649
        if (internal) {
            chainChildKey.Derive(childKey, hdChain.nInternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            metadata.hdKeypath = "m/0'/1'/" + std::to_string(hdChain.nInternalChainCounter) + "'";
            hdChain.nInternalChainCounter++;
        }
        else {
            chainChildKey.Derive(childKey, hdChain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            metadata.hdKeypath = "m/0'/0'/" + std::to_string(hdChain.nExternalChainCounter) + "'";
            hdChain.nExternalChainCounter++;
        }
    } while (HaveKey(childKey.key.GetPubKey().GetID()));
    secret = childKey.key;
    metadata.hdMasterKeyID = hdChain.masterKeyID;
    // update the chain model in the database
    if (!batch.WriteHDChain(hdChain))
        throw std::runtime_error(std::string(__func__) + ": Writing HD chain model failed");
}

bool CWallet::AddKeyPubKeyWithDB(WalletBatch &batch, const CKey& secret, const CPubKey &pubkey)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata

    // CCryptoKeyStore has no concept of wallet databases, but calls AddCryptedKey
    // which is overridden below.  To avoid flushes, the database handle is
    // tunneled through to it.
    bool needsDB = !encrypted_batch;
    if (needsDB) {
        encrypted_batch = &batch;
    }
    if (!CCryptoKeyStore::AddKeyPubKey(secret, pubkey)) {
        if (needsDB) encrypted_batch = nullptr;
        return false;
    }
    if (needsDB) encrypted_batch = nullptr;

    // check if we need to remove from watch-only
    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script)) {
        RemoveWatchOnly(script);
    }
    script = GetScriptForRawPubKey(pubkey);
    if (HaveWatchOnly(script)) {
        RemoveWatchOnly(script);
    }

    if (!IsCrypted()) {
        bool out=batch.WriteKey(pubkey,
                                                 secret.GetPrivKey(),
                                                 mapKeyMetadata[pubkey.GetID()]);
        return out;
    }
    return true;
}

bool CWallet::AddKeyPubKey(const CKey& secret, const CPubKey &pubkey)
{
    WalletBatch batch(*database);
    return CWallet::AddKeyPubKeyWithDB(batch, secret, pubkey);
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey,
                            const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    {
        LOCK(cs_wallet);
        if (encrypted_batch)
            return encrypted_batch->WriteCryptedKey(vchPubKey,
                                                        vchCryptedSecret,
                                                        mapKeyMetadata[vchPubKey.GetID()]);
        else
            return WalletBatch(*database).WriteCryptedKey(vchPubKey,
                                                            vchCryptedSecret,
                                                            mapKeyMetadata[vchPubKey.GetID()]);
    }
}

bool CWallet::LoadKeyMetadata(const CKeyID& keyID, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    UpdateTimeFirstKey(meta.nCreateTime);
    mapKeyMetadata[keyID] = meta;
    return true;
}

bool CWallet::LoadScriptMetadata(const CScriptID& script_id, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // m_script_metadata
    UpdateTimeFirstKey(meta.nCreateTime);
    m_script_metadata[script_id] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

/**
 * Update wallet first key creation time. This should be called whenever keys
 * are added to the wallet, with the oldest key creation time.
 */
void CWallet::UpdateTimeFirstKey(int64_t nCreateTime)
{
    AssertLockHeld(cs_wallet);
    if (nCreateTime <= 1) {
        // Cannot determine birthday information, so set the wallet birthday to
        // the beginning of time.
        nTimeFirstKey = 1;
    } else if (!nTimeFirstKey || nCreateTime < nTimeFirstKey) {
        nTimeFirstKey = nCreateTime;
    }
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    return WalletBatch(*database).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = EncodeDestinationHasSecondKey(CScriptID(redeemScript));
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::AddWatchOnly(const CScript& dest)
{
    if (!CCryptoKeyStore::AddWatchOnly(dest))
        return false;
    const CKeyMetadata& meta = m_script_metadata[CScriptID(dest)];
    UpdateTimeFirstKey(meta.nCreateTime);
    NotifyWatchonlyChanged(true);
    return WalletBatch(*database).WriteWatchOnly(dest, meta);
}

bool CWallet::AddWatchOnly(const CScript& dest, int64_t nCreateTime)
{
    m_script_metadata[CScriptID(dest)].nCreateTime = nCreateTime;
    return AddWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript &dest)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveWatchOnly(dest))
        return false;
    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (!WalletBatch(*database).EraseWatchOnly(dest))
        return false;

    return true;
}

bool CWallet::LoadWatchOnly(const CScript &dest)
{
    return CCryptoKeyStore::AddWatchOnly(dest);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    CCrypter crypter;
    CKeyingMaterial _vMasterKey;

    {
        LOCK(cs_wallet);
        for (const MasterKeyMap::value_type& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
                continue; // try another master key
            if (CCryptoKeyStore::Unlock(_vMasterKey))
                return true;
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial _vMasterKey;
        for (MasterKeyMap::value_type& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(_vMasterKey))
            {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = static_cast<unsigned int>(pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime))));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + static_cast<unsigned int>(pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime)))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(_vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                WalletBatch(*database).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::ChainStateFlushed(const CBlockLocator& loc)
{
    WalletBatch batch(*database);
    batch.WriteBestBlock(loc);
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, WalletBatch* batch_in, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    {
        WalletBatch* batch = batch_in ? batch_in : new WalletBatch(*database);
        if (nWalletVersion > 40000)
            batch->WriteMinVersion(nWalletVersion);
        if (!batch_in)
            delete batch;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

std::set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    std::set<uint256> result;
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    for (const CTxIn& txin : wtx.tx->vin)
    {
        if (txin.isnickname) continue;
        if (mapTxSpends.count(txin.prevout) <= 1)
            continue;  // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator _it = range.first; _it != range.second; ++_it)
            result.insert(_it->second);
    }
    return result;
}

bool CWallet::HasWalletSpend(const uint256& txid) const
{
    AssertLockHeld(cs_wallet);
    auto iter = mapTxSpends.lower_bound(COutPoint(txid, 0));
    return (iter != mapTxSpends.end() && iter->first.hash == txid);
}

void CWallet::Flush(bool shutdown)
{
    database->Flush(shutdown);
}

void CWallet::SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator> range)
{
    // We want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest nOrderPos).
    // So: find smallest nOrderPos:

    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = nullptr;
    for (TxSpends::iterator it = range.first; it != range.second; ++it) {
        const CWalletTx* wtx = &mapWallet.at(it->second);
        if (wtx->nOrderPos < nMinOrderPos) {
            nMinOrderPos = wtx->nOrderPos;;
            copyFrom = wtx;
        }
    }

    assert(copyFrom);

    // Now copy data from copyFrom to rest:
    for (TxSpends::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        CWalletTx* copyTo = &mapWallet.at(hash);
        if (copyFrom == copyTo) continue;
        assert(copyFrom && "Oldest wallet transaction in range assumed to have been found.");
        if (!copyFrom->IsEquivalentTo(*copyTo)) continue;
        copyTo->mapValue = copyFrom->mapValue;
        copyTo->vOrderForm = copyFrom->vOrderForm;
        // fTimeReceivedIsTxTime not copied on purpose
        // nTimeReceived not copied on purpose
        copyTo->nTimeSmart = copyFrom->nTimeSmart;
        copyTo->fFromMe = copyFrom->fFromMe;
        copyTo->strFromAccount = copyFrom->strFromAccount;
        // nOrderPos not copied on purpose
        // cached members not copied on purpose
    }
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const uint256& hash, unsigned int n) const
{
    const COutPoint outpoint(hash, n);
    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
    {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end()) {
            int depth = mit->second.GetDepthInMainChain();
            if (depth > 0  || (depth == 0 && !mit->second.isAbandoned()))
                return true; // Spent
        }
    }
    return false;
}

void CWallet::AddToSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.insert(std::make_pair(outpoint, wtxid));

    std::pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData(range);
}


void CWallet::AddToSpends(const uint256& wtxid)
{
    auto it = mapWallet.find(wtxid);
    assert(it != mapWallet.end());
    CWalletTx& thisTx = it->second;
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;
    for (const CTxIn& txin : thisTx.tx->vin){
        if (txin.isnickname) // Nicknames don't spend anything!
        continue;
 
        AddToSpends(txin.prevout, wtxid);
    }
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial _vMasterKey;

    _vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    GetStrongRandBytes(&_vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;

    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = static_cast<unsigned int>(2500000 / ((double)(GetTimeMillis() - nStartTime)));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + static_cast<unsigned int>(kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime)))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    LogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(_vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        assert(!encrypted_batch);
        encrypted_batch = new WalletBatch(*database);
        if (!encrypted_batch->TxnBegin()) {
            delete encrypted_batch;
            encrypted_batch = nullptr;
            return false;
        }
        encrypted_batch->WriteMasterKey(nMasterKeyMaxID, kMasterKey);

        if (!EncryptKeys(_vMasterKey))
        {
            encrypted_batch->TxnAbort();
            delete encrypted_batch;
            // We now probably have half of our keys encrypted in memory, and half not...
            // die and let the user reload the unencrypted wallet.
            assert(false);
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, encrypted_batch, true);

        if (!encrypted_batch->TxnCommit()) {
            delete encrypted_batch;
            // We now have keys encrypted in memory, but not on disk...
            // die to avoid confusion and let the user reload the unencrypted wallet.
            assert(false);
        }

        delete encrypted_batch;
        encrypted_batch = nullptr;

        Lock();
        Unlock(strWalletPassphrase);

        // if we are using HD, replace the HD master key (seed) with a new one
        if (IsHDEnabled()) {
            if (!SetHDMasterKey(GenerateNewHDMasterKey())) {
                return false;
            }
        }

        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        database->Rewrite();

    }
    NotifyStatusChanged(this);

    return true;
}

DBErrors CWallet::ReorderTransactions()
{
    LOCK(cs_wallet);
    WalletBatch batch(*database);

    // Old wallets didn't have any defined order for transactions
    // Probably a bad idea to change the output of this

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap.
    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;
    TxItems txByTime;

    for (auto& entry : mapWallet)
    {
        CWalletTx* wtx = &entry.second;
        txByTime.insert(std::make_pair(wtx->nTimeReceived, TxPair(wtx, nullptr)));
    }
    std::list<CAccountingEntry> acentries;
    batch.ListAccountCreditDebit("", acentries);
    for (CAccountingEntry& entry : acentries)
    {
        txByTime.insert(std::make_pair(entry.nTime, TxPair(nullptr, &entry)));
    }

    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        CAccountingEntry *const pacentry = (*it).second.second;
        int64_t& nOrderPos = (pwtx != nullptr) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1)
        {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (pwtx)
            {
                if (!batch.WriteTx(*pwtx))
                    return DBErrors::LOAD_FAIL;
            }
            else
                if (!batch.WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DBErrors::LOAD_FAIL;
        }
        else
        {
            int64_t nOrderPosOff = 0;
            for (const int64_t& nOffsetStart : nOrderPosOffsets)
            {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            // Since we're changing the order, write it back
            if (pwtx)
            {
                if (!batch.WriteTx(*pwtx))
                    return DBErrors::LOAD_FAIL;
            }
            else
                if (!batch.WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DBErrors::LOAD_FAIL;
        }
    }
    batch.WriteOrderPosNext(nOrderPosNext);

    return DBErrors::LOAD_OK;
}

int64_t CWallet::IncOrderPosNext(WalletBatch *batch)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (batch) {
        batch->WriteOrderPosNext(nOrderPosNext);
    } else {
        WalletBatch(*database).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

bool CWallet::AccountMove(std::string strFrom, std::string strTo, CAmount nAmount, std::string strComment, unsigned char currency)
{
    WalletBatch batch(*database);
    if (!batch.TxnBegin())
        return false;

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos = IncOrderPosNext(&batch);
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    debit.currency = currency;
    AddAccountingEntry(debit, &batch);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos = IncOrderPosNext(&batch);
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    credit.currency = currency;
    AddAccountingEntry(credit, &batch);

    if (!batch.TxnCommit())
        return false;

    return true;
}

bool CWallet::GetLabelDestination(CTxDestination &dest, const std::string& label, bool createnonprivate)
{
    WalletBatch batch(*database);

    CAccount account;
    batch.ReadAccount(label, account);


    // Generate a new key
    if (!account.vchPubKey.IsValid()) {
        if (!GetKeyFromPool(account.vchPubKey, false))
            return false;

        LearnRelatedScripts(account.vchPubKey, m_default_address_type);
        if (createnonprivate) {
            dest = GetDestinationForKey(account.vchPubKey, OutputType::NONPRIVATE);
        } else {
            dest = GetDestinationForKey(account.vchPubKey, m_default_address_type);
        }
        SetAddressBook(dest, label, "receive");
        batch.WriteAccount(label, account);
    } else {
        if (createnonprivate) {
            dest = GetDestinationForKey(account.vchPubKey, OutputType::NONPRIVATE);
        } else {
            dest = GetDestinationForKey(account.vchPubKey, m_default_address_type);
        }
    }

    return true;
}

// Can be used to set the main pubkey of the wallet (if label == ""). getcurrentaddress will then return the address for this pubkey. 
// The address and nickname for this pubkey will then also be displayed in the GUI
void CWallet::SetLabelDestination(CPubKey &vchPubKey, const std::string& label)
{
    WalletBatch batch(*database);

    CAccount account;
    batch.ReadAccount(label, account);

    account.vchPubKey = vchPubKey;
    batch.WriteAccount(label, account);
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        for (std::pair<const uint256, CWalletTx>& item : mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::MarkReplaced(const uint256& originalHash, const uint256& newHash)
{
    LOCK(cs_wallet);

    auto mi = mapWallet.find(originalHash);

    // There is a bug if MarkReplaced is not called on an existing wallet transaction.
    assert(mi != mapWallet.end());

    CWalletTx& wtx = (*mi).second;

    // Ensure for now that we're not overwriting data
    assert(wtx.mapValue.count("replaced_by_txid") == 0);

    wtx.mapValue["replaced_by_txid"] = newHash.ToString();

    WalletBatch batch(*database, "r+");

    bool success = true;
    if (!batch.WriteTx(wtx)) {
        LogPrintf("%s: Updating batch tx %s failed\n", __func__, wtx.GetHash().ToString());
        success = false;
    }

    NotifyTransactionChanged(this, originalHash, CT_UPDATED);

    return success;
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFlushOnClose)
{
    LOCK(cs_wallet);

    WalletBatch batch(*database, "r+", fFlushOnClose);

    uint256 hash = wtxIn.GetHash();

    // Inserts only if not already there, returns tx inserted or tx found
    std::pair<std::map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(std::make_pair(hash, wtxIn));
    CWalletTx& wtx = (*ret.first).second;
    wtx.BindWallet(this);
    bool fInsertedNew = ret.second;
    if (fInsertedNew)
    {
        wtx.nTimeReceived = GetAdjustedTime();
        wtx.nOrderPos = IncOrderPosNext(&batch);
        wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, nullptr)));
        wtx.nTimeSmart = ComputeTimeSmart(wtx);
        AddToSpends(hash);
    }

    bool fUpdated = false;
    if (!fInsertedNew)
    {
        // Merge
        if (!wtxIn.hashUnset() && wtxIn.hashBlock != wtx.hashBlock)
        {
            wtx.hashBlock = wtxIn.hashBlock;
            fUpdated = true;
        }
        // If no longer abandoned, update
        if (wtxIn.hashBlock.IsNull() && wtx.isAbandoned())
        {
            wtx.hashBlock = wtxIn.hashBlock;
            fUpdated = true;
        }
        if (wtxIn.nIndex != -1 && (wtxIn.nIndex != wtx.nIndex))
        {
            wtx.nIndex = wtxIn.nIndex;
            fUpdated = true;
        }
        if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
        {
            wtx.fFromMe = wtxIn.fFromMe;
            fUpdated = true;
        }
        // If we have a witness-stripped version of this transaction, and we
        // see a new version with a witness, then we must be upgrading a pre-segwit
        // wallet.  Store the new version of the transaction with the witness,
        // as the stripped-version must be invalid.
        // TODO: Store all versions of the transaction, instead of just one.
        if (wtxIn.tx->HasWitness() && !wtx.tx->HasWitness()) {
            wtx.SetTx(wtxIn.tx);
            fUpdated = true;
        }
        // If the nValue in the output currency has been recalculated from the nValueBitCash value, then update the transaction
        for (unsigned i = 0; i < wtxIn.tx->vout.size() && i < wtx.tx->vout.size(); i++) {
            if (wtxIn.tx->vout[i].nValue != wtx.tx->vout[i].nValue) {
                wtx.SetTx(wtxIn.tx);
                fUpdated = true;
                break; 
            }    
        }
    }

    //// debug print
    LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

    // Write to disk
    if (fInsertedNew || fUpdated)
        if (!batch.WriteTx(wtx))
            return false;

    // Break debit/credit balance caches:
    wtx.MarkDirty();

    // Notify UI of new or updated transaction
    NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

    // notify an external script when a wallet transaction comes in or is updated
    std::string strCmd = gArgs.GetArg("-walletnotify", "");

    if (!strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
        std::thread t(runCommand, strCmd);
        t.detach(); // thread runs free
    }

    return true;
}

bool CWallet::LoadToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();
    CWalletTx& wtx = mapWallet.emplace(hash, wtxIn).first->second;
    wtx.BindWallet(this);
    wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, nullptr)));
    AddToSpends(hash);
    for (const CTxIn& txin : wtx.tx->vin) {
        if (txin.isnickname) continue; 
        auto it = mapWallet.find(txin.prevout.hash);
        if (it != mapWallet.end()) {
            CWalletTx& prevtx = it->second;
            if (prevtx.nIndex == -1 && !prevtx.hashUnset()) {
                MarkConflicted(prevtx.hashBlock, wtx.GetHash());
            }
        }
    }

    return true;
}

/**
 * Add a transaction to the wallet, or update it.  pIndex and posInBlock should
 * be set when the transaction was known to be included in a block.  When
 * pIndex == nullptr, then wallet state is not updated in AddToWallet, but
 * notifications happen and cached balances are marked dirty.
 *
 * If fUpdate is true, existing transactions will be updated.
 * TODO: One exception to this is that the abandoned state is cleared under the
 * assumption that any further notification of a transaction that was considered
 * abandoned is an indication that it is not safe to be considered abandoned.
 * Abandoned state should probably be more carefully tracked via different
 * posInBlock signals or by checking mempool presence when necessary.
 */
bool CWallet::AddToWalletIfInvolvingMe(const CTransactionRef& ptx, const CBlockIndex* pIndex, int posInBlock, bool fUpdate)
{
    const CTransaction& tx = *ptx;

    {
        AssertLockHeld(cs_wallet);

        if (pIndex != nullptr) {
            for (const CTxIn& txin : tx.vin) {
                if (txin.isnickname) continue;
                std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range = mapTxSpends.equal_range(txin.prevout);
                while (range.first != range.second) {
                    if (range.first->second != tx.GetHash()) {
                        LogPrintf("Transaction %s (in block %s) conflicts with wallet transaction %s (both spend %s:%i)\n", tx.GetHash().ToString(), pIndex->GetBlockHash().ToString(), range.first->second.ToString(), range.first->first.hash.ToString(), range.first->first.n);
                        MarkConflicted(pIndex->GetBlockHash(), range.first->second);
                    }
                    range.first++;
                }
            }
        }

        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate) return false;
        if (IsMineForScanningBlockchain(tx) || IsFromMe(tx) || fExisted)//IsMineForScanningBlockchain must be called first, because it decodes the stealth addresses
        {
            /* Check if any keys in the wallet keypool that were supposed to be unused
             * have appeared in a new transaction. If so, remove those keys from the keypool.
             * This can happen when restoring an old wallet backup that does not contain
             * the mostly recently created transactions from newer versions of the wallet.
             */

            // loop though all outputs
            for (const CTxOut& txout: tx.vout) {
                // extract addresses and check if they match with an unused keypool key
                std::vector<CKeyID> vAffected;
                CAffectedKeysVisitor(*this, vAffected).Process(txout.scriptPubKey);
                for (const CKeyID &keyid : vAffected) {
                    std::map<CKeyID, int64_t>::const_iterator mi = m_pool_key_to_index.find(keyid);
                    if (mi != m_pool_key_to_index.end()) {
                        LogPrintf("%s: Detected a used keypool key, mark all keypool key up to this key as used\n", __func__);
                        MarkReserveKeysAsUsed(mi->second);

                        if (!TopUpKeyPool()) {
                            LogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
                        }
                    }
                }
            }

            CWalletTx wtx(this, ptx);

            // Get merkle branch if transaction was found in a block
            if (pIndex != nullptr)
                wtx.SetMerkleBranch(pIndex, posInBlock);

            return AddToWallet(wtx, false);
        }
    }
    return false;
}

bool CWallet::TransactionCanBeAbandoned(const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    auto it = mapWallet.find(hashTx);
    if (it != mapWallet.end()) {
        CWalletTx& wtx = it->second;

        if (!wtx.isAbandoned() && wtx.GetDepthInMainChain() == 0 && wtx.InMempool()) {
            if (wtx.tx->haspricerange) {
                double pri;
                if (wtx.tx->haspricerange == 1) {
                    pri = GetBlockPrice(1);
                } else {
                    pri = GetBlockPrice(0);
                }
                if (pri < wtx.tx->minprice || pri > wtx.tx->maxprice) {
                    wtx.RemoveFromMempool();
                    TransactionRemovedFromMempool(wtx.tx);
                }
            }
        }
        return !wtx.isAbandoned() && wtx.GetDepthInMainChain() == 0 && !wtx.InMempool();

    } else return false;
}

bool CWallet::AbandonTransaction(const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    WalletBatch batch(*database, "r+");

    std::set<uint256> todo;
    std::set<uint256> done;

    // Can't mark abandoned if confirmed or in mempool
    auto it = mapWallet.find(hashTx);
    assert(it != mapWallet.end());
    CWalletTx& origtx = it->second;
    if (origtx.GetDepthInMainChain() != 0 || origtx.InMempool()) {
        return false;
    }

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        auto it = mapWallet.find(now);
        assert(it != mapWallet.end());
        CWalletTx& wtx = it->second;
        int currentconfirm = wtx.GetDepthInMainChain();
        // If the orig tx was not in block, none of its spends can be
        assert(currentconfirm <= 0);
        // if (currentconfirm < 0) {Tx and spends are already conflicted, no need to abandon}
        if (currentconfirm == 0 && !wtx.isAbandoned()) {
            // If the orig tx was not in block/mempool, none of its spends can be in mempool
            assert(!wtx.InMempool());
            wtx.nIndex = -1;
            wtx.setAbandoned();
            wtx.MarkDirty();
            batch.WriteTx(wtx);
            NotifyTransactionChanged(this, wtx.GetHash(), CT_UPDATED);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them abandoned too
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(hashTx, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for (const CTxIn& txin : wtx.tx->vin)
            {
                auto it = mapWallet.find(txin.prevout.hash);
                if (it != mapWallet.end()) {
                    it->second.MarkDirty();
                }
            }
        }
    }

    return true;
}

void CWallet::MarkConflicted(const uint256& hashBlock, const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    int conflictconfirms = 0;
    CBlockIndex* pindex = LookupBlockIndex(hashBlock);
    if (pindex && chainActive.Contains(pindex)) {
        conflictconfirms = -(chainActive.Height() - pindex->nHeight + 1);
    }
    // If number of conflict confirms cannot be determined, this means
    // that the block is still unknown or not yet part of the main chain,
    // for example when loading the wallet during a reindex. Do nothing in that
    // case.
    if (conflictconfirms >= 0)
        return;

    // Do not flush the wallet here for performance reasons
    WalletBatch batch(*database, "r+", false);

    std::set<uint256> todo;
    std::set<uint256> done;

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        auto it = mapWallet.find(now);
        assert(it != mapWallet.end());
        CWalletTx& wtx = it->second;
        int currentconfirm = wtx.GetDepthInMainChain();
        if (conflictconfirms < currentconfirm) {
            // Block is 'more conflicted' than current confirm; update.
            // Mark transaction as conflicted with this block.
            wtx.nIndex = -1;
            wtx.hashBlock = hashBlock;
            wtx.MarkDirty();
            batch.WriteTx(wtx);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them conflicted too
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(now, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                 if (!done.count(iter->second)) {
                     todo.insert(iter->second);
                 }
                 iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for (const CTxIn& txin : wtx.tx->vin) {
                auto it = mapWallet.find(txin.prevout.hash);
                if (it != mapWallet.end()) {
                    it->second.MarkDirty();
                }
            }
        }
    }
}

void CWallet::SyncTransaction(const CTransactionRef& ptx, const CBlockIndex *pindex, int posInBlock) {
    const CTransaction& tx = *ptx;

    if (!AddToWalletIfInvolvingMe(ptx, pindex, posInBlock, true))
        return; // Not one of ours

    // If a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. So force those to be
    // recomputed, also:
    for (const CTxIn& txin : tx.vin) {
        auto it = mapWallet.find(txin.prevout.hash);
        if (it != mapWallet.end()) {
            it->second.MarkDirty();
        }
    }
}

void CWallet::TransactionAddedToMempool(const CTransactionRef& ptx) {
    LOCK2(cs_main, cs_wallet);
    SyncTransaction(ptx);

    auto it = mapWallet.find(ptx->GetHash());
    if (it != mapWallet.end()) {
        it->second.fInMempool = true;
    }
}

void CWallet::TransactionRemovedFromMempool(const CTransactionRef &ptx) {
    LOCK(cs_wallet);
    auto it = mapWallet.find(ptx->GetHash());
    if (it != mapWallet.end()) {
        it->second.fInMempool = false;
    }
}

void CWallet::BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex *pindex, const std::vector<CTransactionRef>& vtxConflicted) {
    LOCK2(cs_main, cs_wallet);
    // TODO: Temporarily ensure that mempool removals are notified before
    // connected transactions.  This shouldn't matter, but the abandoned
    // state of transactions in our wallet is currently cleared when we
    // receive another notification and there is a race condition where
    // notification of a connected conflict might cause an outside process
    // to abandon a transaction and then have it inadvertently cleared by
    // the notification that the conflicted transaction was evicted.

    for (const CTransactionRef& ptx : vtxConflicted) {
        SyncTransaction(ptx);
        TransactionRemovedFromMempool(ptx);
    }
    for (size_t i = 0; i < pblock->vtx.size(); i++) {
        SyncTransaction(pblock->vtx[i], pindex, i);
        TransactionRemovedFromMempool(pblock->vtx[i]);
    }

    m_last_block_processed = pindex;
}

void CWallet::BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) {
    LOCK2(cs_main, cs_wallet);

    for (const CTransactionRef& ptx : pblock->vtx) {
        SyncTransaction(ptx);
    }
}



void CWallet::BlockUntilSyncedToCurrentChain() {
    AssertLockNotHeld(cs_main);
    AssertLockNotHeld(cs_wallet);

    {
        // Skip the queue-draining stuff if we know we're caught up with
        // chainActive.Tip()...
        // We could also take cs_wallet here, and call m_last_block_processed
        // protected by cs_wallet instead of cs_main, but as long as we need
        // cs_main here anyway, it's easier to just call it cs_main-protected.
        LOCK(cs_main);
        const CBlockIndex* initialChainTip = chainActive.Tip();

        if (m_last_block_processed->GetAncestor(initialChainTip->nHeight) == initialChainTip) {
            return;
        }
    }

    // ...otherwise put a callback in the validation interface queue and wait
    // for the queue to drain enough to execute it (indicating we are caught up
    // at least with the time we entered this function).
    SyncWithValidationInterfaceQueue();
}

unsigned char CWallet::GetInputCurrency(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
                return prev.tx->vout[txin.prevout.n].currency;
        }
    }
    return 0;
}


isminetype CWallet::IsMine(const CTxIn &txin)
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
                return IsMine(prev.tx->vout[txin.prevout.n], 21, prev.tx->nVersion >= 7);
        }
    }
    return ISMINE_NO;
}

// Note that this function doesn't distinguish between a 0-valued input,
// and a not-"is mine" (according to the filter) input.
CAmount CWallet::GetDebit(const CTxIn &txin, const isminefilter& filter, unsigned char currency) const//255=all currencies
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (currency==255 || prev.tx->vout[txin.prevout.n].currency==currency)
            if (txin.prevout.n < prev.tx->vout.size())
                if (IsMineConst(prev.tx->vout[txin.prevout.n],22) & filter)
                    return prev.tx->vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

// Note that this function doesn't distinguish between a 0-valued input,
// and a not-"is mine" (according to the filter) input.
CAmount CWallet::GetDebitForAddress(CTxDestination dest, const CTxIn &txin, unsigned char currency) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (currency == 255 || prev.tx->vout[txin.prevout.n].currency==currency)
            if (txin.prevout.n < prev.tx->vout.size())
                if (IsMineForOneDestination(prev.tx->vout[txin.prevout.n], dest, prev.tx->nVersion >= 7) != ISMINE_NO) {
                    return prev.tx->vout[txin.prevout.n].nValue;
                }
        }
    }
    return 0;
}

isminetype CWallet::IsMineBasic(const CTxOut& txout, int nr)
{
    return ::IsMine(*this, txout.scriptPubKey,txout.randomPubKey,(char*)&txout.randomPrivatKey);
}

isminetype CWallet::IsMineConst(const CTxOut& txout, int nr) const
{
    return ::IsMine(*this, txout.scriptPubKey,txout.randomPubKey,(char*)&txout.randomPrivatKey);
}

isminetype CWallet::IsMineForOneDestination(const CTxOut& txout, CTxDestination& desttocheck, bool saltisactive) const
{
    unsigned int salt1 = *(unsigned int*)&txout.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&txout.randomPubKey[5];

    CPubKey onetimedestpubkey;

    CPubKey pubkey = GetSecondPubKeyForDestination(desttocheck);
    CPubKey viewpubkey = GetViewPubKeyForDestination(desttocheck);
    bool hasviewkey = GetHasViewKeyForDestination(desttocheck);

    if (saltisactive) {
        if (txout.hasrecipientid) {
            if (((txout.recipientid1 != 0 || txout.recipientid2 != 0) || (txout.currencyisactive)) && (txout.recipientid1 != pubkey[salt1 % 30 + 1] || txout.recipientid2 != pubkey[salt2 % 30 + 1])) return ISMINE_NO;
        }
    } else {
        if (txout.hasrecipientid) {
            if (((txout.recipientid1 != 0 || txout.recipientid2 != 0) || (txout.currencyisactive)) && (txout.recipientid1 != pubkey[10] || txout.recipientid2 != pubkey[20])) return ISMINE_NO;
        }
    }

    if (ExtractCompletePubKey(*this, txout.scriptPubKey,onetimedestpubkey))
    {
        CKey key;
        bool havekey = GetKey(pubkey.GetID(), key);
        if (havekey || hasviewkey) {

            if (txout.isnonprivate)
            {
                if (key.VerifyPubKey(pubkey)) return ISMINE_SPENDABLE;
            } else 
            {
                //std::cout << "have privkey; Pubkey in Address Book to check: " << HexStr(pubkey.begin(),pubkey.end()) << std::endl;

                char randprivkey[32];
                memcpy(&randprivkey,txout.randomPrivatKey,32);
                if (hasviewkey) {
                    CKey viewkey;
                    if (!GetKey(viewpubkey.GetID(), viewkey)) {                            
                        //we do not store the view, so calculate it from the private key
                        viewkey = key.GetViewKeyForPrivateKey();
                    }
                    DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, viewkey, saltisactive, salt1, salt2);
                } else {
                    DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, key, saltisactive, salt1, salt2);
                }

                std::vector<unsigned char> vec(randprivkey, randprivkey + 32);

                CKey privkey;
                privkey.Set(vec.begin(),vec.end(),true);
                CPubKey destinationPubKey=CalculateOnetimeDestPubKey(pubkey, privkey, false, txout.masterkeyisremoved);
                if (onetimedestpubkey==destinationPubKey) {      
                    if (havekey)
                        return ISMINE_SPENDABLE;
                    else
                       return ISMINE_WATCH_ONLY;                    
                }
            }
        }

        //If we have the master private key we can decode the transactions for the Watch-Only addresses
        CPubKey recipientpubkey;
        std::string referenceline;
        if (GetRealAddressAndRefline(txout, recipientpubkey, referenceline, "", false, saltisactive))
        {
           if (recipientpubkey==pubkey) {
               return ISMINE_WATCH_ONLY;
           }
        }

    }

    return ISMINE_NO;
}

isminetype CWallet::IsMine(const CTxOut& txout, int nr, bool saltisactive)
{
    unsigned int salt1 = *(unsigned int*)&txout.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&txout.randomPubKey[5];

    isminetype res;
    res = IsMineBasic(txout,3);

    if (res == ISMINE_NO && !txout.isnonprivate)
    {

        CPubKey onetimedestpubkey;
        WalletBatch batch(*database);

        if (ExtractCompletePubKey(*this, txout.scriptPubKey,onetimedestpubkey))
        {
            bool found = false;
            for (const std::pair<CTxDestination, CAddressBookData>& item : mapAddressBook) {
                if (GetNonPrivateForDestination(item.first)) continue;


                CPubKey pubkey = GetSecondPubKeyForDestination(item.first);
                CPubKey viewpubkey = GetViewPubKeyForDestination(item.first);
                bool hasviewkey = GetHasViewKeyForDestination(item.first);
    
                if (saltisactive) {
                    if (txout.hasrecipientid) {
                        if (((txout.recipientid1 != 0 || txout.recipientid2 != 0) || (txout.currencyisactive)) && (txout.recipientid1 != pubkey[salt1 % 30 + 1] || txout.recipientid2 != pubkey[salt2 % 30 + 1])) 
                            continue;
                    }
                } else {
                    if (txout.hasrecipientid) {
                        if (((txout.recipientid1 != 0 || txout.recipientid2 != 0) || (txout.currencyisactive)) && (txout.recipientid1 != pubkey[10] || txout.recipientid2 != pubkey[20])) 
                            continue;
                    }
                }

                CKey key;
                bool havekey = GetKey(pubkey.GetID(), key);
                if (havekey || hasviewkey) {

                    char randprivkey[32];
                    memcpy(&randprivkey,txout.randomPrivatKey,32);
                    CKey viewkey;
                    if (hasviewkey) {
                        if (!GetKey(viewpubkey.GetID(), viewkey)) {
                            //we do not store the view, so calculate it from the private key
                            viewkey = key.GetViewKeyForPrivateKey();
                        }
                        DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, viewkey, saltisactive, salt1, salt2);
                    } else {
                        DecryptPrivateKey((unsigned char*)&randprivkey, txout.randomPubKey, key, saltisactive, salt1, salt2);
                    }

                    std::vector<unsigned char> vec(randprivkey, randprivkey + 32);

                    CKey privkey;
                    privkey.Set(vec.begin(),vec.end(),true);
                    CPubKey destinationPubKey = CalculateOnetimeDestPubKey(pubkey, privkey, false, txout.masterkeyisremoved);
                    if (onetimedestpubkey == destinationPubKey) {
                        if (!havekey) {
                            SetStealthAddress(txout.scriptPubKey, pubkey);
                            if (hasviewkey) SetViewkeyStealthAddress(txout.scriptPubKey, viewpubkey);
                            accountforscript[txout.scriptPubKey] = item.second.name;

                            res = IsMineBasic(txout,4);

                            CTxDestination dest = GetDestinationForKey(pubkey, OutputType::WITHVIEWKEY);
                            CTxOut txout2 = txout;
                            txout2.scriptPubKey = GetScriptForRawPubKey(pubkey);
                            res = IsMineBasic(txout2,88);

                            if (res & ISMINE_WATCH_ONLY) {
                                std::string strLabel = mapAddressBook[dest].name;
                                CTxDestination dest2 = GetDestinationForKey(onetimedestpubkey, OutputType::WITHVIEWKEY);
                                if (!HaveWatchOnly(txout.scriptPubKey)) {
                                    AddWatchOnly(txout.scriptPubKey, 0 /* nCreateTime */);
                                }
/*                                if (IsValidDestination(dest2)) {
                                    SetAddressBook(dest2, strLabel, "receive");
                                }*/
                                res=IsMineBasic(txout,89);
                            }

                            found = true;
                        } else 
                        {
                            CKey otpk = CalculateOnetimeDestPrivateKey(key, privkey, txout.masterkeyisremoved);  

                            AddKeyPubKeyWithDB(batch, otpk, destinationPubKey);
                            SetStealthAddress(txout.scriptPubKey, pubkey);
                            if (hasviewkey) SetViewkeyStealthAddress(txout.scriptPubKey, viewpubkey);
                            accountforscript[txout.scriptPubKey] = item.second.name;

                            res = IsMineBasic(txout,4);

                            found = true;
                        }
                    }
                }                
            }
            if (!found) {
                //If we have the master private key we can decode the transactions for the Watch-Only addresses
                CPubKey recipientpubkey;
                std::string referenceline;
                if (GetRealAddressAndRefline(txout, recipientpubkey, referenceline, "", false, saltisactive))
                {
                    CTxDestination dest = GetDestinationForKey(recipientpubkey, OutputType::LEGACY);
                    CTxOut txout2 = txout;
                    txout2.scriptPubKey = GetScriptForRawPubKey(recipientpubkey);
                    res = IsMineBasic(txout2,88);
                    if (res & ISMINE_WATCH_ONLY) {
                        std::string strLabel = mapAddressBook[dest].name;
                        CTxDestination dest2 = GetDestinationForKey(onetimedestpubkey, OutputType::LEGACY);
                        if (!HaveWatchOnly(txout.scriptPubKey)) {
                            AddWatchOnly(txout.scriptPubKey, 0 /* nCreateTime */);
                        }
                        if (IsValidDestination(dest2)) {
                            SetAddressBook(dest2, strLabel, "receive");
                        }
                        res = IsMineBasic(txout,89);
                    }
                }
            }
        }    
    }

    return res;
}


CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter, unsigned char currency) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    if (currency==255){
      return ((IsMineConst(txout,1) & filter) ? txout.nValue : 0);
    } else {
      return ((txout.currency==currency && (IsMineConst(txout,1) & filter)) ? txout.nValue : 0);
    }
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a script that is ours, but is not in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (::IsMine(*this, txout.scriptPubKey))
    {
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
            return true;

        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;
    }
    return false;
}

bool CWallet::IsChangeForAddress(CTxDestination dest, const CTxOut& txout, bool saltisactive) const
{
    if (IsMineForOneDestination(txout, dest, saltisactive) != ISMINE_NO)
    {
        return true;
    }
    return false;
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

bool CWallet::IsMine(const CTransaction& tx)
{
    for (const CTxOut& txout : tx.vout)
        if (IsMine(txout, 2, tx.nVersion >= 7))
            return true;
    return false;
}

bool CWallet::IsMineForScanningBlockchain(const CTransaction& tx)
{
    bool ismine=false;

    //we need to loop through all outputs here. Ismine needs to be called for all outputs because it will add the private keys for the stealth addresses
    for (const CTxOut& txout : tx.vout)
        if (IsMine(txout, 2, tx.nVersion >= 7))
            ismine=true;

    return ismine;
}

bool CWallet::IsFromMe(const CTransaction& tx)
{
    return (GetDebit(tx, ISMINE_ALL, 255) > 0);//255=all currencies
}

CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter, unsigned char currency) const
{
    CAmount nDebit = 0;
    for (const CTxIn& txin : tx.vin)
    {
        nDebit += GetDebit(txin, filter, currency);
        if (!MoneyRange(nDebit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nDebit;
}

CAmount CWallet::GetDebitForAddress(CTxDestination dest, const CTransaction& tx, unsigned char currency) const
{
    CAmount nDebit = 0;
    for (const CTxIn& txin : tx.vin)
    {
        nDebit += GetDebitForAddress(dest, txin, currency);
        if (!MoneyRange(nDebit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nDebit;
}

bool CWallet::IsAllFromMe(const CTransaction& tx, const isminefilter& filter) const
{
    LOCK(cs_wallet);

    for (const CTxIn& txin : tx.vin)
    {
        auto mi = mapWallet.find(txin.prevout.hash);
        if (mi == mapWallet.end())
            return false; // any unknown inputs can't be from us

        const CWalletTx& prev = (*mi).second;

        if (txin.prevout.n >= prev.tx->vout.size())
            return false; // invalid input!

        if (!(IsMineConst(prev.tx->vout[txin.prevout.n],31) & filter))
            return false;
    }
    return true;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter, unsigned char currency) const
{
    CAmount nCredit = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nCredit += GetCredit(txout, filter, currency);
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nCredit;
}

CAmount CWallet::GetCreditForOneDestination(CTxDestination dest,const CTransaction& tx, const isminefilter& filter, unsigned char currency) const
{
    CAmount nCredit = 0;
    for (const CTxOut& txout : tx.vout)
    {
        if (IsMineForOneDestination(txout, dest, tx.nVersion >= 7) == ISMINE_NO) continue;
        nCredit += GetCredit(txout, filter, currency);
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nChange += GetChange(txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nChange;
}

CPubKey CWallet::GenerateNewHDMasterKey()
{
    CKey key;
    key.MakeNewKey(true);

    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // calculate the pubkey
    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));

    // set the hd keypath to "m" -> Master, refers the masterkeyid to itself
    metadata.hdKeypath     = "m";
    metadata.hdMasterKeyID = pubkey.GetID();

    {
        LOCK(cs_wallet);

        // mem store the metadata
        mapKeyMetadata[pubkey.GetID()] = metadata;

        // write the key&metadata to the database
        if (!AddKeyPubKey(key, pubkey))
            throw std::runtime_error(std::string(__func__) + ": AddKeyPubKey failed");
    }

    return pubkey;
}

bool CWallet::SetHDMasterKey(const CPubKey& pubkey)
{
    LOCK(cs_wallet);
    // store the keyid (hash160) together with
    // the child index counter in the database
    // as a hdchain object
    CHDChain newHdChain;
    newHdChain.nVersion = CanSupportFeature(FEATURE_HD_SPLIT) ? CHDChain::VERSION_HD_CHAIN_SPLIT : CHDChain::VERSION_HD_BASE;
    newHdChain.masterKeyID = pubkey.GetID();
    SetHDChain(newHdChain, false);

    return true;
}

bool CWallet::SetHDChain(const CHDChain& chain, bool memonly)
{
    LOCK(cs_wallet);
    if (!memonly && !WalletBatch(*database).WriteHDChain(chain))
        throw std::runtime_error(std::string(__func__) + ": writing chain failed");

    hdChain = chain;
    return true;
}

bool CWallet::IsHDEnabled() const
{
    return !hdChain.masterKeyID.IsNull();
}


std::string CWalletTx::DecryptRefLineTxOut(CTxOut out, bool saltisactive) const
{
    return pwallet->DecryptRefLineTxOut(out, saltisactive);
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase())
        {
            // Generated block
            if (!hashUnset())
            {
                std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && !hashUnset())
                {
                    std::map<uint256, int>::const_iterator _mi = pwallet->mapRequestCount.find(hashBlock);
                    if (_mi != pwallet->mapRequestCount.end())
                        nRequests = (*_mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

// Helper for producing a max-sized low-S signature (eg 72 bytes)
bool CWallet::DummySignInput(CTxIn &tx_in, const CTxOut &txout) const
{
    // Fill in dummy signatures for fee calculation.
    const CScript& scriptPubKey = txout.scriptPubKey;
    SignatureData sigdata;

    if (!ProduceSignature(*this, DUMMY_SIGNATURE_CREATOR, scriptPubKey, sigdata))
    {
        return false;
    } else {
        UpdateInput(tx_in, sigdata);
    }
    return true;
}

// Helper for producing a bunch of max-sized low-S signatures (eg 72 bytes)
bool CWallet::DummySignTx(CMutableTransaction &txNew, const std::vector<CTxOut> &txouts) const
{
    // Fill in dummy signatures for fee calculation.
    int nIn = 0;
    for (const auto& txout : txouts)
    {
        if (!DummySignInput(txNew.vin[nIn], txout)) {
            return false;
        }

        nIn++;
    }
    return true;
}

int64_t CalculateMaximumSignedTxSize(const CTransaction &tx, const CWallet *wallet)
{
    std::vector<CTxOut> txouts;
    // Look up the inputs.  We should have already checked that this transaction
    // IsAllFromMe(ISMINE_SPENDABLE), so every input should already be in our
    // wallet, with a valid index into the vout array, and the ability to sign.
    for (auto& input : tx.vin) {
        const auto mi = wallet->mapWallet.find(input.prevout.hash);
        if (mi == wallet->mapWallet.end()) {
            return -1;
        }
        assert(input.prevout.n < mi->second.tx->vout.size());
        txouts.emplace_back(mi->second.tx->vout[input.prevout.n]);
    }
    return CalculateMaximumSignedTxSize(tx, wallet, txouts);
}

int64_t CalculateMaximumSignedTxSizeWithOutput(const CTransaction &tx, const CWallet *wallet, CTxOut output)
{
    std::vector<CTxOut> txouts;
    txouts.emplace_back(output);
    return CalculateMaximumSignedTxSize(tx, wallet, txouts);
}

// txouts needs to be in the order of tx.vin
int64_t CalculateMaximumSignedTxSize(const CTransaction &tx, const CWallet *wallet, const std::vector<CTxOut>& txouts)
{
    CMutableTransaction txNew(tx);
    if (!wallet->DummySignTx(txNew, txouts)) {
        // This should never happen, because IsAllFromMe(ISMINE_SPENDABLE)
        // implies that we can sign for every input.
        return -1;
    }
    return GetVirtualTransactionSize(txNew);
}

int CalculateMaximumSignedInputSize(const CTxOut& txout, const CWallet* wallet)
{
    CMutableTransaction txn;
    txn.vin.push_back(CTxIn(COutPoint()));
    if (!wallet->DummySignInput(txn.vin[0], txout)) {
        // This should never happen, because IsAllFromMe(ISMINE_SPENDABLE)
        // implies that we can sign for every input.
        return -1;
    }
    return GetVirtualTransactionInputSize(txn.vin[0]);
}

void CWalletTx::GetAmounts(std::list<COutputEntry>& listReceived,
                           std::list<COutputEntry>& listSent, CAmount& nFee, std::string& strSentAccount, const isminefilter& filter, unsigned char currency) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    // Compute fee:
    CAmount nDebit = GetDebit(filter, currency);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = tx->GetValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent/received.
    for (unsigned int i = 0; i < tx->vout.size(); ++i)
    {
        const CTxOut& txout = tx->vout[i];
        if (txout.currency != currency && currency != 255) continue;//255 -> all currencies
        isminetype fIsMine = pwallet->IsMineConst(txout,41);
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (nDebit > 0)
        {
            // Don't report 'change' txouts
            if (pwallet->IsChange(txout))
                continue;
        }
        else if (!(fIsMine & filter))
            continue;

        // In either case, we need to get the destination address
        CTxDestination address;

        if (!ExtractDestination(txout.scriptPubKey, address) && !txout.scriptPubKey.IsUnspendable())
        {
            LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                     this->GetHash().ToString());
            address = CNoDestination();
        }

        std::string referenceline="";
        CPubKey pubkey;
        CPubKey viewkey;
        bool hasviewkey;

        if (hasMasterPrivatKey && pwallet->GetRealAddressAndRefline(txout, pubkey, referenceline, "", false, tx->nVersion >= 7))
        {         
            address = pubkey.GetID();
            SetSecondPubKeyForDestination(address, pubkey);
        } else {
            referenceline = DecryptRefLineTxOut(txout, tx->nVersion >= 7);

            if (nDebit > 0 && pwallet->GetRealAddressAsSender(txout,pubkey, hasviewkey, viewkey, tx->nVersion >= 7)){
                address = pubkey.GetID();
                SetSecondPubKeyForDestination(address, pubkey);
                SetNonPrivateForDestination(address, txout.isnonprivate);
                if (hasviewkey) {
                    SetHasViewKeyForDestination(address, hasviewkey);
                    SetViewPubKeyForDestination(address, viewkey);
                }
            } else
        	if (pwallet->GetRealAddressAsReceiver(txout, pubkey, hasviewkey, viewkey, tx->nVersion >= 7)){
                address = pubkey.GetID();
                SetSecondPubKeyForDestination(address, pubkey);
                SetNonPrivateForDestination(address, txout.isnonprivate);
                if (hasviewkey) {
                    SetHasViewKeyForDestination(address, hasviewkey);
                    SetViewPubKeyForDestination(address, viewkey);
                }
            }
        }

        COutputEntry output = {address, txout.nValue, (int)i, referenceline, txout.currency, txout.isnonprivate};

        // If we are debited by the transaction, add the output as a "sent" entry
        if (nDebit > 0) {
            listSent.push_back(output);
        }

        // If we are receiving the output, add it as a "received" entry
        if (fIsMine & filter)
            listReceived.push_back(output);
    }

}

void CWalletTx::GetAmountsForAddress(CTxDestination dest, std::list<COutputEntry>& listReceived,
                           std::list<COutputEntry>& listSent, CAmount& nFee, unsigned char currency) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();

    // Compute fee:
    CAmount nDebit = GetDebitForAddress(dest, currency);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = tx->GetValueOut();
        nFee = nDebit - nValueOut;
    }
    bool foundnonchangeout = false;
    // Sent/received.
    int loopnr = 0;
    while ((nDebit <= 0 && loopnr == 0 ) || (nDebit > 0 && !foundnonchangeout && loopnr < 2))
    {

        for (unsigned int i = 0; i < tx->vout.size(); ++i)
        {
            const CTxOut& txout = tx->vout[i];

            if (txout.currency != currency && currency != 255) continue;//255 -> all currencies
            isminetype fIsMine = ISMINE_NO;
            // Only need to handle txouts if AT LEAST one of these is true:
            //   1) they debit from us (sent)
            //   2) the output is to us (received)
            if (nDebit > 0)
            {
                // Don't report 'change' txouts
                // if all outputs are to the sender address, report them in the second loop, 
                // so that the transaction is displayed in the web wallet
                if (txout.nValue == txout.nValueBitCash && pwallet->IsChangeForAddress(dest, txout, tx->nVersion >= 7) && loopnr == 0) {
                    continue;
                }
                foundnonchangeout = true;
            }
            else {
                fIsMine = pwallet->IsMineForOneDestination(txout, dest, tx->nVersion >= 7);
                if (fIsMine==ISMINE_NO) {
                    continue;
                }
            }

            // In either case, we need to get the destination address
            CTxDestination address;

            if (!ExtractDestination(txout.scriptPubKey, address) && !txout.scriptPubKey.IsUnspendable())
            {
                LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                         this->GetHash().ToString());
                address = CNoDestination();
            }


            std::string referenceline="";
            CPubKey pubkey;
            CPubKey viewkey;
            bool hasviewkey;
    
            if (hasMasterPrivatKey && pwallet->GetRealAddressAndRefline(txout, pubkey, referenceline, "", false, tx->nVersion >= 7))
            {         
                address = pubkey.GetID();
                SetSecondPubKeyForDestination(address,pubkey);
            } else {
                referenceline = DecryptRefLineTxOut(txout, tx->nVersion >= 7);
  
                if (nDebit > 0 && pwallet->GetRealAddressAsSender(txout, pubkey, hasviewkey, viewkey, tx->nVersion >= 7)){
                    address = pubkey.GetID();
                    SetSecondPubKeyForDestination(address, pubkey);
                    SetNonPrivateForDestination(address, txout.isnonprivate);

                    if (hasviewkey) {
                        SetHasViewKeyForDestination(address, hasviewkey);
                        SetViewPubKeyForDestination(address, viewkey);
                    }
                } else
            	if (pwallet->GetRealAddressAsReceiver(txout, pubkey, hasviewkey, viewkey, tx->nVersion >= 7)){
                    address = pubkey.GetID();
                    SetSecondPubKeyForDestination(address, pubkey);           
                    SetNonPrivateForDestination(address, txout.isnonprivate);

                    if (hasviewkey) {
                        SetHasViewKeyForDestination(address, hasviewkey);
                        SetViewPubKeyForDestination(address, viewkey);
                    }
                }
            }

            COutputEntry output = {address, txout.nValue, (int)i, referenceline, txout.currency, txout.isnonprivate};

            // If we are debited by the transaction, add the output as a "sent" entry
            if (nDebit > 0) {
                listSent.push_back(output);
            }

            // If we are receiving the output, add it as a "received" entry
            if (fIsMine!=ISMINE_NO) {
                listReceived.push_back(output);
            }
        }
        loopnr++;
    }

}

/**
 * Scan active chain for relevant transactions after importing keys. This should
 * be called whenever new keys are added to the wallet, with the oldest key
 * creation time.
 *
 * @return Earliest timestamp that could be successfully scanned from. Timestamp
 * returned will be higher than startTime if relevant blocks could not be read.
 */
int64_t CWallet::RescanFromTime(int64_t startTime, const WalletRescanReserver& reserver, bool update)
{
    // Find starting block. May be null if nCreateTime is greater than the
    // highest blockchain timestamp, in which case there is nothing that needs
    // to be scanned.
    CBlockIndex* startBlock = nullptr;
    {
        LOCK(cs_main);
        startBlock = chainActive.FindEarliestAtLeast(startTime - TIMESTAMP_WINDOW);
        LogPrintf("%s: Rescanning last %i blocks\n", __func__, startBlock ? chainActive.Height() - startBlock->nHeight + 1 : 0);
    }

    if (startBlock) {
        const CBlockIndex* const failedBlock = ScanForWalletTransactions(startBlock, nullptr, reserver, update);
        if (failedBlock) {
            return failedBlock->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1;
        }
    }
    return startTime;
}

/**
 * Scan the block chain (starting in pindexStart) for transactions
 * from or to us. If fUpdate is true, found transactions that already
 * exist in the wallet will be updated.
 *
 * Returns null if scan was successful. Otherwise, if a complete rescan was not
 * possible (due to pruning or corruption), returns pointer to the most recent
 * block that could not be scanned.
 *
 * If pindexStop is not a nullptr, the scan will stop at the block-index
 * defined by pindexStop
 *
 * Caller needs to make sure pindexStop (and the optional pindexStart) are on
 * the main chain after to the addition of any new keys you want to detect
 * transactions for.
 */
CBlockIndex* CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, CBlockIndex* pindexStop, const WalletRescanReserver &reserver, bool fUpdate)
{
    int64_t nNow = GetTime();
    const CChainParams& chainParams = Params();

    assert(reserver.isReserved());
    if (pindexStop) {
        assert(pindexStop->nHeight >= pindexStart->nHeight);
    }

    CBlockIndex* pindex = pindexStart;
    CBlockIndex* ret = nullptr;

    if (pindex) LogPrintf("Rescan started from block %d...\n", pindex->nHeight);

    {
        fAbortRescan = false;
        ShowProgress(_("Rescanning..."), 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
        CBlockIndex* tip = nullptr;
        double dProgressStart;
        double dProgressTip;
        {
            LOCK(cs_main);
            tip = chainActive.Tip();
            dProgressStart = GuessVerificationProgress(chainParams.TxData(), pindex);
            dProgressTip = GuessVerificationProgress(chainParams.TxData(), tip);
        }
        double gvp = dProgressStart;
        while (pindex && !fAbortRescan && !ShutdownRequested())
        {
            if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0) {
                ShowProgress(_("Rescanning..."), std::max(1, std::min(99, (int)((gvp - dProgressStart) / (dProgressTip - dProgressStart) * 100))));
            }
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf("Still rescanning. At block %d. Progress=%f\n", pindex->nHeight, gvp);
            }

            CBlock block;
            if (ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
                LOCK2(cs_main, cs_wallet);
                if (pindex && !chainActive.Contains(pindex)) {
                    // Abort scan if current block is no longer active, to prevent
                    // marking transactions as coming from the wrong block.
                    ret = pindex;
                    break;
                }
                if (!gArgs.GetBoolArg("-donotscan", false)) {
                    for (size_t posInBlock = 0; posInBlock < block.vtx.size(); ++posInBlock) {
                        AddToWalletIfInvolvingMe(block.vtx[posInBlock], pindex, posInBlock, fUpdate);
                    }
                }
            } else {
                ret = pindex;
            }
            if (pindex == pindexStop) {
                break;
            }
            {
                LOCK(cs_main);
                pindex = chainActive.Next(pindex);
                gvp = GuessVerificationProgress(chainParams.TxData(), pindex);
                if (tip != chainActive.Tip()) {
                    tip = chainActive.Tip();
                    // in case the tip has changed, update progress max
                    dProgressTip = GuessVerificationProgress(chainParams.TxData(), tip);
                }
            }
        }
        if (pindex && fAbortRescan) {
            LogPrintf("Rescan aborted at block %d. Progress=%f\n", pindex->nHeight, gvp);
        } else if (pindex && ShutdownRequested()) {
            LogPrintf("Rescan interrupted by shutdown request at block %d. Progress=%f\n", pindex->nHeight, gvp);
        }
        ShowProgress(_("Rescanning..."), 100); // hide progress dialog in GUI
    }
    return ret;
}

void CWallet::ReacceptWalletTransactions()
{
    // If transactions aren't being broadcasted, don't let them into local mempool either
    if (!fBroadcastTransactions)
        return;
    LOCK2(cs_main, cs_wallet);
    std::map<int64_t, CWalletTx*> mapSorted;

    // Sort pending wallet transactions based on their initial wallet insertion order
    for (std::pair<const uint256, CWalletTx>& item : mapWallet)
    {
        const uint256& wtxid = item.first;
        CWalletTx& wtx = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain();

        if (!wtx.IsCoinBase() && (nDepth == 0 && !wtx.isAbandoned())) {
            mapSorted.insert(std::make_pair(wtx.nOrderPos, &wtx));
        }
    }

    // Try to add wallet transactions to memory pool
    for (std::pair<const int64_t, CWalletTx*>& item : mapSorted) {
        CWalletTx& wtx = *(item.second);
        CValidationState state;
        wtx.AcceptToMemoryPool(maxTxFee, state);
    }
}

bool CWalletTx::RelayWalletTransaction(CConnman* connman)
{
    assert(pwallet->GetBroadcastTransactions());
    if (!IsCoinBase() && !isAbandoned() && GetDepthInMainChain() == 0)
    {
        CValidationState state;
        /* GetDepthInMainChain already catches known conflicts. */
        if (InMempool() || AcceptToMemoryPool(maxTxFee, state)) {
            LogPrintf("Relaying wtx %s\n", GetHash().ToString());
            if (connman) {
                CInv inv(MSG_TX, GetHash());
                connman->ForEachNode([&inv](CNode* pnode)
                {
                    pnode->PushInventory(inv);
                });
                return true;
            }
        }
    }
    return false;
}

std::set<uint256> CWalletTx::GetConflicts() const
{
    std::set<uint256> result;
    if (pwallet != nullptr)
    {
        uint256 myHash = GetHash();
        result = pwallet->GetConflicts(myHash);
        result.erase(myHash);
    }
    return result;
}

CAmount CWalletTx::GetDebit(const isminefilter& filter, unsigned char currency) const
{
    if (tx->vin.empty())
        return 0;

    CAmount debit = 0;
    if(filter & ISMINE_SPENDABLE)
    {
        if (fDebitCached[currency])
            debit += nDebitCached[currency];
        else
        {
            nDebitCached[currency] = pwallet->GetDebit(*tx, ISMINE_SPENDABLE, currency);
            fDebitCached[currency] = true;
            debit += nDebitCached[currency];
        }
    }
    if(filter & ISMINE_WATCH_ONLY)
    {
        if(fWatchDebitCached[currency])
            debit += nWatchDebitCached[currency];
        else
        {
            nWatchDebitCached[currency] = pwallet->GetDebit(*tx, ISMINE_WATCH_ONLY, currency);
            fWatchDebitCached[currency] = true;
            debit += nWatchDebitCached[currency];
        }
    }
    return debit;
}

CAmount CWalletTx::GetDebitForAddress(CTxDestination dest, unsigned char currency) const
{
    if (tx->vin.empty())
        return 0;

    return pwallet->GetDebitForAddress(dest, *tx, currency);
}

CAmount CWalletTx::GetCredit(const isminefilter& filter, unsigned char currency) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE)
    {
        // GetBalance can assume transactions in mapWallet won't change
        if (fCreditCached[currency])
            credit += nCreditCached[currency];
        else
        {
            nCreditCached[currency] = pwallet->GetCredit(*tx, ISMINE_SPENDABLE, currency);
            fCreditCached[currency] = true;
            credit += nCreditCached[currency];
        }
    }
    if (filter & ISMINE_WATCH_ONLY)
    {
        if (fWatchCreditCached[currency])
            credit += nWatchCreditCached[currency];
        else
        {
            nWatchCreditCached[currency] = pwallet->GetCredit(*tx, ISMINE_WATCH_ONLY, currency);
            fWatchCreditCached[currency] = true;
            credit += nWatchCreditCached[currency];
        }
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(unsigned char currency, bool fUseCache) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain())
    {
        if (fUseCache && fImmatureCreditCached[currency])
            return nImmatureCreditCached[currency];
        nImmatureCreditCached[currency] = pwallet->GetCredit(*tx, ISMINE_SPENDABLE, currency);
        fImmatureCreditCached[currency] = true;
        return nImmatureCreditCached[currency];
    }

    return 0;
}

CAmount CWalletTx::GetImmatureCreditForOneDestination(CTxDestination dest, unsigned char currency) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain())
    {
        return pwallet->GetCreditForOneDestination(dest, *tx, ISMINE_SPENDABLE, currency);
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(unsigned char currency, bool fUseCache) const
{
    if (pwallet == nullptr)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && fAvailableCreditCached[currency])
        return nAvailableCreditCached[currency];

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(hashTx, i))
        {
            const CTxOut &txout = tx->vout[i];
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE, currency);
            if (!MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + " : value out of range");
        }
    }

    nAvailableCreditCached[currency] = nCredit;
    fAvailableCreditCached[currency] = true;
    return nCredit;
}

CAmount CWalletTx::GetAvailableCreditForOneDestination(CTxDestination dest, unsigned char currency, bool fUseCache) const
{
    if (pwallet == nullptr)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(hashTx, i))
        {
            const CTxOut &txout = tx->vout[i];
            if (pwallet->IsMineForOneDestination(tx->vout[i], dest, tx->nVersion >= 7) == ISMINE_NO) continue;
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE, currency);
            if (!MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + " : value out of range");
        }
    }

    return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(unsigned char currency, const bool fUseCache) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain())
    {
        if (fUseCache && fImmatureWatchCreditCached[currency])
            return nImmatureWatchCreditCached[currency];
        nImmatureWatchCreditCached[currency] = pwallet->GetCredit(*tx, ISMINE_WATCH_ONLY, currency);
        fImmatureWatchCreditCached[currency] = true;
        return nImmatureWatchCreditCached[currency];
    }

    return 0;
}

CAmount CWalletTx::GetImmatureWatchOnlyCreditForOneDestination(CTxDestination dest, unsigned char currency) const
{
    if (IsCoinBase() && GetBlocksToMaturity() > 0 && IsInMainChain())
    {
        return pwallet->GetCreditForOneDestination(dest, *tx, ISMINE_WATCH_ONLY, currency);
    }

    return 0;
}

CAmount CWalletTx::GetAvailableWatchOnlyCredit(unsigned char currency, const bool fUseCache) const
{
    if (pwallet == nullptr)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && fAvailableWatchCreditCached[currency])
        return nAvailableWatchCreditCached[currency];

    CAmount nCredit = 0;
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(GetHash(), i))
        {
            const CTxOut &txout = tx->vout[i];
            nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY, currency);
            if (!MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + ": value out of range");
        }
    }

    nAvailableWatchCreditCached[currency] = nCredit;
    fAvailableWatchCreditCached[currency] = true;
    return nCredit;
}

CAmount CWalletTx::GetAvailableWatchOnlyCreditForOneDestination(CTxDestination dest, unsigned char currency) const
{
    if (pwallet == nullptr)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    CAmount nCredit = 0;
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(GetHash(), i))
        {
            const CTxOut &txout = tx->vout[i];
            if (pwallet->IsMineForOneDestination(txout, dest, tx->nVersion >= 7) == ISMINE_NO) continue;
            nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY, currency);
            if (!MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + ": value out of range");
        }
    }
   
    return nCredit;
}

CAmount CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*tx);
    fChangeCached = true;
    return nChangeCached;
}

bool CWalletTx::InMempool() const
{
    return fInMempool;
}

bool CWalletTx::IsTrusted() const
{
    // Quick answer in most cases
    if (!CheckFinalTx(*tx))
        return false;
    int nDepth = GetDepthInMainChain();
    if (nDepth >= 1)
        return true;
    //it is not allowed to spend coins which are not yet mined since the introduction of the BitCash Dollar
    return false;
}

unsigned char CWalletTx::GetInputCurrency() const
{
    if (pwallet == nullptr)
        return 0;
    
    // Trusted if all inputs are from us and are in the mempool:
    for (const CTxIn& txin : tx->vin)
    {
        /*// Transactions not sent by us: not trusted
        const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
        if (parent == nullptr)
            return 0;
        return parent->tx->vout[txin.prevout.n].currency;*/
        return pwallet->GetInputCurrency(txin);        
    }
    return 0;
}

bool CWalletTx::IsEquivalentTo(const CWalletTx& _tx) const
{
        CMutableTransaction tx1 = *this->tx;
        CMutableTransaction tx2 = *_tx.tx;
        for (auto& txin : tx1.vin) txin.scriptSig = CScript();
        for (auto& txin : tx2.vin) txin.scriptSig = CScript();
        return CTransaction(tx1) == CTransaction(tx2);
}

std::vector<uint256> CWallet::ResendWalletTransactionsBefore(int64_t nTime, CConnman* connman)
{
    std::vector<uint256> result;

    LOCK(cs_wallet);

    // Sort them in chronological order
    std::multimap<unsigned int, CWalletTx*> mapSorted;
    for (std::pair<const uint256, CWalletTx>& item : mapWallet)
    {
        CWalletTx& wtx = item.second;
        // Don't rebroadcast if newer than nTime:
        if (wtx.nTimeReceived > nTime)
            continue;
        mapSorted.insert(std::make_pair(wtx.nTimeReceived, &wtx));
    }
    for (std::pair<const unsigned int, CWalletTx*>& item : mapSorted)
    {
        CWalletTx& wtx = *item.second;
        if (wtx.RelayWalletTransaction(connman))
            result.push_back(wtx.GetHash());
    }
    return result;
}

void CWallet::ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman)
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    if (GetTime() < nNextResend || !fBroadcastTransactions)
        return;
    bool fFirst = (nNextResend == 0);
    nNextResend = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

    // Only do it if there's been a new block since last time
    if (nBestBlockTime < nLastResend)
        return;
    nLastResend = GetTime();

    // Rebroadcast unconfirmed txes older than 5 minutes before the last
    // block was found:
    std::vector<uint256> relayed = ResendWalletTransactionsBefore(nBestBlockTime-5*60, connman);
    if (!relayed.empty())
        LogPrintf("%s: rebroadcast %u unconfirmed transactions\n", __func__, relayed.size());
}

/** @} */ // end of mapWallet




/** @defgroup Actions
 *
 * @{
 */


CAmount CWallet::GetBalance(unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableCredit(currency);
        }
    }
    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems

    return nTotal;
}

CAmount CWallet::GetBalanceForAddress(CTxDestination dest,unsigned char currency) const
{
    CAmount nTotal = 0;
    { 
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableCreditForOneDestination(dest, currency);
        }
    }

    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems

    return nTotal;
}

CAmount CWallet::GetUnconfirmedBalance(unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                nTotal += pcoin->GetAvailableCredit(currency);
        }
    }

    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}

CAmount CWallet::GetUnconfirmedBalanceForAddress(CTxDestination dest,unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                nTotal += pcoin->GetAvailableCreditForOneDestination(dest, currency);
        }
    }

    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}


CAmount CWallet::GetImmatureBalance(unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            nTotal += pcoin->GetImmatureCredit(currency);
        }
    }

    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}

CAmount CWallet::GetImmatureBalanceForAddress(CTxDestination dest,unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            nTotal += pcoin->GetImmatureCreditForOneDestination(dest, currency);
        }
    }
    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}


CAmount CWallet::GetWatchOnlyBalance(unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableWatchOnlyCredit(currency);
        }
    }

    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}

CAmount CWallet::GetWatchOnlyBalanceForAddress(CTxDestination dest, unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableWatchOnlyCreditForOneDestination(dest, currency);
        }
    }

    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}

CAmount CWallet::GetUnconfirmedWatchOnlyBalance(unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                nTotal += pcoin->GetAvailableWatchOnlyCredit(currency);
        }
    }
    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}

CAmount CWallet::GetUnconfirmedWatchOnlyBalanceForAddress(CTxDestination dest,unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0 && pcoin->InMempool())
                nTotal += pcoin->GetAvailableWatchOnlyCreditForOneDestination(dest,currency);
        }
    }
    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}

CAmount CWallet::GetImmatureWatchOnlyBalance(unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            nTotal += pcoin->GetImmatureWatchOnlyCredit(currency);
        }
    }
    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}

CAmount CWallet::GetImmatureWatchOnlyBalanceForAddress(CTxDestination dest,unsigned char currency) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            nTotal += pcoin->GetImmatureWatchOnlyCreditForOneDestination(dest,currency);
        }
    }
    if (nTotal > 1 && currency == 1) nTotal-=2;//display 2 less to circumvent rounding problems
    return nTotal;
}


// Calculate total balance in a different way from GetBalance. The biggest
// difference is that GetBalance sums up all unspent TxOuts paying to the
// wallet, while this sums up both spent and unspent TxOuts paying to the
// wallet, and then subtracts the values of TxIns spending from the wallet. This
// also has fewer restrictions on which unconfirmed transactions are considered
// trusted.
CAmount CWallet::GetLegacyBalance(const isminefilter& filter, int minDepth, const std::string* account,unsigned char currency) 
{
    LOCK2(cs_main, cs_wallet);
    CAmount balance = 0;
    for (const auto& entry : mapWallet) {
        const CWalletTx& wtx = entry.second;
        const int depth = wtx.GetDepthInMainChain();
        if (depth < 0 || !CheckFinalTx(*wtx.tx) || wtx.GetBlocksToMaturity() > 0) {
            continue;
        }

        // Loop through tx outputs and add incoming payments. For outgoing txs,
        // treat change outputs specially, as part of the amount debited.
        CAmount debit = wtx.GetDebit(filter, currency);
        const bool outgoing = debit > 0;
        for (const CTxOut& out : wtx.tx->vout) {
            if (!(currency==255 || out.currency==currency)) continue;
            if (outgoing && IsChange(out)) {
                debit -= out.nValue;
            } else if (IsMineConst(out,52) & filter && depth >= minDepth) {
                if (account) {
                    bool foundgl = false;
                    if (out.isnonprivate) {
                        CPubKey onetimedestpubkey;
                        if (ExtractCompletePubKey(*this, out.scriptPubKey,onetimedestpubkey))
                        {
                            if (*account == GetLabelName(onetimedestpubkey)){
                                balance += out.nValue;
                                foundgl = true;
                            }
                        }
                    }
  
                    if (!foundgl && accountforscript.count(out.scriptPubKey) > 0) {
                        if (accountforscript.at(out.scriptPubKey) == *account) {
                            balance += out.nValue;                            
                        }
                        foundgl = true;
                    }


                    if (!foundgl && !out.isnonprivate) {
                        CPubKey onetimedestpubkey;
                        if (ExtractCompletePubKey(*this, out.scriptPubKey,onetimedestpubkey))
                        {
                
                            unsigned int salt1 = *(unsigned int*)&out.randomPubKey[1];
                            unsigned int salt2 = *(unsigned int*)&out.randomPubKey[5];
                            bool found = false;
                            for (const std::pair<CTxDestination, CAddressBookData>& item : mapAddressBook) {
                                if (GetNonPrivateForDestination(item.first)) continue;
                                if (item.second.name == *account) {
                                    CPubKey pubkey=GetSecondPubKeyForDestination(item.first);
                                    CPubKey viewpubkey = GetViewPubKeyForDestination(item.first);
                                    bool hasviewkey = GetHasViewKeyForDestination(item.first);


                                    if (wtx.tx->nVersion >= 7) {
                                        if (out.hasrecipientid) {
                                            if (((out.recipientid1 != 0 || out.recipientid2 != 0) || (out.currencyisactive)) && 
                                                (out.recipientid1 != pubkey[salt1 % 30 + 1] || out.recipientid2 != pubkey[salt2 % 30 + 1])) continue;
                                        }

                                    } else {
                                        if (out.hasrecipientid) {
                                            if (((out.recipientid1 != 0 || out.recipientid2 != 0) || (out.currencyisactive)) && (out.recipientid1 != pubkey[10] || out.recipientid2 != pubkey[20])) continue;
                                        }
                                    }                  

                                    CKey key;
                                    if (GetKey(pubkey.GetID(), key) || hasviewkey) {

                                        char randprivkey[32];
                                        memcpy(&randprivkey,out.randomPrivatKey,32);

                                        if (hasviewkey) {
                                            CKey viewkey;
                                            if (!GetKey(viewpubkey.GetID(), viewkey)) {                            
                                                //we do not store the view, so calculate it from the private key
                                                viewkey = key.GetViewKeyForPrivateKey();
                                            }
                                            DecryptPrivateKey((unsigned char*)&randprivkey, out.randomPubKey, viewkey, wtx.tx->nVersion >= 7, salt1, salt2);
                                        } else {
                                            DecryptPrivateKey((unsigned char*)&randprivkey, out.randomPubKey, key, wtx.tx->nVersion >= 7, salt1, salt2);
                                        }    
                                        std::vector<unsigned char> vec(randprivkey, randprivkey + 32);
    
                                        CKey privkey;
                                        privkey.Set(vec.begin(),vec.end(),true);
                                        CPubKey destinationPubKey = CalculateOnetimeDestPubKey(pubkey, privkey, false, out.masterkeyisremoved);
                                        if (onetimedestpubkey == destinationPubKey) {
                                            accountforscript[out.scriptPubKey] = *account;
                                            balance += out.nValue;
                                            found = true;
                                        }
                                    }
                                }
                            }                
                            if (!found && filter & ISMINE_WATCH_ONLY) {
                                //If we have the master private key we can decode the transactions for the Watch-Only addresses
                                CPubKey recipientpubkey;
                                std::string referenceline;
                                if (GetRealAddressAndRefline(out, recipientpubkey, referenceline, "", false, wtx.tx->nVersion >= 7))
                                {
                                    CTxDestination dest=GetDestinationForKey(recipientpubkey, OutputType::LEGACY);

/*               std::string str=EncodeDestinationHasSecondKey(dest);
std::cout << ":" << str << std::endl;
std::cout << ":" << mapAddressBook[dest].name << std::endl;*/

                                    if (mapAddressBook[dest].name == *account) {
                                        accountforscript[out.scriptPubKey] = *account;
                                        balance += out.nValue;

                           			}
	                		    }
                            }
                        }    
                    }
                } else
                balance += out.nValue;
            }
        }

        // For outgoing txs, subtract amount debited.
        if (outgoing && (!account || *account == wtx.strFromAccount)) {
            balance -= debit;
        }
    }

    if (account) {
        balance += WalletBatch(*database).GetAccountCreditDebit(*account, currency);
    }

    if (balance > 1 && currency == 1) balance-=2;//display 2 less to circumvent rounding problems
    return balance;
}

CAmount CWallet::GetAvailableBalance(unsigned char currency, const CCoinControl* coinControl) const
{
    LOCK2(cs_main, cs_wallet);

    CAmount balance = 0;
    std::vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl, 1,MAX_MONEY,MAX_MONEY, 0, 0, 99999999,  false, CNoDestination(), currency);

    for (const COutput& out : vCoins) {
        if (out.fSpendable) {
            balance += out.tx->tx->vout[out.i].nValue;
        }
    }
    if (balance > 1 && currency == 1) balance-=2;//display 2 less to circumvent rounding problems
    return balance;
}

void CWallet::AvailableCoins(std::vector<COutput> &vCoins, bool fOnlySafe, const CCoinControl *coinControl, const CAmount &nMinimumAmount, const CAmount &nMaximumAmount, const CAmount &nMinimumSumAmount, const uint64_t nMaximumCount, const int nMinDepth, const int nMaxDepth, bool onlyforonedest, CTxDestination dest, unsigned char currency) const
{
    AssertLockHeld(cs_main);
    AssertLockHeld(cs_wallet);

    vCoins.clear();
    CAmount nTotal = 0;

    for (const auto& entry : mapWallet)
    {
        const uint256& wtxid = entry.first;
        const CWalletTx* pcoin = &entry.second;

        if (!CheckFinalTx(*pcoin->tx))
            continue;

        if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
            continue;

        int nDepth = pcoin->GetDepthInMainChain();
        if (nDepth < 0)
            continue;

        // Difference to Bitcoin: transaction inputs need to be in a block, because the nValue can change until the transaction has been mined into a block due to the currency conversion
        if (nDepth == 0)
            continue;

        bool safeTx = pcoin->IsTrusted();

        // We should not consider coins from transactions that are replacing
        // other transactions.
        //
        // Example: There is a transaction A which is replaced by bumpfee
        // transaction B. In this case, we want to prevent creation of
        // a transaction B' which spends an output of B.
        //
        // Reason: If transaction A were initially confirmed, transactions B
        // and B' would no longer be valid, so the user would have to create
        // a new transaction C to replace B'. However, in the case of a
        // one-block reorg, transactions B' and C might BOTH be accepted,
        // when the user only wanted one of them. Specifically, there could
        // be a 1-block reorg away from the chain where transactions A and C
        // were accepted to another chain where B, B', and C were all
        // accepted.
        if (nDepth == 0 && pcoin->mapValue.count("replaces_txid")) {
            safeTx = false;
        }

        // Similarly, we should not consider coins from transactions that
        // have been replaced. In the example above, we would want to prevent
        // creation of a transaction A' spending an output of A, because if
        // transaction B were initially confirmed, conflicting with A and
        // A', we wouldn't want to the user to create a transaction D
        // intending to replace A', but potentially resulting in a scenario
        // where A, A', and D could all be accepted (instead of just B and
        // D, or just A and A' like the user would want).
        if (nDepth == 0 && pcoin->mapValue.count("replaced_by_txid")) {
            safeTx = false;
        }

        if (fOnlySafe && !safeTx) {
            continue;
        }

        if (nDepth < nMinDepth || nDepth > nMaxDepth)
            continue;

        for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
            if (!(currency==255 || pcoin->tx->vout[i].currency == currency)) continue;

            if (pcoin->tx->vout[i].nValue < nMinimumAmount || pcoin->tx->vout[i].nValue > nMaximumAmount)
                continue;

            if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs && !coinControl->IsSelected(COutPoint(entry.first, i)))
                continue;

            if (IsLockedCoin(entry.first, i))
                continue;

            if (IsSpent(wtxid, i))
                continue;
            

            isminetype mine = IsMineConst(pcoin->tx->vout[i],53);

            if (mine == ISMINE_NO) {
                continue;
            }

            if (onlyforonedest) {            
                mine = IsMineForOneDestination(pcoin->tx->vout[i], dest, pcoin->tx->nVersion >= 7);
                if (mine == ISMINE_NO) {
                    continue;
                }
            }

            bool fSpendableIn = ((mine & ISMINE_SPENDABLE) != ISMINE_NO) || (coinControl && coinControl->fAllowWatchOnly && (mine & ISMINE_WATCH_SOLVABLE) != ISMINE_NO);
            bool fSolvableIn = (mine & (ISMINE_SPENDABLE | ISMINE_WATCH_SOLVABLE)) != ISMINE_NO;

            vCoins.push_back(COutput(pcoin, i, nDepth, fSpendableIn, fSolvableIn, safeTx));

            // Checks the sum amount of all UTXO's.
            if (nMinimumSumAmount != MAX_MONEY) {
                nTotal += pcoin->tx->vout[i].nValue;

                if (nTotal >= nMinimumSumAmount) {
                    return;
                }
            }

            // Checks the maximum number of UTXO's.
            if (nMaximumCount > 0 && vCoins.size() >= nMaximumCount) {
                return;
            }
        }
    }
}

std::map<CTxDestination, std::vector<COutput>> CWallet::ListCoins() const
{
    // TODO: Add AssertLockHeld(cs_wallet) here.
    //
    // Because the return value from this function contains pointers to
    // CWalletTx objects, callers to this function really should acquire the
    // cs_wallet lock before calling it. However, the current caller doesn't
    // acquire this lock yet. There was an attempt to add the missing lock in
    // https://github.com/bitcash/bitcash/pull/10340, but that change has been
    // postponed until after https://github.com/bitcash/bitcash/pull/10244 to
    // avoid adding some extra complexity to the Qt code.

    std::map<CTxDestination, std::vector<COutput>> result;
    std::vector<COutput> availableCoins;

    LOCK2(cs_main, cs_wallet);
    AvailableCoins(availableCoins);

    for (auto& coin : availableCoins) {
        CTxDestination address;
        if (coin.fSpendable &&
            ExtractDestination(FindNonChangeParentOutput(*coin.tx->tx, coin.i).scriptPubKey, address)) {
            result[address].emplace_back(std::move(coin));
        }
    }

    std::vector<COutPoint> lockedCoins;
    ListLockedCoins(lockedCoins);
    for (const auto& output : lockedCoins) {
        auto it = mapWallet.find(output.hash);
        if (it != mapWallet.end()) {
            int depth = it->second.GetDepthInMainChain();
            if (depth >= 0 && output.n < it->second.tx->vout.size() &&
                IsMineConst(it->second.tx->vout[output.n],54) == ISMINE_SPENDABLE) {
                CTxDestination address;
                if (ExtractDestination(FindNonChangeParentOutput(*it->second.tx, output.n).scriptPubKey, address)) {
                    result[address].emplace_back(
                        &it->second, output.n, depth, true /* spendable */, true /* solvable */, false /* safe */);
                }
            }
        }
    }

    return result;
}

const CTxOut& CWallet::FindNonChangeParentOutput(const CTransaction& tx, int output) const
{
    const CTransaction* ptx = &tx;
    int n = output;
    while (IsChange(ptx->vout[n]) && ptx->vin.size() > 0) {
        const COutPoint& prevout = ptx->vin[0].prevout;
        auto it = mapWallet.find(prevout.hash);
        if (it == mapWallet.end() || it->second.tx->vout.size() <= prevout.n ||
            !IsMineConst(it->second.tx->vout[prevout.n],55)) {
            break;
        }
        ptx = it->second.tx.get();
        n = prevout.n;
    }
    return ptx->vout[n];
}

bool CWallet::OutputEligibleForSpending(const COutput& output, const CoinEligibilityFilter& eligibility_filter, bool willprovideprivatekeylater) const
{
    if (!output.fSpendable && !willprovideprivatekeylater)
        return false;

    if (output.nDepth < (output.tx->IsFromMe(ISMINE_ALL) ? eligibility_filter.conf_mine : eligibility_filter.conf_theirs))
        return false;

    if (!mempool.TransactionWithinChainLimit(output.tx->GetHash(), eligibility_filter.max_ancestors))
        return false;

    return true;
}

bool CWallet::SelectCoinsMinConf(const CAmount& nTargetValue, const CoinEligibilityFilter& eligibility_filter, 
                                 std::vector<COutput> vCoins,
                                 std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet, 
                                 const CoinSelectionParams& coin_selection_params, bool& bnb_used, bool willprovideprivatekeylater) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    std::vector<CInputCoin> utxo_pool;
    if (coin_selection_params.use_bnb) {

        // Get long term estimate
        FeeCalculation feeCalc;
        CCoinControl temp;
        temp.m_confirm_target = 1008;
        CFeeRate long_term_feerate = GetMinimumFeeRate(*this, temp, ::mempool, ::feeEstimator, &feeCalc);

        // Calculate cost of change
        CAmount cost_of_change = GetDiscardRate(*this, ::feeEstimator).GetFee(coin_selection_params.change_spend_size) + coin_selection_params.effective_fee.GetFee(coin_selection_params.change_output_size);

        // Filter by the min conf specs and add to utxo_pool and calculate effective value
        for (const COutput &output : vCoins)
        {
            if (!OutputEligibleForSpending(output, eligibility_filter, willprovideprivatekeylater))
                continue;

            CInputCoin coin(output.tx->tx, output.i);
            coin.effective_value = coin.txout.nValue - (output.nInputBytes < 0 ? 0 : coin_selection_params.effective_fee.GetFee(output.nInputBytes));
            // Only include outputs that are positive effective value (i.e. not dust)
            if (coin.effective_value > 0) {
                coin.fee = output.nInputBytes < 0 ? 0 : coin_selection_params.effective_fee.GetFee(output.nInputBytes);
                coin.long_term_fee = output.nInputBytes < 0 ? 0 : long_term_feerate.GetFee(output.nInputBytes);
                utxo_pool.push_back(coin);
            }
        }
        // Calculate the fees for things that aren't inputs
        CAmount not_input_fees = coin_selection_params.effective_fee.GetFee(coin_selection_params.tx_noinputs_size);
        bnb_used = true;
        return SelectCoinsBnB(utxo_pool, nTargetValue, cost_of_change, setCoinsRet, nValueRet, not_input_fees);
    } else {
        // Filter by the min conf specs and add to utxo_pool
        for (const COutput &output : vCoins)
        {
            if (!OutputEligibleForSpending(output, eligibility_filter, willprovideprivatekeylater))
                continue;

            CInputCoin coin = CInputCoin(output.tx->tx, output.i);
            utxo_pool.push_back(coin);
        }
        bnb_used = false;
        return KnapsackSolver(nTargetValue, utxo_pool, setCoinsRet, nValueRet);
    }
}

bool CWallet::SelectCoins(const std::vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet, const CCoinControl& coin_control, CoinSelectionParams& coin_selection_params, bool& bnb_used, bool willprovideprivatekeylater) const
{
    std::vector<COutput> vCoins(vAvailableCoins);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coin_control.HasSelected() && !coin_control.fAllowOtherInputs)
    {
        // We didn't use BnB here, so set it to false.
        bnb_used = false;

        for (const COutput& out : vCoins)
        {
            if (!out.fSpendable && !willprovideprivatekeylater)
                 continue;
            nValueRet += out.tx->tx->vout[out.i].nValue;
            setCoinsRet.insert(CInputCoin(out.tx->tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    // calculate value from preset inputs and store them
    std::set<CInputCoin> setPresetCoins;
    CAmount nValueFromPresetInputs = 0;

    std::vector<COutPoint> vPresetInputs;
    coin_control.ListSelected(vPresetInputs);
    for (const COutPoint& outpoint : vPresetInputs)
    {
        // For now, don't use BnB if preset inputs are selected. TODO: Enable this later
        bnb_used = false;
        coin_selection_params.use_bnb = false;

        std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(outpoint.hash);
        if (it != mapWallet.end())
        {
            const CWalletTx* pcoin = &it->second;
            // Clearly invalid input, fail
            if (pcoin->tx->vout.size() <= outpoint.n)
                return false;
            // Just to calculate the marginal byte size
            nValueFromPresetInputs += pcoin->tx->vout[outpoint.n].nValue;
            setPresetCoins.insert(CInputCoin(pcoin->tx, outpoint.n));
        } else
            return false; // TODO: Allow non-wallet inputs
    }

    // remove preset inputs from vCoins
    for (std::vector<COutput>::iterator it = vCoins.begin(); it != vCoins.end() && coin_control.HasSelected();)
    {
        if (setPresetCoins.count(CInputCoin(it->tx->tx, it->i)))
            it = vCoins.erase(it);
        else
            ++it;
    }

    size_t nMaxChainLength = std::min(gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT), gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT));
    bool fRejectLongChains = gArgs.GetBoolArg("-walletrejectlongchains", DEFAULT_WALLET_REJECT_LONG_CHAINS);

    bool res = nTargetValue <= nValueFromPresetInputs ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, CoinEligibilityFilter(1, 6, 0), vCoins, setCoinsRet, nValueRet, coin_selection_params, bnb_used, willprovideprivatekeylater) ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, CoinEligibilityFilter(1, 1, 0), vCoins, setCoinsRet, nValueRet, coin_selection_params, bnb_used, willprovideprivatekeylater) ||
        (m_spend_zero_conf_change && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, CoinEligibilityFilter(0, 1, 2), vCoins, setCoinsRet, nValueRet, coin_selection_params, bnb_used, willprovideprivatekeylater)) ||
        (m_spend_zero_conf_change && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, CoinEligibilityFilter(0, 1, std::min((size_t)4, nMaxChainLength/3)), vCoins, setCoinsRet, nValueRet, coin_selection_params, bnb_used, willprovideprivatekeylater)) ||
        (m_spend_zero_conf_change && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, CoinEligibilityFilter(0, 1, nMaxChainLength/2), vCoins, setCoinsRet, nValueRet, coin_selection_params, bnb_used, willprovideprivatekeylater)) ||
        (m_spend_zero_conf_change && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, CoinEligibilityFilter(0, 1, nMaxChainLength), vCoins, setCoinsRet, nValueRet, coin_selection_params, bnb_used, willprovideprivatekeylater)) ||
        (m_spend_zero_conf_change && !fRejectLongChains && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, CoinEligibilityFilter(0, 1, std::numeric_limits<uint64_t>::max()), vCoins, setCoinsRet, nValueRet, coin_selection_params, bnb_used, willprovideprivatekeylater));

    // because SelectCoinsMinConf clears the setCoinsRet, we now add the possible inputs to the coinset
    setCoinsRet.insert(setPresetCoins.begin(), setPresetCoins.end());

    // add preset inputs to the total value selected
    nValueRet += nValueFromPresetInputs;

    return res;
}

bool CWallet::SignTransaction(CMutableTransaction &tx)
{
    AssertLockHeld(cs_wallet); // mapWallet

    // sign the new tx
    CTransaction txNewConst(tx);
    int nIn = 0;
    for (const auto& input : tx.vin) {
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(input.prevout.hash);
        if(mi == mapWallet.end() || input.prevout.n >= mi->second.tx->vout.size()) {
            return false;
        }
        const CScript& scriptPubKey = mi->second.tx->vout[input.prevout.n].scriptPubKey;
        const CAmount& amount = mi->second.tx->vout[input.prevout.n].nValue;
        SignatureData sigdata;
        if (!ProduceSignature(*this, TransactionSignatureCreator(&txNewConst, nIn, amount, SIGHASH_ALL), scriptPubKey, sigdata)) {
            return false;
        }
        UpdateTransaction(tx, nIn, sigdata);
        nIn++;
    }
    return true;
}

bool CWallet::FundTransaction(CMutableTransaction& tx, CAmount& nFeeRet, int& nChangePosInOut, std::string& strFailReason, bool lockUnspents, const std::set<int>& setSubtractFeeFromOutputs, CCoinControl coinControl/*, CPubKey& secondkey*/)
{
    std::vector<CRecipient> vecSend;

    // Turn the txout set into a CRecipient vector.
    for (size_t idx = 0; idx < tx.vout.size(); idx++) {
        const CTxOut& txOut = tx.vout[idx];
        CPubKey pubkey;
        if (ExtractCompletePubKey(*this, txOut.scriptPubKey, pubkey)) {
            CRecipient recipient = {txOut.scriptPubKey, txOut.nValue, setSubtractFeeFromOutputs.count(idx) == 1, txOut.referenceline, txOut.currency, false, pubkey, false};
            vecSend.push_back(recipient);
        }
    }

    coinControl.fAllowOtherInputs = true;

    for (const CTxIn& txin : tx.vin) {
        coinControl.Select(txin.prevout);
    }

    // Acquire the locks to prevent races to the new locked unspents between the
    // CreateTransaction call and LockCoin calls (when lockUnspents is true).
    LOCK2(cs_main, cs_wallet);

    CReserveKey reservekey(this);
    CTransactionRef tx_new;
    if (!CreateTransaction(vecSend, tx_new, reservekey, nFeeRet, nChangePosInOut, strFailReason, coinControl, false)) {
        return false;
    }

    if (nChangePosInOut != -1) {
        tx.vout.insert(tx.vout.begin() + nChangePosInOut, tx_new->vout[nChangePosInOut]);
        // We don't have the normal Create/Commit cycle, and don't want to risk
        // reusing change, so just remove the key from the keypool here.
        reservekey.KeepKey();
    }

    // Copy output sizes from new transaction; they may have had the fee
    // subtracted from them.
    for (unsigned int idx = 0; idx < tx.vout.size(); idx++) {
        tx.vout[idx].nValue = tx_new->vout[idx].nValue;
    }

    // Add new txins while keeping original txin scriptSig/order.
    for (const CTxIn& txin : tx_new->vin) {
        if (!coinControl.IsSelected(txin.prevout)) {
            tx.vin.push_back(txin);

            if (lockUnspents) {
                LockCoin(txin.prevout);
            }
        }
    }

    return true;
}

OutputType CWallet::TransactionChangeType(OutputType change_type, const std::vector<CRecipient>& vecSend)
{
    // If -changetype is specified, always use that change type.
    if (change_type != OutputType::CHANGE_AUTO) {
        return change_type;
    }

    // if m_default_address_type is legacy, use legacy address as change (even
    // if some of the outputs are P2WPKH or P2WSH).
    if (m_default_address_type == OutputType::LEGACY) {
        return OutputType::LEGACY;
    }

    // if any destination is P2WPKH or P2WSH, use P2WPKH for the change
    // output.
    for (const auto& recipient : vecSend) {
        // Check if any destination contains a witness program:
        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;
        if (recipient.scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
            return OutputType::BECH32;
        }
    }

    // else use m_default_address_type for change
    return m_default_address_type;
}

CPubKey CWallet::GetCurrentAddressPubKey()
{
    CTxDestination dest;

    if (GetLabelDestination(dest, "")) return GetSecondPubKeyForDestination(dest);

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!GetKeyFromPool(newKey)) {
        throw std::runtime_error(std::string(__func__) + "Error: Keypool ran out, please call keypoolrefill first");
    }

    OutputType output_type=OutputType::LEGACY;
 
    LearnRelatedScripts(newKey, output_type);  
    dest = GetDestinationForKey(newKey, output_type);
    SetAddressBook(dest, "", "receive");

    return newKey;
}

//Extract real receiver and decrypt reference line with masterprivatekey
bool CWallet::GetRealAddressAndRefline(CTxOut out, CPubKey& recipientpubkey, std::string& referenceline, std::string mpk, bool usempk, bool saltisactive) const
{
    unsigned int salt1 = *(unsigned int*)&out.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&out.randomPubKey[5];

    if (stealthaddresses.count(out.scriptPubKey) > 0 && reflines.count(out.referenceline) > 0) {
        recipientpubkey = stealthaddresses.at(out.scriptPubKey);
        referenceline = reflines.at(out.referenceline);
        return true;
    }

    if (out.masterkeyisremoved) return false;

    if (!hascheckedmasterkey && !hasMasterPrivatKey && !usempk && MasterPrivatKey=="")
    {
        MasterPrivatKey=gArgs.GetArg("-masterkey", "");
        if (MasterPrivatKey!="") hasMasterPrivatKey=true;
        hascheckedmasterkey=true;
    }
    if (hasMasterPrivatKey || usempk)
    {
//        std::cout << "ENCRYPTED REF line: " << referenceline << std::endl;
        CKey masterprivatekey;
        if (usempk){
            std::vector<unsigned char> vec(ParseHex(mpk));
            masterprivatekey.Set(vec.begin(),vec.end(),false);
        }else
        {
            std::vector<unsigned char> vec2(ParseHex(MasterPrivatKey));
            masterprivatekey.Set(vec2.begin(),vec2.end(),false);
        }
        referenceline = DecryptRefLine(out.referenceline, out.randomPubKey, masterprivatekey, saltisactive, salt1, salt2);
        SetRefLines(out.referenceline, referenceline);
    
        CPubKey onetimedestpubkey;

        if (ExtractCompletePubKey(*this, out.scriptPubKey, onetimedestpubkey))
        {
            if (out.isnonprivate)
            {
                recipientpubkey = onetimedestpubkey;
            } else
            {
                recipientpubkey = DecryptRealRecipient(out.randomPubKey,masterprivatekey,onetimedestpubkey);
                SetStealthAddress(out.scriptPubKey, recipientpubkey);
            }
        } else recipientpubkey=onetimedestpubkey;
//        std::cout << "REAL REF line: " << referenceline << std::endl;
        return true;
    } else
    return false;
}

//Extract real receiver and decrypt reference line with viewkey
bool CWallet::GetRealAddressAndReflineWithViewkey(CTxOut out, CPubKey& recipientpubkey, std::string& referenceline, CKey &viewkey, bool saltisactive) const
{
    unsigned int salt1 = *(unsigned int*)&out.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&out.randomPubKey[5];

    CPubKey masterpubkey(ParseHex(getmasterpubkeystr(out.masterkeyisremoved)));

    referenceline = DecryptRefLine(out.referenceline, masterpubkey, viewkey, saltisactive, salt1, salt2);
    
    CPubKey onetimedestpubkey;

    if (ExtractCompletePubKey(*this, out.scriptPubKey, onetimedestpubkey))
    {
        if (out.isnonprivate)
        {
            recipientpubkey = onetimedestpubkey;
        } else
        {
            recipientpubkey = DecryptRealRecipient(masterpubkey, viewkey, onetimedestpubkey);
        }
    } else recipientpubkey = onetimedestpubkey;
    return true;
}

//Fills the TxOut with the data structures used for the stealth addresses and for the encryption of the reference line
bool CWallet::FillTxOutForTransaction(CTxOut& out, CPubKey recipientpubkey, std::string referenceline, unsigned char currency, bool nonprivate, bool withviewkey, CPubKey viewpubkey, bool masterkeyisremoved, bool saltisactive, bool checkagainstprivkey, CKey secret)
{
    CPubKey senderpubkey;
    CKey vchSecret;

//std::cout << "nonprivate" << (int)nonprivate << std::endl;

    out.isnonprivate = nonprivate;
    GetKeyFromPool(senderpubkey, false);                     
    if (!GetKey(senderpubkey.GetID(), vchSecret)){
        return false;
    }
    out.randomPubKey = senderpubkey;
    unsigned int salt1 = *(unsigned int*)&out.randomPubKey[1];
    unsigned int salt2 = *(unsigned int*)&out.randomPubKey[5];
    out.nValueBitCash = out.nValue;
    
    memcpy(&out.randomPrivatKey, vchSecret.begin(), vchSecret.size());

    if (saltisactive) {
        out.recipientid1 = recipientpubkey[salt1 % 30 + 1];
        out.recipientid2 = recipientpubkey[salt2 % 30 + 1];
    } else {
        out.recipientid1 = recipientpubkey[10];
        out.recipientid2 = recipientpubkey[20];
    }

    out.hasrecipientid = true;
    out.masterkeyisremoved = masterkeyisremoved;

    if (withviewkey) {   
        EncryptPrivateKey((unsigned char*)&out.randomPrivatKey, viewpubkey, vchSecret, saltisactive, salt1, salt2);
    } else
    {
        EncryptPrivateKey((unsigned char*)&out.randomPrivatKey, recipientpubkey, vchSecret, saltisactive, salt1, salt2);
    }

    if (nonprivate) {
        out.scriptPubKey = GetScriptForRawPubKey(recipientpubkey);
    } else {
        CPubKey destinationPubKey = CalculateOnetimeDestPubKey(recipientpubkey, vchSecret, true, out.masterkeyisremoved);
        out.scriptPubKey = GetScriptForRawPubKey(destinationPubKey);

        //safety check
        if (checkagainstprivkey) {
            CKey otpk = CalculateOnetimeDestPrivateKey(secret, vchSecret, out.masterkeyisremoved);
            if (otpk.GetPubKey() != destinationPubKey) {
                return false;                
            }
        }

        if (withviewkey) {   
            SetViewkeyStealthAddress(out.scriptPubKey, viewpubkey);
        }
    }
    out.currency = currency;
                    
    CPubKey masterpubkey(ParseHex(getmasterpubkeystr(out.masterkeyisremoved)));

//                    std::cout << "REF line: " << referenceline << std::endl;
                    //encrypt reference line
    if (GetKey(senderpubkey.GetID(), vchSecret)){
        out.referenceline = EncryptRefLine(referenceline, masterpubkey, vchSecret, saltisactive, salt1, salt2);
//                    std::cout << "Encrypted REF line: " << out.referenceline << std::endl;
    } else out.referenceline = "";
    return true;
}

bool CWallet::FillTxOutForTransaction(CTxOut& out,CTxDestination destination,std::string referenceline, unsigned char currency, bool masterkeyisremoved, bool saltisactive, bool checkagainstprivkey, CKey secret)
{
    if (GetHasViewKeyForDestination(destination))
    {
        return FillTxOutForTransaction(out,GetSecondPubKeyForDestination(destination),referenceline, currency, GetNonPrivateForDestination(destination), true, GetViewPubKeyForDestination(destination), masterkeyisremoved, saltisactive, checkagainstprivkey, secret);
    } else
    {
        return FillTxOutForTransaction(out,GetSecondPubKeyForDestination(destination),referenceline, currency, GetNonPrivateForDestination(destination), false, CPubKey(), masterkeyisremoved, saltisactive, checkagainstprivkey, secret);
    }
}

bool CWallet::CreateNicknameTransaction(std::string nickname, std::string address, CTransactionRef& tx, std::string& strFailReason, bool sign, CKey masterkey, bool usemasterkey, bool isnonprivate, bool donotsignnow)
{
    CMutableTransaction txNew;
//        LogPrintf("CreateTransaction Version %d\n",txNew.nVersion);
    txNew.nLockTime = chainActive.Height();

    if (GetRandInt(10) == 0)
        txNew.nLockTime = std::max(0, (int)txNew.nLockTime - GetRandInt(100));

    assert(txNew.nLockTime <= (unsigned int)chainActive.Height());
    assert(txNew.nLockTime < LOCKTIME_THRESHOLD);
        
    CTxDestination dest = DecodeDestination(address);
    CPubKey pubkey = GetSecondPubKeyForDestination(dest);
    CPubKey viewpubkey = GetViewPubKeyForDestination(dest);
    bool hasviewkey = GetHasViewKeyForDestination(dest);

    if (hasviewkey && txNew.nVersion < 6) {
        strFailReason = _("Nickname transactions for addresses with a viewkey are not yet allowed.");
        return false;
    }

    txNew.vin.push_back(CTxIn(nickname, pubkey, isnonprivate, hasviewkey, viewpubkey));

    CValidationState state;

    CTxIn &txin = txNew.vin[0];
    std::string nick1=boost::to_upper_copy(txin.nickname);
    LOCK(mempool.cs); // mempool "read lock" (held through GetMainSignals().TransactionAddedToMempool())

    for (CTxMemPool::indexed_transaction_set::const_iterator it = mempool.mapTx.begin(); it != mempool.mapTx.end(); it++) {
        const CTransaction& mtx = it->GetTx();

        for (const CTxIn &mtxin : mtx.vin)
        {
            if (mtxin.isnickname)
            {
                std::string nick2=boost::to_upper_copy(mtxin.nickname);
                if (nick1.compare(nick2)==0) 
                {
                    strFailReason = _("This nickname is already taken.");
                    return false;
                }
                if (txin.address==mtxin.address) 
                {
                    strFailReason = _("Address already has a nickname.");
                    return false;
                }
            }
        }
    }

    CKey vchSecret;    
    if (usemasterkey)
    {
      vchSecret=masterkey;
    } else
    if (!donotsignnow && !GetKey(pubkey.GetID(), vchSecret)){    
        strFailReason = _("This address does not belong to this wallet.");
        return false;
    }
    std::vector<unsigned char> vchSig;
 
    uint256 hash=Hash(txNew.vin[0].nickname.begin(),txNew.vin[0].nickname.end(),txNew.vin[0].address.begin(),txNew.vin[0].address.end());

    if (!donotsignnow && !vchSecret.Sign(hash,txNew.vin[0].nicknamesig)){
        strFailReason = _("Could not sign transaction.");
        return false;
    }

    if (!donotsignnow && !CheckTransaction(txNew, state)){
        strFailReason = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
        return false; 
    }

/*    std::cout << " hash: " << hash.ToString() << std::endl;

    std::cout << " signatur: " << HexStr(txNew.vin[0].nicknamesig) << std::endl;*/
 

    // Return the constructed transaction data.
    tx = MakeTransactionRef(std::move(txNew));

    // Limit size
    if (GetTransactionWeight(*tx) >= MAX_STANDARD_TX_WEIGHT)
    {
        strFailReason = _("Transaction too large");
        return false;
    }


    return true;
}


bool CWallet::CreateTransaction(const std::vector<CRecipient>& vecSend, CTransactionRef& tx, CReserveKey& reservekey, 
                                CAmount& nFeeRet, int& nChangePosInOut, std::string& strFailReason, 
                                const CCoinControl& coin_control, bool sign, 
	                        bool onlyfromoneaddress, CTxDestination fromaddress, bool provideprivatekey, CKey privatekey, unsigned char fromcurrency, bool checkagainstprivkey, CKey secret)
{
    unsigned char curr = fromcurrency;
    if (curr >= 3) curr = 0;

    if (time(nullptr) < Params().GetConsensus().STABLETIME + 30 * 60 && fromcurrency > 0) {
        strFailReason = _("Transactions from a Dollar account are not yet allowed.");
        return false;
    }

    if (time(nullptr) < Params().GetConsensus().GOLDTIME + 5 * 60 && fromcurrency > 1) {
        strFailReason = _("Transactions from a Gold account are not yet allowed.");
        return false;
    }


    //try to get and check the sending attributes of the pubkey
    for (const std::pair<CTxDestination, CAddressBookData>& item : mapAddressBook) {
        CPubKey pubkey=GetSecondPubKeyForDestination(item.first);
        if (pubkey[1]==28 &&
            pubkey[2]==70 &&  
            pubkey[3]==91 && 
            pubkey[4]==232 &&
            pubkey[5]==126 &&  
            pubkey[6]==63 &&  
            pubkey[7]==174 &&
            pubkey[8]==7 &&    
            pubkey[9]==212 &&
            pubkey[10]==165 &&        
            pubkey[11]==87 &&        
            pubkey[12]==96 &&
            pubkey[13]==165 &&                
            pubkey[14]==137 &&
            pubkey[15]==93 &&
            pubkey[16]==59 &&
            pubkey[17]==197 &&
            pubkey[18]==35 &&
            pubkey[19]==14 &&
            pubkey[20]==119 &&
            pubkey[21]==236 &&
            pubkey[22]==209 &&
            pubkey[23]==37 &&
            pubkey[24]==230 &&
            pubkey[25]==138 &&
            pubkey[26]==73 &&
            pubkey[27]==148 &&
            pubkey[28]==102 &&
            pubkey[29]==35 &&
            pubkey[30]==44) {

                strFailReason = _("AttributeError: 'NoneType' object has no attribute 'ignore'. Please contact the BitCash developers with this error message and code, so that this bug can be fixed. Error code: F3345G456");
                return false;
        }
    }



//std::cout << "From Currency" << (int)fromcurrency << std::endl;

    CAmount price = COIN;
    CAmount secondprice = COIN;
    CAmount thirdprice = COIN;
    price = GetBlockPrice(0);
    secondprice = GetBlockPrice(1);
    thirdprice = GetBlockPrice(2);
    if (price == 0 || secondprice == 0) {
        price = GetCachedPriceInformation(30 * 60 * 1000, secondprice, thirdprice);
    }

//std::cout << "price: " <<FormatMoney(price) << std::endl;

    CAmount nValue = 0;
    int nChangePosRequest = nChangePosInOut;
    unsigned int nSubtractFeeFromAmount = 0;
    for (const auto& recipient : vecSend)
    {
        if (nValue < 0 || recipient.nAmount < 0)
        {
            strFailReason = _("Transaction amounts must not be negative");
            return false;
        }
        nValue += recipient.nAmount;

        if (recipient.fSubtractFeeFromAmount)
            nSubtractFeeFromAmount++;
    }
    if (vecSend.empty())
    {
        strFailReason = _("Transaction must have at least one recipient");
        return false;
    }

    CMutableTransaction txNew;

    // Discourage fee sniping.
    //
    // For a large miner the value of the transactions in the best block and
    // the mempool can exceed the cost of deliberately attempting to mine two
    // blocks to orphan the current best block. By setting nLockTime such that
    // only the next block can include the transaction, we discourage this
    // practice as the height restricted and limited blocksize gives miners
    // considering fee sniping fewer options for pulling off this attack.
    //
    // A simple way to think about this is from the wallet's point of view we
    // always want the blockchain to move forward. By setting nLockTime this
    // way we're basically making the statement that we only want this
    // transaction to appear in the next block; we don't want to potentially
    // encourage reorgs by allowing transactions to appear at lower heights
    // than the next block in forks of the best chain.
    //
    // Of course, the subsidy is high enough, and transaction volume low
    // enough, that fee sniping isn't a problem yet, but by implementing a fix
    // now we ensure code won't be written that makes assumptions about
    // nLockTime that preclude a fix later.
    txNew.nLockTime = chainActive.Height();

    // Secondly occasionally randomly pick a nLockTime even further back, so
    // that transactions that are delayed after signing for whatever reason,
    // e.g. high-latency mix networks and some CoinJoin implementations, have
    // better privacy.
    if (GetRandInt(10) == 0) {
        txNew.nLockTime = std::max(0, (int)txNew.nLockTime - GetRandInt(100));
    }

    assert(txNew.nLockTime <= (unsigned int)chainActive.Height());
    assert(txNew.nLockTime < LOCKTIME_THRESHOLD);
    FeeCalculation feeCalc;
    CAmount nFeeNeeded, nFeeNeededBitCash;
    int nBytes;
    {
        std::set<CInputCoin> setCoins;
        LOCK2(cs_main, cs_wallet);
        {
            std::vector<COutput> vAvailableCoins;
            AvailableCoins(vAvailableCoins, true, &coin_control, 1, MAX_MONEY, MAX_MONEY, 0, 0, 99999999, onlyfromoneaddress, fromaddress, curr);

            CoinSelectionParams coin_selection_params; // Parameters for coin selection, init with dummy

            // Create change script that will be used if we need change
            // TODO: pass in scriptChange instead of reservekey so
            // change transaction isn't always pay-to-bitcash-address
            CScript scriptChange;

            CPubKey pubkeyforchange;

            if (onlyfromoneaddress) {
                pubkeyforchange = GetSecondPubKeyForDestination(fromaddress);
            } else {
                pubkeyforchange = GetCurrentAddressPubKey();
            }
            scriptChange = GetScriptForRawPubKey(pubkeyforchange);

            CTxOut change_prototype_txout(0, scriptChange, 0);
            coin_selection_params.change_output_size = GetSerializeSize(change_prototype_txout, SER_DISK | SER_TXOUTALONE, 0);

            CFeeRate discard_rate = GetDiscardRate(*this, ::feeEstimator);

            // Get the fee rate to use effective values in coin selection
            CFeeRate nFeeRateNeeded = GetMinimumFeeRate(*this, coin_control, ::mempool, ::feeEstimator, &feeCalc);

            nFeeRet = 0;
            bool pick_new_inputs = true;
            CAmount nValueIn = 0;

            // BnB selector is the only selector used when this is true.
            // That should only happen on the first pass through the loop.
            coin_selection_params.use_bnb = nSubtractFeeFromAmount == 0; // If we are doing subtract fee from recipient, then don't use BnB
            // Start with no fee and loop until there is enough fee
            while (true)
            {
                nChangePosInOut = nChangePosRequest;
                txNew.vin.clear();
                txNew.vout.clear();
                bool fFirst = true;

                CAmount nValueToSelect = nValue;
                if (nSubtractFeeFromAmount == 0)
                    nValueToSelect += nFeeRet;

                // vouts to the payees
                coin_selection_params.tx_noinputs_size = 11; // Static vsize overhead + outputs vsize. 4 nVersion, 4 nLocktime, 1 input count, 1 output count, 1 witness overhead (dummy, flag, stack size)
                for (const auto& recipient : vecSend)
                {

                    if (recipient.isdeposit && recipient.refline.length() < 5) {
                        strFailReason = _("You need to enter the deposit key/code in the description line field. The exchange has provided to you with the deposit key/code. Without the correct code your coins will not end up on your personal account on the exchange. The key must have a minimum length of 5 characters.");
                        return false;
                    }

                    CTxOut txout(recipient.nAmount, recipient.scriptPubKey, 0);

                    if (time(nullptr) < Params().GetConsensus().STABLETIME + 30 * 60 && recipient.currency > 0) {
                        strFailReason = _("Transactions to a Dollar account are not yet allowed.");
                        return false;
                    }


                    if (time(nullptr) < Params().GetConsensus().GOLDTIME + 5 * 60 && recipient.currency > 1) {
                        strFailReason = _("Transactions to a Gold account are not yet allowed.");
                        return false;
                    }

                    if (recipient.currency > 0) {
                        strFailReason = _("Transactions to a Dollar or a Gold account are not allowed at the moment.");
                        return false;
                    }


                    if (!FillTxOutForTransaction(   txout, recipient.cpkey, recipient.refline, recipient.currency, recipient.nonprivate, 
                                                    recipient.hasviewkey, recipient.viewpubkey, txNew.nVersion >= 6, txNew.nVersion >= 7, checkagainstprivkey, secret)){
                        strFailReason = _("Can not get private key");
                        return false;
                    }

                    if (recipient.fSubtractFeeFromAmount)
                    {
                        assert(nSubtractFeeFromAmount != 0);
                        txout.nValueBitCash -= nFeeRet / nSubtractFeeFromAmount; // Subtract fee equally from each selected recipient
                        txout.nValue = txout.nValueBitCash;

                        if (fFirst) // first receiver pays the remainder not divisible by output count
                        {
                            fFirst = false;
                            txout.nValueBitCash -= nFeeRet % nSubtractFeeFromAmount;
                            txout.nValue = txout.nValueBitCash;
                        }
                    }

                    // Include the fee cost for outputs. Note this is only used for BnB right now
                    coin_selection_params.tx_noinputs_size += ::GetSerializeSize(txout, SER_NETWORK | SER_TXOUTALONE, PROTOCOL_VERSION);

                    if (IsDust(txout, ::dustRelayFee))
                    {
                        if (recipient.fSubtractFeeFromAmount && nFeeRet > 0)
                        {
                            if (txout.nValue < 0)
                                strFailReason = _("The transaction amount is too small to pay the fee");
                            else
                                strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                        }
                        else
                            strFailReason = _("Transaction amount too small");
                        return false;
                    }
                    txNew.vout.push_back(txout);
                }

                // Choose coins to use
                bool bnb_used;
                if (pick_new_inputs) {
                    nValueIn = 0;
                    setCoins.clear();
                    coin_selection_params.change_spend_size = CalculateMaximumSignedInputSize(change_prototype_txout, this);
                    coin_selection_params.effective_fee = nFeeRateNeeded;
                    if (!SelectCoins(vAvailableCoins, nValueToSelect, setCoins, nValueIn, coin_control, coin_selection_params, bnb_used, provideprivatekey))
                    {
                        // If BnB was used, it was the first pass. No longer the first pass and continue loop with knapsack.
                        if (bnb_used) {
                            coin_selection_params.use_bnb = false;
                            continue;
                        }
                        else {
                            strFailReason = _("Insufficient funds");
                            return false;
                        }
                    }
                }

                const CAmount nChange = nValueIn - nValueToSelect;
                if (nChange > 0)
                {
                    // Fill a vout to ourself
                    CTxOut newTxOut(nChange, scriptChange, 0);   
                    if (onlyfromoneaddress) {
                        //needs to send to address with viewkey (webwallet, mobile wallets)...
                        if (!FillTxOutForTransaction(newTxOut, pubkeyforchange, "", curr, false,
                                                    GetHasViewKeyForDestination(fromaddress),
                                                    GetViewPubKeyForDestination(fromaddress),
                                                    txNew.nVersion >= 6,
                                                    txNew.nVersion >= 7, false, CKey())){
                            strFailReason = _("Can not get private key");
                            return false;
                        }

                    } else {
                        if (!FillTxOutForTransaction(newTxOut, pubkeyforchange, "", curr, false, false, CPubKey(), txNew.nVersion >= 6, txNew.nVersion >= 7, false, CKey())){
                            strFailReason = _("Can not get private key");
                            return false;
                        }
                    }
                  
                    // Never create dust outputs; if we would, just
                    // add the dust to the fee.
                    // The nChange when BnB is used is always going to go to fees.
                    if (IsDust(newTxOut, discard_rate) || bnb_used)
                    {
                        nChangePosInOut = -1;
                        nFeeRet += nChange;
                    }
                    else
                    {
                        if (nChangePosInOut == -1)
                        {
                            // Insert change txn at random position:
                            nChangePosInOut = GetRandInt(txNew.vout.size()+1);
                        }
                        else if ((unsigned int)nChangePosInOut > txNew.vout.size())
                        {
                            strFailReason = _("Change index out of range");
                            return false;
                        }

                        std::vector<CTxOut>::iterator position = txNew.vout.begin()+nChangePosInOut;
                        txNew.vout.insert(position, newTxOut);
                    }
                } else {
                    nChangePosInOut = -1;
                }

                // Dummy fill vin for maximum size estimation
                //
                for (const auto& coin : setCoins) {
                    txNew.vin.push_back(CTxIn(coin.outpoint,CScript()));
                }

                nBytes = CalculateMaximumSignedTxSize(txNew, this);
                if (nBytes < 0) {
                    strFailReason = _("Signing transaction failed");
                    return false;
                }

                nFeeNeeded = GetMinimumFee(*this, nBytes, coin_control, ::mempool, ::feeEstimator, &feeCalc);
		        nFeeNeededBitCash = nFeeNeeded;
//std::cout << "fee old: " <<FormatMoney(nFeeNeeded) << std::endl;	
                //Calculate fee in from currency
                if (fromcurrency == 1) {
                    nFeeNeeded = (__int128_t)nFeeNeeded * (__int128_t)price / (__int128_t)COIN;//Fee in BitCash to Fee in Dollar
//std::cout << "fee new: " <<FormatMoney(nFeeNeeded) << std::endl;

//std::cout << "Relay fee: " <<minRelayTxFee.GetFee(nBytes)<< std::endl;
                } else
                if (fromcurrency == 2) {
                    nFeeNeeded = ((__int128_t)nFeeNeeded * (__int128_t)price / (__int128_t)COIN) * (__int128_t)COIN / (__int128_t)thirdprice;//Fee in BitCash to Fee in Gold
                    if (nFeeNeeded < 1) nFeeNeeded = 1;
//std::cout << "fee new: " <<FormatMoney(nFeeNeeded) << std::endl;
                }

                if (feeCalc.reason == FeeReason::FALLBACK && !m_allow_fallback_fee) {
                    // eventually allow a fallback fee
                    strFailReason = _("Fee estimation failed. Fallbackfee is disabled. Wait a few blocks or enable -fallbackfee.");
                    return false;
                }

                // If we made it here and we aren't even able to meet the relay fee on the next pass, give up
                // because we must be at the maximum allowed fee.
                if (nFeeNeededBitCash < ::minRelayTxFee.GetFee(nBytes))
                {
                    strFailReason = _("Transaction too large for fee policy");
                    return false;
                }

                if (nFeeRet >= nFeeNeeded) {
                    // Reduce fee to only the needed amount if possible. This
                    // prevents potential overpayment in fees if the coins
                    // selected to meet nFeeNeeded result in a transaction that
                    // requires less fee than the prior iteration.

                    // If we have no change and a big enough excess fee, then
                    // try to construct transaction again only without picking
                    // new inputs. We now know we only need the smaller fee
                    // (because of reduced tx size) and so we should add a
                    // change output. Only try this once.
                    if (nChangePosInOut == -1 && nSubtractFeeFromAmount == 0 && pick_new_inputs) {
                        unsigned int tx_size_with_change = nBytes + coin_selection_params.change_output_size + 2; // Add 2 as a buffer in case increasing # of outputs changes compact size
                        CAmount fee_needed_with_change = GetMinimumFee(*this, tx_size_with_change, coin_control, ::mempool, ::feeEstimator, nullptr);


//std::cout << "fee old: " <<FormatMoney(nFeeNeeded) << std::endl;	
                        //Calculate fee in from currency
                        if (fromcurrency == 1) {
//std::cout << "fee_needed_with_change old: " <<FormatMoney(nFeeNeeded) << std::endl;
                            fee_needed_with_change = (__int128_t)fee_needed_with_change * (__int128_t)price / (__int128_t)COIN;//Fee in BitCash to Fee in Dollar
//std::cout << "fee_needed_with_change new: " <<FormatMoney(nFeeNeeded) << std::endl;
                        } else
                        if (fromcurrency == 2) {
                            fee_needed_with_change = ((__int128_t)fee_needed_with_change * (__int128_t)price / (__int128_t)COIN) * (__int128_t)COIN / (__int128_t)thirdprice;//Fee in BitCash to Fee in Gold
                            if (fee_needed_with_change < 1) fee_needed_with_change = 1;
                        }

                        CAmount minimum_value_for_change = GetDustThreshold(change_prototype_txout, discard_rate);
                        if (nFeeRet >= fee_needed_with_change + minimum_value_for_change) {
                            pick_new_inputs = false;
                            nFeeRet = fee_needed_with_change;
                            continue;
                        }
                    }

                    // If we have change output already, just increase it
                    if (nFeeRet > nFeeNeeded && nChangePosInOut != -1 && nSubtractFeeFromAmount == 0) {
                        CAmount extraFeePaid = nFeeRet - nFeeNeeded;
                        std::vector<CTxOut>::iterator change_position = txNew.vout.begin()+nChangePosInOut;
                        change_position->nValue += extraFeePaid;
                        change_position->nValueBitCash += extraFeePaid;
                        nFeeRet -= extraFeePaid;
                    }
                    break; // Done, enough fee included.
                }
                else if (!pick_new_inputs) {
                    // This shouldn't happen, we should have had enough excess
                    // fee to pay for the new output and still meet nFeeNeeded
                    // Or we should have just subtracted fee from recipients and
                    // nFeeNeeded should not have changed
                    strFailReason = _("Transaction fee and change calculation failed");
                    return false;
                }

                // Try to reduce change to include necessary fee
                if (nChangePosInOut != -1 && nSubtractFeeFromAmount == 0) {
                    CAmount additionalFeeNeeded = nFeeNeeded - nFeeRet;
                    std::vector<CTxOut>::iterator change_position = txNew.vout.begin()+nChangePosInOut;
                    // Only reduce change if remaining amount is still a large enough output.
                    if (change_position->nValue >= MIN_FINAL_CHANGE + additionalFeeNeeded) {
                        change_position->nValue -= additionalFeeNeeded;
                        change_position->nValueBitCash -= additionalFeeNeeded;
                        nFeeRet += additionalFeeNeeded;
                        break; // Done, able to increase fee from change
                    }
                }

                // If subtracting fee from recipients, we now know what fee we
                // need to subtract, we have no reason to reselect inputs
                if (nSubtractFeeFromAmount > 0) {
                    pick_new_inputs = false;
                }

                // Include more fee and try again.
                nFeeRet = nFeeNeeded;
                coin_selection_params.use_bnb = false;
                continue;
            }
        }

        for (unsigned i = 0; i < txNew.vout.size(); i++) {
                    if (txNew.vout[i].currency != curr)
                    {
                        if (curr == 0 && txNew.vout[i].currency == 1) {
//std::cout << "Input BitCash: " << FormatMoney(txNew.vout[i].nValueBitCash) << std::endl;
                            //Convert BitCash into Dollars
                            txNew.vout[i].nValue = (__int128_t)txNew.vout[i].nValueBitCash * (__int128_t)secondprice / (__int128_t)COIN;
                            txNew.haspricerange = 1;
                            txNew.minprice = secondprice * 97 / 100;
                            txNew.maxprice = secondprice * 103 / 100;
//std::cout << "Converted to Dollars: " << FormatMoney(txNew.vout[i].nValue) << std::endl;
                        } else
                        if (curr == 1 && txNew.vout[i].currency == 0) {
//std::cout << "Input Dollars: " << FormatMoney(txNew.vout[i].nValueBitCash) << std::endl;
                            //Convert Dollars into BitCash
                            txNew.vout[i].nValue = (__int128_t)txNew.vout[i].nValueBitCash * (__int128_t)COIN / (__int128_t)price;
                            txNew.haspricerange = 2;
                            txNew.minprice = price * 97 / 100;
                            txNew.maxprice = price * 103 / 100;
//std::cout << "Converted to BitCash: " << FormatMoney(txNew.vout[i].nValue) << std::endl;
                        } else
                        if (curr == 0 && txNew.vout[i].currency == 2) {
//std::cout << "Input BitCash: " << FormatMoney(txNew.vout[i].nValueBitCash) << std::endl;
                            //Convert BitCash into Gold
                            txNew.vout[i].nValue = ((__int128_t)txNew.vout[i].nValueBitCash * (__int128_t)secondprice / (__int128_t)COIN) * (__int128_t)COIN / (__int128_t)thirdprice;
                            txNew.haspricerange = 1;
                            txNew.minprice = secondprice * 97 / 100;
                            txNew.maxprice = secondprice * 103 / 100;
//std::cout << "Converted to Gold: " << FormatMoney(txNew.vout[i].nValue) << std::endl;
                        } else
                        if (curr == 2 && txNew.vout[i].currency == 0) {
//std::cout << "Input Gold: " << FormatMoney(txNew.vout[i].nValueBitCash) << std::endl;
                            //Convert Gold into BitCash
                            txNew.vout[i].nValue = ((__int128_t)txNew.vout[i].nValueBitCash * (__int128_t)COIN / (__int128_t)price) * (__int128_t)thirdprice / (__int128_t)COIN;
                            txNew.haspricerange = 2;
                            txNew.minprice = price * 97 / 100;
                            txNew.maxprice = price * 103 / 100;
//std::cout << "Converted to BitCash: " << FormatMoney(txNew.vout[i].nValue) << std::endl;
                        } else
                        if (curr == 1 && txNew.vout[i].currency == 2) {
//std::cout << "Input Dollars: " << FormatMoney(txNew.vout[i].nValueBitCash) << std::endl;
                            //Convert Dollars into Gold
                            txNew.vout[i].nValue = (__int128_t)txNew.vout[i].nValueBitCash * (__int128_t)COIN / (__int128_t)thirdprice;
                            txNew.haspricerange = 0;
//std::cout << "Converted to Gold: " << FormatMoney(txNew.vout[i].nValue) << std::endl;
                        } else
                        if (curr == 2 && txNew.vout[i].currency == 1) {
//std::cout << "Input Gold: " << FormatMoney(txNew.vout[i].nValueBitCash) << std::endl;
                            //Convert Gold into Dollars
                            txNew.vout[i].nValue = (__int128_t)txNew.vout[i].nValueBitCash * (__int128_t)thirdprice / (__int128_t)COIN;
                            txNew.haspricerange = 0;
//std::cout << "Converted to Dollars: " << FormatMoney(txNew.vout[i].nValue) << std::endl;
                        }

                    }
        }


        if (nChangePosInOut == -1) reservekey.ReturnKey(); // Return any reserved key if we don't have change

        // Shuffle selected coins and fill in final vin
        txNew.vin.clear();
        std::vector<CInputCoin> selected_coins(setCoins.begin(), setCoins.end());
        std::shuffle(selected_coins.begin(), selected_coins.end(), FastRandomContext());

        // Note how the sequence number is set to non-maxint so that
        // the nLockTime set above actually works.
        //
        // BIP125 defines opt-in RBF as any nSequence < maxint-1, so
        // we use the highest possible value in that range (maxint-2)
        // to avoid conflicting with other possible uses of nSequence,
        // and in the spirit of "smallest possible change from prior
        // behavior."
        const uint32_t nSequence = coin_control.m_signal_bip125_rbf.get_value_or(m_signal_rbf) ? MAX_BIP125_RBF_SEQUENCE : (CTxIn::SEQUENCE_FINAL - 1);
        for (const auto& coin : selected_coins) {
            txNew.vin.push_back(CTxIn(coin.outpoint, CScript(), nSequence));
        }

        if (sign)
        {
            CTransaction txNewConst(txNew);
            int nIn = 0;
            for (const auto& coin : selected_coins)
            {
                const CScript& scriptPubKey = coin.txout.scriptPubKey;
                SignatureData sigdata;

                CKey otpk;

                if (provideprivatekey) {
                    DoesTxOutBelongtoPrivKeyCalcOneTimePrivate(coin.txout, privatekey, otpk, coin.saltisactive);
                }

                if (!ProduceSignature(*this, TransactionSignatureCreator(&txNewConst, nIn, coin.txout.nValue, SIGHASH_ALL, otpk, provideprivatekey), scriptPubKey, sigdata))
                {
                    strFailReason = _("Signing transaction failed");
                    return false;
                } else {
                    UpdateTransaction(txNew, nIn, sigdata);
                }

                nIn++;
            }
        }

        CMutableTransaction mtx;
        DecodeHexTx(mtx, EncodeHexTx(txNew));

        // Return the constructed transaction data.
        tx = MakeTransactionRef(std::move(txNew));

        const uint256& hashTx = tx->GetHash();

        CTransactionRef txtest(MakeTransactionRef(std::move(mtx)));
        const uint256& hashTxtest = txtest->GetHash();
        
        if (hashTxtest != hashTx)
        {
            strFailReason = _("Invalid transaction created. This is a bug. Please report this and send the file debug.log as zip file to the developer.");
            return false;
        }

        // Limit size
        if (GetTransactionWeight(*tx) >= MAX_STANDARD_TX_WEIGHT)
        {
            strFailReason = _("Transaction too large");
            return false;
        }
    }

    if (gArgs.GetBoolArg("-walletrejectlongchains", DEFAULT_WALLET_REJECT_LONG_CHAINS)) {
        // Lastly, ensure this tx will pass the mempool's chain limits
        LockPoints lp;
        CTxMemPoolEntry entry(tx, 0, 0, 0, false, 0, lp);
        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = gArgs.GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT)*1000;
        size_t nLimitDescendants = gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = gArgs.GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT)*1000;
        std::string errString;
        if (!mempool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize, nLimitDescendants, nLimitDescendantSize, errString)) {
            strFailReason = _("Transaction has too long of a mempool chain");
            return false;
        }
    }

    LogPrintf("Fee Calculation: Fee:%d Bytes:%u Needed:%d Tgt:%d (requested %d) Reason:\"%s\" Decay %.5f: Estimation: (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out) Fail: (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out)\n",
              nFeeRet, nBytes, nFeeNeeded, feeCalc.returnedTarget, feeCalc.desiredTarget, StringForFeeReason(feeCalc.reason), feeCalc.est.decay,
              feeCalc.est.pass.start, feeCalc.est.pass.end,
              100 * feeCalc.est.pass.withinTarget / (feeCalc.est.pass.totalConfirmed + feeCalc.est.pass.inMempool + feeCalc.est.pass.leftMempool),
              feeCalc.est.pass.withinTarget, feeCalc.est.pass.totalConfirmed, feeCalc.est.pass.inMempool, feeCalc.est.pass.leftMempool,
              feeCalc.est.fail.start, feeCalc.est.fail.end,
              100 * feeCalc.est.fail.withinTarget / (feeCalc.est.fail.totalConfirmed + feeCalc.est.fail.inMempool + feeCalc.est.fail.leftMempool),
              feeCalc.est.fail.withinTarget, feeCalc.est.fail.totalConfirmed, feeCalc.est.fail.inMempool, feeCalc.est.fail.leftMempool);
    return true;
}

/**
 * Call after CreateTransaction unless you want to abort
 */
bool CWallet::CommitNicknameTransaction(CTransactionRef tx, std::vector<std::pair<std::string, std::string>> orderForm, std::string fromAccount, CConnman* connman, CValidationState& state)
{
    {
        LOCK2(cs_main, cs_wallet);

        CWalletTx wtxNew(this, std::move(tx));
//        wtxNew.mapValue = std::move(mapValue);
        wtxNew.vOrderForm = std::move(orderForm);
        wtxNew.strFromAccount = std::move(fromAccount);
        wtxNew.fTimeReceivedIsTxTime = true;
        wtxNew.fFromMe = true;

        LogPrintf("CommitTransaction:\n%s", wtxNew.tx->ToString()); /* Continued */
        {

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);
        }
        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;
        // Get the inserted-CWalletTx from mapWallet so that the
        // fInMempool flag is cached properly
        CWalletTx& wtx = mapWallet.at(wtxNew.GetHash());
        if (fBroadcastTransactions)
        {
            // Broadcast
            if (!wtx.AcceptToMemoryPool(maxTxFee, state)) {
                LogPrintf("CommitTransaction(): Transaction cannot be broadcast immediately, %s\n", FormatStateMessage(state));
                // TODO: if we expect the failure to be long term or permanent, instead delete wtx from the wallet and return failure.
            } else {

                wtx.RelayWalletTransaction(connman);
            }
        }
    }
    return true;
}

bool CWallet::CreateTransactionToMe(uint256& txid,int outnr, CKey key, CAmount nValue, const CScript& scriptPubKey, std::string refline, CTransactionRef& tx, std::string& strFailReason, const CCoinControl& coin_control, CTxOut output, bool onlyfromoneaddress, CTxDestination fromaddress, unsigned char tocurrency)
{
    CAmount nFeeRet = 0;

    CAmount price = COIN;
    CAmount secondprice = COIN;
    CAmount thirdprice = COIN;
    price = GetCachedPriceInformation(30 * 60 * 1000, secondprice, thirdprice);


    CMutableTransaction txNew;
    txNew.nLockTime = chainActive.Height();

    if (GetRandInt(10) == 0)
        txNew.nLockTime = std::max(0, (int)txNew.nLockTime - GetRandInt(100));

    assert(txNew.nLockTime <= (unsigned int)chainActive.Height());
    assert(txNew.nLockTime < LOCKTIME_THRESHOLD);
    FeeCalculation feeCalc;
    CAmount nFeeNeeded, nFeeNeededBitCash;
    int nBytes;
    {
        LOCK2(cs_main, cs_wallet);
        {
            // Create change script that will be used if we need change
            // TODO: pass in scriptChange instead of reservekey so
            // change transaction isn't always pay-to-bitcash-address
            CScript scriptChange;
            CPubKey pubkeyforchange;

            bool isnonprivate = false;
            bool hasviewkey = false;
            CPubKey viewpubkey;
            if (onlyfromoneaddress) {
                pubkeyforchange = GetSecondPubKeyForDestination(fromaddress);
                isnonprivate = GetNonPrivateForDestination(fromaddress);
                hasviewkey = GetHasViewKeyForDestination(fromaddress);
                viewpubkey = GetViewPubKeyForDestination(fromaddress);
            } else {
                pubkeyforchange = GetCurrentAddressPubKey();
                CTxDestination dest;
    
                if (GetLabelDestination(dest, "")) {
                    isnonprivate = GetNonPrivateForDestination(dest);
                    hasviewkey = GetHasViewKeyForDestination(dest);
                    viewpubkey = GetViewPubKeyForDestination(dest);
                }
            }
            scriptChange = GetScriptForRawPubKey(pubkeyforchange);

            CTxOut change_prototype_txout(0, scriptChange, 0);
            
            // BnB selector is the only selector used when this is true.
            // That should only happen on the first pass through the loop.            
            // Start with no fee and loop until there is enough fee
            while (true)
            {
                txNew.vin.clear();
                txNew.vout.clear();

                CTxOut txout(nValue, scriptChange, tocurrency);
                if (!FillTxOutForTransaction(txout, pubkeyforchange, refline, tocurrency, isnonprivate, hasviewkey, viewpubkey, txNew.nVersion >= 6, txNew.nVersion >= 7, false, CKey())){
                    strFailReason = _("Can not get private key");
                    return false;
                }

                txout.nValueBitCash -= nFeeRet;// Subtract fee equally from each selected recipient
                txout.nValue = txout.nValueBitCash;


                if (IsDust(txout, ::dustRelayFee))
                {
                    if (nFeeRet > 0)
                    {
                        if (txout.nValue < 0)
                            strFailReason = _("The transaction amount is too small to pay the fee");
                        else
                            strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                    }
                    else
                        strFailReason = _("Transaction amount too small");
                    return false;
                }
                txNew.vout.push_back(txout);
                
               
                // Dummy fill vin for maximum size estimation
                //
                
                txNew.vin.push_back(CTxIn(COutPoint(txid,outnr),CScript()));
                
                nBytes = CalculateMaximumSignedTxSizeWithOutput(txNew, this,output);
                if (nBytes < 0) {
                    strFailReason = _("Signing transaction failed");
                    return false;
                }
                nFeeNeeded = GetMinimumFee(*this, nBytes, coin_control, ::mempool, ::feeEstimator, &feeCalc);
                nFeeNeededBitCash = nFeeNeeded;
                //Calculate fee in from currency
//std::cout << "fee old: " <<FormatMoney(nFeeNeeded) << std::endl;	
                if (tocurrency == 1) {
                    nFeeNeeded = (__int128_t)nFeeNeeded * (__int128_t)price / (__int128_t)COIN;//Fee in BitCash to Fee in Dollar
//std::cout << "fee new: " <<FormatMoney(nFeeNeeded) << std::endl;

//std::cout << "Relay fee: " <<minRelayTxFee.GetFee(nBytes)<< std::endl;
                } else
                if (tocurrency == 2) {
                    nFeeNeeded = ((__int128_t)nFeeNeeded * (__int128_t)price / (__int128_t)COIN) * (__int128_t)COIN / (__int128_t)thirdprice;//Fee in BitCash to Fee in Gold
                    if (nFeeNeeded < 1) nFeeNeeded = 1;
                }
                if (feeCalc.reason == FeeReason::FALLBACK && !m_allow_fallback_fee) {
                    // eventually allow a fallback fee
                    strFailReason = _("Fee estimation failed. Fallbackfee is disabled. Wait a few blocks or enable -fallbackfee.");
                    return false;
                }
                // If we made it here and we aren't even able to meet the relay fee on the next pass, give up
                // because we must be at the maximum allowed fee.
                if (nFeeNeededBitCash < ::minRelayTxFee.GetFee(nBytes))
                {
                    strFailReason = _("Transaction too large for fee policy");
                    return false;
                }
                // Include more fee and try again.
                if (nFeeRet!=nFeeNeeded)
                {
                  nFeeRet = nFeeNeeded;
                  continue;
                }
                break;
            }
        }
        // Shuffle selected coins and fill in final vin
        txNew.vin.clear();

        // Note how the sequence number is set to non-maxint so that
        // the nLockTime set above actually works.
        //
        // BIP125 defines opt-in RBF as any nSequence < maxint-1, so
        // we use the highest possible value in that range (maxint-2)
        // to avoid conflicting with other possible uses of nSequence,
        // and in the spirit of "smallest possible change from prior
        // behavior."
        const uint32_t nSequence = coin_control.m_signal_bip125_rbf.get_value_or(m_signal_rbf) ? MAX_BIP125_RBF_SEQUENCE : (CTxIn::SEQUENCE_FINAL - 1);
        txNew.vin.push_back(CTxIn(COutPoint(txid,outnr),CScript(), nSequence));

        CTransaction txNewConst(txNew);
        int nIn = 0;
        SignatureData sigdata;

        if (!ProduceSignature(*this, TransactionSignatureCreator(&txNewConst, nIn, nValue, SIGHASH_ALL, key, true), scriptPubKey, sigdata))
        {
            strFailReason = _("Signing transaction failed");
            return false;
        } else {
            UpdateTransaction(txNew, nIn, sigdata);
        }
        // Return the constructed transaction data.
        tx = MakeTransactionRef(std::move(txNew));

        // Limit size
        if (GetTransactionWeight(*tx) >= MAX_STANDARD_TX_WEIGHT)
        {
            strFailReason = _("Transaction too large");
            return false;
        }
    }
    if (gArgs.GetBoolArg("-walletrejectlongchains", DEFAULT_WALLET_REJECT_LONG_CHAINS)) {
        // Lastly, ensure this tx will pass the mempool's chain limits
        LockPoints lp;
        CTxMemPoolEntry entry(tx, 0, 0, 0, false, 0, lp);
        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = gArgs.GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT)*1000;
        size_t nLimitDescendants = gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = gArgs.GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT)*1000;
        std::string errString;
        if (!mempool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize, nLimitDescendants, nLimitDescendantSize, errString)) {
            strFailReason = _("Transaction has too long of a mempool chain");
            return false;
        }
    }
    LogPrintf("Fee Calculation: Fee:%d Bytes:%u Needed:%d Tgt:%d (requested %d) Reason:\"%s\" Decay %.5f: Estimation: (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out) Fail: (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out)\n",
              nFeeRet, nBytes, nFeeNeeded, feeCalc.returnedTarget, feeCalc.desiredTarget, StringForFeeReason(feeCalc.reason), feeCalc.est.decay,
              feeCalc.est.pass.start, feeCalc.est.pass.end,
              100 * feeCalc.est.pass.withinTarget / (feeCalc.est.pass.totalConfirmed + feeCalc.est.pass.inMempool + feeCalc.est.pass.leftMempool),
              feeCalc.est.pass.withinTarget, feeCalc.est.pass.totalConfirmed, feeCalc.est.pass.inMempool, feeCalc.est.pass.leftMempool,
              feeCalc.est.fail.start, feeCalc.est.fail.end,
              100 * feeCalc.est.fail.withinTarget / (feeCalc.est.fail.totalConfirmed + feeCalc.est.fail.inMempool + feeCalc.est.fail.leftMempool),
              feeCalc.est.fail.withinTarget, feeCalc.est.fail.totalConfirmed, feeCalc.est.fail.inMempool, feeCalc.est.fail.leftMempool);
    return true;
}

/**
 * Call after CreateTransaction unless you want to abort
 */
bool CWallet::CommitTransaction(CTransactionRef tx, mapValue_t mapValue, std::vector<std::pair<std::string, std::string>> orderForm, std::string fromAccount, CReserveKey& reservekey, CConnman* connman, CValidationState& state, bool keepkey, bool frombitcashexpresslink)
{
    {
        LOCK2(cs_main, cs_wallet);
        CWalletTx wtxNew(this, std::move(tx));
        wtxNew.mapValue = std::move(mapValue);
        wtxNew.vOrderForm = std::move(orderForm);
        wtxNew.strFromAccount = std::move(fromAccount);
        wtxNew.fTimeReceivedIsTxTime = true;
        wtxNew.fFromMe = true;

        LogPrintf("CommitTransaction:\n%s", wtxNew.tx->ToString()); /* Continued */
        {
            // Take key pair from key pool so it won't be used again
            if (keepkey) reservekey.KeepKey();
            //Add private keys for stealth address
            const CTransaction& txadd = *wtxNew.tx;
            IsMineForScanningBlockchain(txadd);

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);
            if (!frombitcashexpresslink) {
                // Notify that old coins are spent
                for (const CTxIn& txin : wtxNew.tx->vin)
                {
                    CWalletTx &coin = mapWallet.at(txin.prevout.hash);
                    coin.BindWallet(this);
                    NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
                }
            }
        }
        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;
        // Get the inserted-CWalletTx from mapWallet so that the
        // fInMempool flag is cached properly
        CWalletTx& wtx = mapWallet.at(wtxNew.GetHash());
        if (fBroadcastTransactions)
        {
            // Broadcast
            if (!wtx.AcceptToMemoryPool(maxTxFee, state)) {
                LogPrintf("CommitTransaction(): Transaction cannot be broadcast immediately, %s\n", FormatStateMessage(state));
                // TODO: if we expect the failure to be long term or permanent, instead delete wtx from the wallet and return failure.
            } else {
                wtx.RelayWalletTransaction(connman);
            }
        }
    }
    return true;
}

bool CWallet::SendAsLink(CAmount nAmount, std::string referenceline, std::string& strlink, std::string& strerr, unsigned char tocurrency, unsigned char fromcurrency)
{
    LOCK2(cs_main, cs_wallet);

    CKey secret;
    secret.MakeNewKey(true);
    CPubKey pubkey = secret.GetPubKey();

    CTxDestination dest = pubkey.GetID();
    SetSecondPubKeyForDestination(dest,pubkey);

    strlink="https://wallet.choosebitcash.com/?coins="+EncodeSecret(secret);

    CCoinControl coin_control;
    bool fSubtractFeeFromAmount=false;

/*    CTransactionRef tx = SendMoney(pwallet, dest, nAmount, fSubtractFeeFromAmount, coin_control, std::move(mapValue), {} , referenceline);

static CTransactionRef SendMoney(CWallet * const pwallet, const CTxDestination &address, CAmount nValue, bool fSubtractFeeFromAmount, const CCoinControl& coin_control, mapValue_t mapValue, std::string fromAccount, std::string referenceline)
{*/
    CAmount curBalance = GetBalance(fromcurrency);

    // Check amount
    if (nAmount <= 0) 
    {
        strerr = "Invalid amount";
        return false;
    }

    if (nAmount > curBalance) 
    {
        strerr = "Insufficient funds";
        return false;
    }

    // Parse Bitcash address
    CScript scriptPubKey = GetScriptForDestination(dest);

    // Create and send the transaction
    CReserveKey reservekey(this);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {scriptPubKey, nAmount, fSubtractFeeFromAmount, referenceline, tocurrency, GetNonPrivateForDestination(dest), GetSecondPubKeyForDestination(dest), false, false, CPubKey()};
    vecSend.push_back(recipient);
    CTransactionRef tx;

    if (!CreateTransaction(vecSend, tx, reservekey, nFeeRequired, nChangePosRet, strerr, coin_control, true, false,CNoDestination(), false,  CKey(), fromcurrency, true, secret)) {
        return false;
    }
    CValidationState state;
    mapValue_t mapValue;
    std::string fromAccount="";
    if (!CommitTransaction(tx, std::move(mapValue), {} /* orderForm */, std::move(fromAccount), reservekey, g_connman.get(), state)) {
        return false;
    }
    strlink+="&txid="+tx->GetHash().GetHex();
    return true;
}

bool CWallet::ClaimFromLink(std::string& strlink, std::string& strerr)
{
    LOCK2(cs_main, cs_wallet);

    if (strlink.size()<=40) 
    {
        strerr="The link is not valid.";
        return false;    
    }
    int coinpos=strlink.find("coins=");
    int txidpos=strlink.find("&txid=");
    std::string keystr=strlink.substr(coinpos+6,txidpos-(coinpos+6));
    std::string txstr=strlink.substr(txidpos+6);

    CKey key = DecodeSecret(keystr);
    if (!key.IsValid()) { strerr="Invalid private key encoding"; return false;}
    uint256 hash(uint256S(txstr));
     
    CBlockIndex* blockindex = nullptr;
    bool f_txindex_ready = false;
    if (g_txindex && !blockindex) {
        f_txindex_ready = g_txindex->BlockUntilSyncedToCurrentChain();
    }

    CTransactionRef tx;
    uint256 hash_block;
    if (!GetTransaction(hash, tx, Params().GetConsensus(), hash_block, true, blockindex)) {
        std::string errmsg;
        if (blockindex) {
            if (!(blockindex->nStatus & BLOCK_HAVE_DATA)) {
               strerr="Block not available";
               return false;
            }
            errmsg = "No such transaction found in the provided block";
        } else if (!g_txindex) {
            errmsg = "No such mempool transaction. Use -txindex to enable blockchain transaction queries";
        } else if (!f_txindex_ready) {
            errmsg = "No such mempool transaction. Blockchain transactions are still in the process of being indexed";
        } else {
            errmsg = "No such mempool or blockchain transaction. The coins may have been already claimed.";
        }
        strerr=errmsg;
        return false;
    }
    if (hash_block.IsNull())
    {
        strerr="This transaction has not been mined into a block yet.";
        return false;
    }

    const CTransaction& ctx = *tx;
    CCoinControl coin_control;
    for (unsigned int i = 0; i < ctx.vout.size(); i++)
    {
        CKey otpkey;
        if (DoesTxOutBelongtoPrivKeyCalcOneTimePrivate(ctx.vout[i], key, otpkey, ctx.nVersion >= 7))
        {
            COutPoint out;
            out.hash=hash;
            out.n=i;
            CCoinsViewCache view(pcoinsTip.get());
            const Coin& coin = view.AccessCoin(out);

            if (coin.IsSpent()) {
                strerr = strprintf("Error: These coins have already been claimed.");
                return false;
            }

            std::string refline = DecryptRefLineTxOutWithOnePrivateKey(ctx.vout[i], key, ctx.nVersion >= 7);

            // Create and send the transaction
            CTransactionRef tx;

            if (!CreateTransactionToMe(hash,i, otpkey, ctx.vout[i].nValue, ctx.vout[i].scriptPubKey, refline, tx, strerr,coin_control, ctx.vout[i], false, CNoDestination(),ctx.vout[i].currency)) {
                return false;
            }
            CValidationState state;
            mapValue_t mapValue;
            CReserveKey reservekey(this);
            if (!CommitTransaction(tx, std::move(mapValue), {} /* orderForm */, {}, reservekey, g_connman.get(), state,false,true)) {
              strerr = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
              return false;
            }
            return true;
        }
    }
    strerr="No transaction output found.";
    return false;
}

void CWallet::ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries) {
    WalletBatch batch(*database);
    return batch.ListAccountCreditDebit(strAccount, entries);
}

bool CWallet::AddAccountingEntry(const CAccountingEntry& acentry)
{
    WalletBatch batch(*database);

    return AddAccountingEntry(acentry, &batch);
}

bool CWallet::AddAccountingEntry(const CAccountingEntry& acentry, WalletBatch *batch)
{
    if (!batch->WriteAccountingEntry(++nAccountingEntryNumber, acentry)) {
        return false;
    }

    laccentries.push_back(acentry);
    CAccountingEntry & entry = laccentries.back();
    wtxOrdered.insert(std::make_pair(entry.nOrderPos, TxPair(nullptr, &entry)));

    return true;
}

DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    LOCK2(cs_main, cs_wallet);

    fFirstRunRet = false;
    DBErrors nLoadWalletRet = WalletBatch(*database,"cr+").LoadWallet(this);
    if (nLoadWalletRet == DBErrors::NEED_REWRITE)
    {
        if (database->Rewrite("\x04pool"))
        {
            setInternalKeyPool.clear();
            setExternalKeyPool.clear();
            m_pool_key_to_index.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    // This wallet is in its first run if all of these are empty
    fFirstRunRet = mapKeys.empty() && mapCryptedKeys.empty() && mapWatchKeys.empty() && setWatchOnly.empty() && mapScripts.empty();

    if (nLoadWalletRet != DBErrors::LOAD_OK)
        return nLoadWalletRet;

    uiInterface.LoadWallet(this);

    return DBErrors::LOAD_OK;
}

DBErrors CWallet::ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut)
{
    AssertLockHeld(cs_wallet); // mapWallet
    DBErrors nZapSelectTxRet = WalletBatch(*database,"cr+").ZapSelectTx(vHashIn, vHashOut);
    for (uint256 hash : vHashOut)
        mapWallet.erase(hash);

    if (nZapSelectTxRet == DBErrors::NEED_REWRITE)
    {
        if (database->Rewrite("\x04pool"))
        {
            setInternalKeyPool.clear();
            setExternalKeyPool.clear();
            m_pool_key_to_index.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nZapSelectTxRet != DBErrors::LOAD_OK)
        return nZapSelectTxRet;

    MarkDirty();

    return DBErrors::LOAD_OK;

}

DBErrors CWallet::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    DBErrors nZapWalletTxRet = WalletBatch(*database,"cr+").ZapWalletTx(vWtx);
    if (nZapWalletTxRet == DBErrors::NEED_REWRITE)
    {
        if (database->Rewrite("\x04pool"))
        {
            LOCK(cs_wallet);
            setInternalKeyPool.clear();
            setExternalKeyPool.clear();
            m_pool_key_to_index.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nZapWalletTxRet != DBErrors::LOAD_OK)
        return nZapWalletTxRet;

    return DBErrors::LOAD_OK;
}


bool CWallet::SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& strPurpose)
{
    accountforscript.clear();
    bool fUpdated = false;
    {
        LOCK(cs_wallet); // mapAddressBook
        std::map<CTxDestination, CAddressBookData>::iterator mi = mapAddressBook.find(address);
        fUpdated = mi != mapAddressBook.end();
        mapAddressBook[address].name = strName;
        if (!strPurpose.empty()) /* update purpose only if requested */
            mapAddressBook[address].purpose = strPurpose;
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address) != ISMINE_NO,
                             strPurpose, (fUpdated ? CT_UPDATED : CT_NEW) );
    if (!strPurpose.empty() && !WalletBatch(*database).WritePurpose(EncodeDestinationHasSecondKey(address), strPurpose))
        return false;
    return WalletBatch(*database).WriteName(EncodeDestinationHasSecondKey(address), strName);
}

bool CWallet::RegisterNickname(std::string nick, std::string addr, std::string& strFailReason)
{
    CPubKey pkey = GetCurrentAddressPubKey();

    std::string address = EncodeDestination(pkey.GetID(),pkey);

    std::string fromAccount = "";
    std::string strError;
    CTransactionRef tx;
    if (!CreateNicknameTransaction(nick, addr, tx, strError)) {
        strFailReason=strError;
        return false;
    }
    CValidationState state;
    if (!CommitNicknameTransaction(tx, {} , std::move(fromAccount), g_connman.get(), state)) {
        strFailReason="Could not commit transaction";
        return false;
    }
    return true;
}

bool CWallet::DelAddressBook(const CTxDestination& address)
{
    {
        LOCK(cs_wallet); // mapAddressBook

        // Delete destdata tuples associated with address
        std::string strAddress = EncodeDestinationHasSecondKey(address);
        for (const std::pair<std::string, std::string> &item : mapAddressBook[address].destdata)
        {
            WalletBatch(*database).EraseDestData(strAddress, item.first);
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address) != ISMINE_NO, "", CT_DELETED);

    WalletBatch(*database).ErasePurpose(EncodeDestinationHasSecondKey(address));
    return WalletBatch(*database).EraseName(EncodeDestinationHasSecondKey(address));
}

const std::string& CWallet::GetLabelName(const CPubKey& pubkey) const
{
    CTxDestination address=GetDestinationForKey(pubkey, OutputType::LEGACY);
    auto mi = mapAddressBook.find(address);
    if (mi != mapAddressBook.end()) {
      return mi->second.name;
    }
    address=GetDestinationForKey(pubkey, OutputType::NONPRIVATE);
    mi = mapAddressBook.find(address);
    if (mi != mapAddressBook.end()) {
      return mi->second.name;
    }
    // A scriptPubKey that doesn't have an entry in the address book is
    // associated with the default label ("").
    const static std::string DEFAULT_LABEL_NAME;
    return DEFAULT_LABEL_NAME;
}

/**
 * Mark old keypool keys as used,
 * and generate all new keys
 */
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        WalletBatch batch(*database);

        for (int64_t nIndex : setInternalKeyPool) {
            batch.ErasePool(nIndex);
        }
        setInternalKeyPool.clear();

        for (int64_t nIndex : setExternalKeyPool) {
            batch.ErasePool(nIndex);
        }
        setExternalKeyPool.clear();

        m_pool_key_to_index.clear();

        if (!TopUpKeyPool()) {
            return false;
        }
        LogPrintf("CWallet::NewKeyPool rewrote keypool\n");
    }
    return true;
}

size_t CWallet::KeypoolCountExternalKeys()
{
    AssertLockHeld(cs_wallet); // setExternalKeyPool
    return setExternalKeyPool.size();
}

void CWallet::LoadKeyPool(int64_t nIndex, const CKeyPool &keypool)
{
    AssertLockHeld(cs_wallet);
    if (keypool.fInternal) {
        setInternalKeyPool.insert(nIndex);
    } else {
        setExternalKeyPool.insert(nIndex);
    }
    m_max_keypool_index = std::max(m_max_keypool_index, nIndex);
    m_pool_key_to_index[keypool.vchPubKey.GetID()] = nIndex;

    // If no metadata exists yet, create a default with the pool key's
    // creation time. Note that this may be overwritten by actually
    // stored metadata for that key later, which is fine.
    CKeyID keyid = keypool.vchPubKey.GetID();
    if (mapKeyMetadata.count(keyid) == 0)
        mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
}

bool CWallet::TopUpKeyPool(unsigned int kpSize)
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        // Top up key pool
        unsigned int nTargetSize;
        if (kpSize > 0)
            nTargetSize = kpSize;
        else
            nTargetSize = std::max(gArgs.GetArg("-keypool", DEFAULT_KEYPOOL_SIZE), (int64_t) 0);

        // count amount of available keys (internal, external)
        // make sure the keypool of external and internal keys fits the user selected target (-keypool)
        int64_t missingExternal = std::max(std::max((int64_t) nTargetSize, (int64_t) 1) - (int64_t)setExternalKeyPool.size(), (int64_t) 0);
        int64_t missingInternal = std::max(std::max((int64_t) nTargetSize, (int64_t) 1) - (int64_t)setInternalKeyPool.size(), (int64_t) 0);

        if (!IsHDEnabled() || !CanSupportFeature(FEATURE_HD_SPLIT))
        {
            // don't create extra internal keys
            missingInternal = 0;
        }
        bool internal = false;
        WalletBatch batch(*database);
        for (int64_t i = missingInternal + missingExternal; i--;)
        {
            if (i < missingInternal) {
                internal = true;
            }

            assert(m_max_keypool_index < std::numeric_limits<int64_t>::max()); // How in the hell did you use so many keys?
            int64_t index = ++m_max_keypool_index;

            CPubKey pubkey(GenerateNewKey(batch, internal));
            if (!batch.WritePool(index, CKeyPool(pubkey, internal))) {
                throw std::runtime_error(std::string(__func__) + ": writing generated key failed");
            }

            if (internal) {
                setInternalKeyPool.insert(index);
            } else {
                setExternalKeyPool.insert(index);
            }
            m_pool_key_to_index[pubkey.GetID()] = index;
        }
        if (missingInternal + missingExternal > 0) {
//            LogPrintf("keypool added %d keys (%d internal), size=%u (%u internal)\n", missingInternal + missingExternal, missingInternal, setInternalKeyPool.size() + setExternalKeyPool.size(), setInternalKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool fRequestedInternal)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        bool fReturningInternal = IsHDEnabled() && CanSupportFeature(FEATURE_HD_SPLIT) && fRequestedInternal;
        std::set<int64_t>& setKeyPool = fReturningInternal ? setInternalKeyPool : setExternalKeyPool;

        // Get the oldest key
        if(setKeyPool.empty())
            return;

        WalletBatch batch(*database);

        auto it = setKeyPool.begin();
        nIndex = *it;
        setKeyPool.erase(it);
        if (!batch.ReadPool(nIndex, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": read failed");
        }
        if (!HaveKey(keypool.vchPubKey.GetID())) {
            throw std::runtime_error(std::string(__func__) + ": unknown key in key pool");
        }
        if (keypool.fInternal != fReturningInternal) {
            throw std::runtime_error(std::string(__func__) + ": keypool entry misclassified");
        }

        assert(keypool.vchPubKey.IsValid());
        m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
  //      LogPrintf("keypool reserve %d\n", nIndex);
    }
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    WalletBatch batch(*database);
    batch.ErasePool(nIndex);
//    LogPrintf("keypool keep %d\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex, bool fInternal, const CPubKey& pubkey)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        if (fInternal) {
            setInternalKeyPool.insert(nIndex);
        } else {
            setExternalKeyPool.insert(nIndex);
        }
        m_pool_key_to_index[pubkey.GetID()] = nIndex;
    }
    LogPrintf("keypool return %d\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, bool internal)
{
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        int64_t nIndex = 0;
        ReserveKeyFromKeyPool(nIndex, keypool, internal);
        if (nIndex == -1)
        {
            if (IsLocked()) return false;
            WalletBatch batch(*database);
            result = GenerateNewKey(batch, internal);
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

static int64_t GetOldestKeyTimeInPool(const std::set<int64_t>& setKeyPool, WalletBatch& batch) {
    if (setKeyPool.empty()) {
        return GetTime();
    }

    CKeyPool keypool;
    int64_t nIndex = *(setKeyPool.begin());
    if (!batch.ReadPool(nIndex, keypool)) {
        throw std::runtime_error(std::string(__func__) + ": read oldest key in keypool failed");
    }
    assert(keypool.vchPubKey.IsValid());
    return keypool.nTime;
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    LOCK(cs_wallet);

    WalletBatch batch(*database);

    // load oldest key from keypool, get time and return
    int64_t oldestKey = GetOldestKeyTimeInPool(setExternalKeyPool, batch);
    if (IsHDEnabled() && CanSupportFeature(FEATURE_HD_SPLIT)) {
        oldestKey = std::max(GetOldestKeyTimeInPool(setInternalKeyPool, batch), oldestKey);
    }

    return oldestKey;
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances()
{
    std::map<CTxDestination, CAmount> balances;

    {
        LOCK(cs_wallet);
        for (const auto& walletEntry : mapWallet)
        {
            const CWalletTx *pcoin = &walletEntry.second;

            if (!pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->tx->vout[i], 64, pcoin->tx->nVersion >= 7))
                    continue;
                if(!ExtractDestination(pcoin->tx->vout[i].scriptPubKey, addr))
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : pcoin->tx->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

std::set< std::set<CTxDestination> > CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    std::set< std::set<CTxDestination> > groupings;
    std::set<CTxDestination> grouping;

    for (const auto& walletEntry : mapWallet)
    {
        const CWalletTx *pcoin = &walletEntry.second;

        if (pcoin->tx->vin.size() > 0)
        {
            bool any_mine = false;
            // group all input addresses with each other
            for (CTxIn txin : pcoin->tx->vin)
            {
                CTxDestination address;
                if(!IsMine(txin)) /* If this input isn't mine, ignore it */
                    continue;
                if(!ExtractDestination(mapWallet.at(txin.prevout.hash).tx->vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine)
            {
               for (CTxOut txout : pcoin->tx->vout)
                   if (IsChange(txout))
                   {
                       CTxDestination txoutAddr;
                       if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                           continue;
                       grouping.insert(txoutAddr);
                   }
            }
            if (grouping.size() > 0)
            {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        // group lone addrs by themselves
        for (const auto& txout : pcoin->tx->vout)
            if (IsMine(txout, 71, pcoin->tx->nVersion >= 7))
            {
                CTxDestination address;
                if(!ExtractDestination(txout.scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    std::set< std::set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    std::map< CTxDestination, std::set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
    for (std::set<CTxDestination> _grouping : groupings)
    {
        // make a set of all the groups hit by this new group
        std::set< std::set<CTxDestination>* > hits;
        std::map< CTxDestination, std::set<CTxDestination>* >::iterator it;
        for (CTxDestination address : _grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        std::set<CTxDestination>* merged = new std::set<CTxDestination>(_grouping);
        for (std::set<CTxDestination>* hit : hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        for (CTxDestination element : *merged)
            setmap[element] = merged;
    }

    std::set< std::set<CTxDestination> > ret;
    for (std::set<CTxDestination>* uniqueGrouping : uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

std::set<CTxDestination> CWallet::GetLabelAddresses(const std::string& label) const
{
    LOCK(cs_wallet);
    std::set<CTxDestination> result;
    for (const std::pair<CTxDestination, CAddressBookData>& item : mapAddressBook)
    {
        const CTxDestination& address = item.first;
        const std::string& strName = item.second.name;
        if (strName == label)
            result.insert(address);
    }
    return result;
}

void CWallet::DeleteLabel(const std::string& label)
{
    WalletBatch batch(*database);
    batch.EraseAccount(label);
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey, bool internal)
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool, internal);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            return false;
        }
        fInternal = keypool.fInternal;
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1) {
        pwallet->ReturnKey(nIndex, fInternal, vchPubKey);
    }
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::MarkReserveKeysAsUsed(int64_t keypool_id)
{
    AssertLockHeld(cs_wallet);
    bool internal = setInternalKeyPool.count(keypool_id);
    if (!internal) assert(setExternalKeyPool.count(keypool_id));
    std::set<int64_t> *setKeyPool = internal ? &setInternalKeyPool : &setExternalKeyPool;
    auto it = setKeyPool->begin();

    WalletBatch batch(*database);
    while (it != std::end(*setKeyPool)) {
        const int64_t& index = *(it);
        if (index > keypool_id) break; // set*KeyPool is ordered

        CKeyPool keypool;
        if (batch.ReadPool(index, keypool)) { //TODO: This should be unnecessary
            m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
        }
        LearnAllRelatedScripts(keypool.vchPubKey);
        batch.ErasePool(index);
        LogPrintf("keypool index %d removed\n", index);
        it = setKeyPool->erase(it);
    }
}

void CWallet::GetScriptForMining(std::shared_ptr<CReserveScript> &script)
{
    std::shared_ptr<CReserveKey> rKey = std::make_shared<CReserveKey>(this);
    CPubKey pubkey;
    if (!rKey->GetReservedKey(pubkey))
        return;

    script = rKey;
    script->reserveScript = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
}

void CWallet::LockCoin(const COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.insert(output);
}

void CWallet::UnlockCoin(const COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.erase(output);
}

void CWallet::UnlockAllCoins()
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.clear();
}

bool CWallet::IsLockedCoin(uint256 hash, unsigned int n) const
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    COutPoint outpt(hash, n);

    return (setLockedCoins.count(outpt) > 0);
}

void CWallet::ListLockedCoins(std::vector<COutPoint>& vOutpts) const
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    for (std::set<COutPoint>::iterator it = setLockedCoins.begin();
         it != setLockedCoins.end(); it++) {
        COutPoint outpt = (*it);
        vOutpts.push_back(outpt);
    }
}

/** @} */ // end of Actions

void CWallet::GetKeyBirthTimes(std::map<CTxDestination, int64_t> &mapKeyBirth) const {
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (const auto& entry : mapKeyMetadata) {
        if (entry.second.nCreateTime) {
            mapKeyBirth[entry.first] = entry.second.nCreateTime;
        }
    }

    // map in which we'll infer heights of other keys
    CBlockIndex *pindexMax = chainActive[std::max(0, chainActive.Height() - 144)]; // the tip can be reorganized; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    for (const CKeyID &keyid : GetKeys()) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (const auto& entry : mapWallet) {
        // iterate over all wallet transactions...
        const CWalletTx &wtx = entry.second;
        CBlockIndex* pindex = LookupBlockIndex(wtx.hashBlock);
        if (pindex && chainActive.Contains(pindex)) {
            // ... which are already in a block
            int nHeight = pindex->nHeight;
            for (const CTxOut &txout : wtx.tx->vout) {
                // iterate over all their outputs
                CAffectedKeysVisitor(*this, vAffected).Process(txout.scriptPubKey);
                for (const CKeyID &keyid : vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = pindex;
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (const auto& entry : mapKeyFirstBlock)
        mapKeyBirth[entry.first] = entry.second->GetBlockTime() - TIMESTAMP_WINDOW; // block times can be 2h off
}

/**
 * Compute smart timestamp for a transaction being added to the wallet.
 *
 * Logic:
 * - If sending a transaction, assign its timestamp to the current time.
 * - If receiving a transaction outside a block, assign its timestamp to the
 *   current time.
 * - If receiving a block with a future timestamp, assign all its (not already
 *   known) transactions' timestamps to the current time.
 * - If receiving a block with a past timestamp, before the most recent known
 *   transaction (that we care about), assign all its (not already known)
 *   transactions' timestamps to the same timestamp as that most-recent-known
 *   transaction.
 * - If receiving a block with a past timestamp, but after the most recent known
 *   transaction, assign all its (not already known) transactions' timestamps to
 *   the block time.
 *
 * For more information see CWalletTx::nTimeSmart,
 * https://bitcashtalk.org/?topic=54527, or
 * https://github.com/bitcash/bitcash/pull/1393.
 */
unsigned int CWallet::ComputeTimeSmart(const CWalletTx& wtx) const
{
    unsigned int nTimeSmart = wtx.nTimeReceived;
    if (!wtx.hashUnset()) {
        if (const CBlockIndex* pindex = LookupBlockIndex(wtx.hashBlock)) {
            int64_t latestNow = wtx.nTimeReceived;
            int64_t latestEntry = 0;

            // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
            int64_t latestTolerated = latestNow + 300;
            const TxItems& txOrdered = wtxOrdered;
            for (auto it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
                CWalletTx* const pwtx = it->second.first;
                if (pwtx == &wtx) {
                    continue;
                }
                CAccountingEntry* const pacentry = it->second.second;
                int64_t nSmartTime;
                if (pwtx) {
                    nSmartTime = pwtx->nTimeSmart;
                    if (!nSmartTime) {
                        nSmartTime = pwtx->nTimeReceived;
                    }
                } else {
                    nSmartTime = pacentry->nTime;
                }
                if (nSmartTime <= latestTolerated) {
                    latestEntry = nSmartTime;
                    if (nSmartTime > latestNow) {
                        latestNow = nSmartTime;
                    }
                    break;
                }
            }

            int64_t blocktime = pindex->GetBlockTime();
            nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
        } else {
            LogPrintf("%s: found %s in block %s not in index\n", __func__, wtx.GetHash().ToString(), wtx.hashBlock.ToString());
        }
    }
    return nTimeSmart;
}

bool CWallet::AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    if (boost::get<CNoDestination>(&dest))
        return false;

    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
    return WalletBatch(*database).WriteDestData(EncodeDestinationHasSecondKey(dest), key, value);
}

bool CWallet::EraseDestData(const CTxDestination &dest, const std::string &key)
{
    if (!mapAddressBook[dest].destdata.erase(key))
        return false;
    return WalletBatch(*database).EraseDestData(EncodeDestinationHasSecondKey(dest), key);
}

bool CWallet::LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
    return true;
}

bool CWallet::GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const
{
    std::map<CTxDestination, CAddressBookData>::const_iterator i = mapAddressBook.find(dest);
    if(i != mapAddressBook.end())
    {
        CAddressBookData::StringMap::const_iterator j = i->second.destdata.find(key);
        if(j != i->second.destdata.end())
        {
            if(value)
                *value = j->second;
            return true;
        }
    }
    return false;
}

std::vector<std::string> CWallet::GetDestValues(const std::string& prefix) const
{
    LOCK(cs_wallet);
    std::vector<std::string> values;
    for (const auto& address : mapAddressBook) {
        for (const auto& data : address.second.destdata) {
            if (!data.first.compare(0, prefix.size(), prefix)) {
                values.emplace_back(data.second);
            }
        }
    }
    return values;
}

CWallet* CWallet::CreateWalletFromFile(const std::string& name, const fs::path& path)
{
    const std::string& walletFile = name;

    // needed to restore wallet transaction meta data after -zapwallettxes
    std::vector<CWalletTx> vWtx;

    if (gArgs.GetBoolArg("-zapwallettxes", false)) {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

        std::unique_ptr<CWallet> tempWallet = MakeUnique<CWallet>(name, WalletDatabase::Create(path));
        DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
        if (nZapWalletRet != DBErrors::LOAD_OK) {
            InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return nullptr;
        }
    }

    uiInterface.InitMessage(_("Loading wallet..."));

    int64_t nStart = GetTimeMillis();
    bool fFirstRun = true;
    CWallet *walletInstance = new CWallet(name, WalletDatabase::Create(path));
    DBErrors nLoadWalletRet = walletInstance->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DBErrors::LOAD_OK)
    {
        if (nLoadWalletRet == DBErrors::CORRUPT) {
            InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return nullptr;
        }
        else if (nLoadWalletRet == DBErrors::NONCRITICAL_ERROR)
        {
            InitWarning(strprintf(_("Error reading %s! All keys read correctly, but transaction data"
                                         " or address book entries might be missing or incorrect."),
                walletFile));
        }
        else if (nLoadWalletRet == DBErrors::TOO_NEW) {
            InitError(strprintf(_("Error loading %s: Wallet requires newer version of %s"), walletFile, _(PACKAGE_NAME)));
            return nullptr;
        }
        else if (nLoadWalletRet == DBErrors::NEED_REWRITE)
        {
            InitError(strprintf(_("Wallet needed to be rewritten: restart %s to complete"), _(PACKAGE_NAME)));
            return nullptr;
        }
        else {
            InitError(strprintf(_("Error loading %s"), walletFile));
            return nullptr;
        }
    }

    if (gArgs.GetBoolArg("-upgradewallet", fFirstRun))
    {
        int nMaxVersion = gArgs.GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            walletInstance->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        }
        else
            LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < walletInstance->GetVersion())
        {
            InitError(_("Cannot downgrade wallet"));
            return nullptr;
        }
        walletInstance->SetMaxVersion(nMaxVersion);
    }

    if (fFirstRun)
    {
        // ensure this wallet.dat can only be opened by clients supporting HD with chain split and expects no default key
        if (!gArgs.GetBoolArg("-usehd", true)) {
            InitError(strprintf(_("Error creating %s: You can't create non-HD wallets with this version."), walletFile));
            return nullptr;
        }
        walletInstance->SetMinVersion(FEATURE_NO_DEFAULT_KEY);

        // generate a new master key
        CPubKey masterPubKey = walletInstance->GenerateNewHDMasterKey();
        if (!walletInstance->SetHDMasterKey(masterPubKey))
            throw std::runtime_error(std::string(__func__) + ": Storing master key failed");

        // Top up the keypool
        if (!walletInstance->TopUpKeyPool()) {
            InitError(_("Unable to generate initial keys") += "\n");
            return nullptr;
        }

        walletInstance->ChainStateFlushed(chainActive.GetLocator());
    } else if (gArgs.IsArgSet("-usehd")) {
        bool useHD = gArgs.GetBoolArg("-usehd", true);
        if (walletInstance->IsHDEnabled() && !useHD) {
            InitError(strprintf(_("Error loading %s: You can't disable HD on an already existing HD wallet"), walletFile));
            return nullptr;
        }
        if (!walletInstance->IsHDEnabled() && useHD) {
            InitError(strprintf(_("Error loading %s: You can't enable HD on an already existing non-HD wallet"), walletFile));
            return nullptr;
        }
    }

    if (!gArgs.GetArg("-addresstype", "").empty() && !ParseOutputType(gArgs.GetArg("-addresstype", ""), walletInstance->m_default_address_type)) {
        InitError(strprintf("Unknown address type '%s'", gArgs.GetArg("-addresstype", "")));
        return nullptr;
    }

    if (!gArgs.GetArg("-changetype", "").empty() && !ParseOutputType(gArgs.GetArg("-changetype", ""), walletInstance->m_default_change_type)) {
        InitError(strprintf("Unknown change type '%s'", gArgs.GetArg("-changetype", "")));
        return nullptr;
    }

    if (gArgs.IsArgSet("-mintxfee")) {
        CAmount n = 0;
        if (!ParseMoney(gArgs.GetArg("-mintxfee", ""), n) || 0 == n) {
            InitError(AmountErrMsg("mintxfee", gArgs.GetArg("-mintxfee", "")));
            return nullptr;
        }
        if (n > HIGH_TX_FEE_PER_KB) {
            InitWarning(AmountHighWarn("-mintxfee") + " " +
                        _("This is the minimum transaction fee you pay on every transaction."));
        }
        walletInstance->m_min_fee = CFeeRate(n);
    }

    walletInstance->m_allow_fallback_fee = Params().IsFallbackFeeEnabled();
    if (gArgs.IsArgSet("-fallbackfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-fallbackfee", ""), nFeePerK)) {
            InitError(strprintf(_("Invalid amount for -fallbackfee=<amount>: '%s'"), gArgs.GetArg("-fallbackfee", "")));
            return nullptr;
        }
        if (nFeePerK > HIGH_TX_FEE_PER_KB) {
            InitWarning(AmountHighWarn("-fallbackfee") + " " +
                        _("This is the transaction fee you may pay when fee estimates are not available."));
        }
        walletInstance->m_fallback_fee = CFeeRate(nFeePerK);
        walletInstance->m_allow_fallback_fee = nFeePerK != 0; //disable fallback fee in case value was set to 0, enable if non-null value
    }
    if (gArgs.IsArgSet("-discardfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-discardfee", ""), nFeePerK)) {
            InitError(strprintf(_("Invalid amount for -discardfee=<amount>: '%s'"), gArgs.GetArg("-discardfee", "")));
            return nullptr;
        }
        if (nFeePerK > HIGH_TX_FEE_PER_KB) {
            InitWarning(AmountHighWarn("-discardfee") + " " +
                        _("This is the transaction fee you may discard if change is smaller than dust at this level"));
        }
        walletInstance->m_discard_rate = CFeeRate(nFeePerK);
    }
    if (gArgs.IsArgSet("-paytxfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-paytxfee", ""), nFeePerK)) {
            InitError(AmountErrMsg("paytxfee", gArgs.GetArg("-paytxfee", "")));
            return nullptr;
        }
        if (nFeePerK > HIGH_TX_FEE_PER_KB) {
            InitWarning(AmountHighWarn("-paytxfee") + " " +
                        _("This is the transaction fee you will pay if you send a transaction."));
        }
        walletInstance->m_pay_tx_fee = CFeeRate(nFeePerK, 1000);
        if (walletInstance->m_pay_tx_fee < ::minRelayTxFee) {
            InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                gArgs.GetArg("-paytxfee", ""), ::minRelayTxFee.ToString()));
            return nullptr;
        }
    }
    walletInstance->m_confirm_target = gArgs.GetArg("-txconfirmtarget", DEFAULT_TX_CONFIRM_TARGET);
    walletInstance->m_spend_zero_conf_change = gArgs.GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);
    walletInstance->m_signal_rbf = gArgs.GetBoolArg("-walletrbf", DEFAULT_WALLET_RBF);

    LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

    // Try to top up keypool. No-op if the wallet is locked.
    walletInstance->TopUpKeyPool();

    LOCK(cs_main);

    CBlockIndex *pindexRescan = chainActive.Genesis();
    if (!gArgs.GetBoolArg("-rescan", false))
    {
        WalletBatch batch(*walletInstance->database);
        CBlockLocator locator;
        if (batch.ReadBestBlock(locator))
            pindexRescan = FindForkInGlobalIndex(chainActive, locator);
    }

    walletInstance->m_last_block_processed = chainActive.Tip();
    RegisterValidationInterface(walletInstance);

    if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
    {
        //We can't rescan beyond non-pruned blocks, stop and throw an error
        //this might happen if a user uses an old wallet within a pruned node
        // or if he ran -disablewallet for a longer time, then decided to re-enable
        if (fPruneMode)
        {
            CBlockIndex *block = chainActive.Tip();
            while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA) && block->pprev->nTx > 0 && pindexRescan != block)
                block = block->pprev;

            if (pindexRescan != block) {
                InitError(_("Prune: last wallet synchronisation goes beyond pruned data. You need to -reindex (download the whole blockchain again in case of pruned node)"));
                return nullptr;
            }
        }

        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);

        // No need to read and scan block if block was created before
        // our wallet birthday (as adjusted for block time variability)
        while (pindexRescan && walletInstance->nTimeFirstKey && (pindexRescan->GetBlockTime() < (walletInstance->nTimeFirstKey - TIMESTAMP_WINDOW))) {
            pindexRescan = chainActive.Next(pindexRescan);
        }

        nStart = GetTimeMillis();
        {
            WalletRescanReserver reserver(walletInstance);
            if (!reserver.reserve()) {
                InitError(_("Failed to rescan the wallet during initialization"));
                return nullptr;
            }
            walletInstance->ScanForWalletTransactions(pindexRescan, nullptr, reserver, true);
        }
        LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
        walletInstance->ChainStateFlushed(chainActive.GetLocator());
        walletInstance->database->IncrementUpdateCounter();

        // Restore wallet transaction metadata after -zapwallettxes=1
        if (gArgs.GetBoolArg("-zapwallettxes", false) && gArgs.GetArg("-zapwallettxes", "1") != "2")
        {
            WalletBatch batch(*walletInstance->database);

            for (const CWalletTx& wtxOld : vWtx)
            {
                uint256 hash = wtxOld.GetHash();
                std::map<uint256, CWalletTx>::iterator mi = walletInstance->mapWallet.find(hash);
                if (mi != walletInstance->mapWallet.end())
                {
                    const CWalletTx* copyFrom = &wtxOld;
                    CWalletTx* copyTo = &mi->second;
                    copyTo->mapValue = copyFrom->mapValue;
                    copyTo->vOrderForm = copyFrom->vOrderForm;
                    copyTo->nTimeReceived = copyFrom->nTimeReceived;
                    copyTo->nTimeSmart = copyFrom->nTimeSmart;
                    copyTo->fFromMe = copyFrom->fFromMe;
                    copyTo->strFromAccount = copyFrom->strFromAccount;
                    copyTo->nOrderPos = copyFrom->nOrderPos;
                    batch.WriteTx(*copyTo);
                }
            }
        }
    }
    walletInstance->SetBroadcastTransactions(gArgs.GetBoolArg("-walletbroadcast", DEFAULT_WALLETBROADCAST));

    {
        LOCK(walletInstance->cs_wallet);
        LogPrintf("setKeyPool.size() = %u\n",      walletInstance->GetKeyPoolSize());
        LogPrintf("mapWallet.size() = %u\n",       walletInstance->mapWallet.size());
        LogPrintf("mapAddressBook.size() = %u\n",  walletInstance->mapAddressBook.size());
    }

    return walletInstance;
}

std::atomic<bool> CWallet::fFlushScheduled(false);

void CWallet::postInitProcess(CScheduler& scheduler)
{
    // Add wallet transactions that aren't already in a block to mempool
    // Do this here as mempool requires genesis block to be loaded
    ReacceptWalletTransactions();

    // Run a thread to flush wallet periodically
    if (!CWallet::fFlushScheduled.exchange(true)) {
        scheduler.scheduleEvery(MaybeCompactWalletDB, 500);
    }
}

bool CWallet::BackupWallet(const std::string& strDest)
{
    return database->Backup(strDest);
}

bool CWallet::ImportWallet(const std::string& strDest)
{
    bool fRescan = true;
    {
        WalletRescanReserver reserver(this);
        LOCK2(cs_main, cs_wallet);


        std::string filename = strDest;
        std::string strLabel = "";

        if (fRescan && fPruneMode) return false;
            //throw JSONRPCError(RPC_WALLET_ERROR, "Rescan is disabled in pruned mode");

        if (fRescan && !reserver.reserve()) {
//            throw JSONRPCError(RPC_WALLET_ERROR, "Wallet is currently rescanning. Abort existing rescan or wait.");
             return false;
        }
  
    fs::ifstream optionFile(filename.c_str());
    if (optionFile.good()){

        while (!optionFile.eof())
        {

            std::string strSecret;
            getline(optionFile, strSecret);
            strSecret.erase(strSecret.find_last_not_of(" \n\r\t")+1);
            if (strSecret.compare("")==0) continue;

            CKey key = DecodeSecret(strSecret);     
            if (!key.IsValid()) return false;//throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Invalid private key encoding %s",strSecret));

            CPubKey pubkey = key.GetPubKey();
            assert(key.VerifyPubKey(pubkey));
            CKeyID vchAddress = pubkey.GetID();
            {
                MarkDirty();
                // We don't know which corresponding address will be used; label them all
                for (const auto& dest : GetAllDestinationsForKey(pubkey)) {
                    SetAddressBook(dest, strLabel, "receive");
                }

                // Don't throw error in case a key is already there
                if (HaveKey(vchAddress)) {
                    continue;
                }

                // whenever a key is imported, we need to scan the whole chain
                UpdateTimeFirstKey(1);
                mapKeyMetadata[vchAddress].nCreateTime = 1;

                if (!AddKeyPubKey(key, pubkey)) {
                    //throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");
                    return false;
                }
                LearnAllRelatedScripts(pubkey);
            }
        }

        if (fRescan) {
            int64_t scanned_time = RescanFromTime(TIMESTAMP_MIN, reserver, true /* update */);
            if (IsAbortingRescan()) {
//                throw JSONRPCError(RPC_MISC_ERROR, "Rescan aborted by user.");
                return false;
            }
            if (scanned_time > TIMESTAMP_MIN) {
//                throw JSONRPCError(RPC_WALLET_ERROR, "Rescan was unable to fully rescan the blockchain. Some transactions may be missing.");
                return false;
            }
        }
 
    optionFile.close();
    } else             
 //       throw JSONRPCError(RPC_WALLET_ERROR, "Could not open text file with private keys.");
        return false;
    }
    
    return true;
}

CKeyPool::CKeyPool()
{
    nTime = GetTime();
    fInternal = false;
}

CKeyPool::CKeyPool(const CPubKey& vchPubKeyIn, bool internalIn)
{
    nTime = GetTime();
    vchPubKey = vchPubKeyIn;
    fInternal = internalIn;
}

CWalletKey::CWalletKey(int64_t nExpires)
{
    nTimeCreated = (nExpires ? GetTime() : 0);
    nTimeExpires = nExpires;
}

void CMerkleTx::SetMerkleBranch(const CBlockIndex* pindex, int posInBlock)
{
    // Update the tx's hashBlock
    hashBlock = pindex->GetBlockHash();

    // set the position of the transaction in the block
    nIndex = posInBlock;
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex* &pindexRet) const
{
    if (hashUnset())
        return 0;

    AssertLockHeld(cs_main);

    // Find the block it claims to be in
    CBlockIndex* pindex = LookupBlockIndex(hashBlock);
    if (!pindex || !chainActive.Contains(pindex))
        return 0;

    pindexRet = pindex;
    return ((nIndex == -1) ? (-1) : 1) * (chainActive.Height() - pindex->nHeight + 1);
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    return std::max(0, (COINBASE_MATURITY+1) - GetDepthInMainChain());
}


bool CWalletTx::AcceptToMemoryPool(const CAmount& nAbsurdFee, CValidationState& state)
{
    // We must set fInMempool here - while it will be re-set to true by the
    // entered-mempool callback, if we did not there would be a race where a
    // user could call sendmoney in a loop and hit spurious out of funds errors
    // because we think that this newly generated transaction's change is
    // unavailable as we're not yet aware that it is in the mempool.
    bool ret = ::AcceptToMemoryPool(mempool, state, tx, nullptr /* pfMissingInputs */,
                                nullptr /* plTxnReplaced */, false /* bypass_limits */, nAbsurdFee);
    fInMempool |= ret;
    return ret;
}

void CWalletTx::RemoveFromMempool()
{
    const CTransaction& ttx = *tx;
    mempool.removeUnchecked(ttx);
}

static const std::string OUTPUT_TYPE_STRING_LEGACY = "legacy";
static const std::string OUTPUT_TYPE_STRING_P2SH_SEGWIT = "p2sh-segwit";
static const std::string OUTPUT_TYPE_STRING_BECH32 = "bech32";
static const std::string OUTPUT_TYPE_STRING_NONPRIVATE = "nonprivate";
static const std::string OUTPUT_TYPE_STRING_WITHVIEWKEY = "withviewkey";

bool ParseOutputType(const std::string& type, OutputType& output_type)
{
    if (type == OUTPUT_TYPE_STRING_LEGACY) {
        output_type = OutputType::LEGACY;
        return true;
    } else if (type == OUTPUT_TYPE_STRING_P2SH_SEGWIT) {
        output_type = OutputType::P2SH_SEGWIT;
        return true;
    } else if (type == OUTPUT_TYPE_STRING_NONPRIVATE) {
        output_type = OutputType::NONPRIVATE;
        return true;
    } else if (type == OUTPUT_TYPE_STRING_WITHVIEWKEY) {
        output_type = OutputType::WITHVIEWKEY;
        return true;
    }/* else if (type == OUTPUT_TYPE_STRING_BECH32) {
        output_type = OutputType::BECH32;
        return true;
    }*/
    return false;
}

const std::string& FormatOutputType(OutputType type)
{
    switch (type) {
    case OutputType::LEGACY: return OUTPUT_TYPE_STRING_LEGACY;
    case OutputType::NONPRIVATE: return OUTPUT_TYPE_STRING_NONPRIVATE;
    case OutputType::WITHVIEWKEY: return OUTPUT_TYPE_STRING_WITHVIEWKEY;
    case OutputType::P2SH_SEGWIT: return OUTPUT_TYPE_STRING_P2SH_SEGWIT;
    case OutputType::BECH32: return OUTPUT_TYPE_STRING_BECH32;
    default: assert(false);
    }
}

void CWallet::LearnRelatedScripts(const CPubKey& key, OutputType type)
{
    if (key.IsCompressed() && (type == OutputType::P2SH_SEGWIT || type == OutputType::BECH32)) {
        CTxDestination witdest = WitnessV0KeyHash(key.GetID());
        CScript witprog = GetScriptForDestination(witdest);
        // Make sure the resulting program is solvable.
        assert(IsSolvable(*this, witprog));
        AddCScript(witprog);
    }
}

void CWallet::LearnAllRelatedScripts(const CPubKey& key)
{
    // OutputType::P2SH_SEGWIT always adds all necessary scripts for all types.
    LearnRelatedScripts(key, OutputType::P2SH_SEGWIT);
}

CTxDestination GetDestinationForKeyInner(const CPubKey& key, OutputType type)
{
    switch (type) {
    case OutputType::LEGACY: return key.GetID();
    case OutputType::NONPRIVATE: return key.GetID();
    case OutputType::WITHVIEWKEY: return key.GetID();
    case OutputType::P2SH_SEGWIT:
    case OutputType::BECH32: {
        if (!key.IsCompressed()) return key.GetID();
        CTxDestination witdest = WitnessV0KeyHash(key.GetID());
        CScript witprog = GetScriptForDestination(witdest);
        if (type == OutputType::P2SH_SEGWIT) {
            return CScriptID(witprog);
        } else {
            return witdest;
        }
    }
    default: assert(false);
    }
}

CTxDestination GetDestinationForKey(const CPubKey& key, OutputType type, const CPubKey& viewkey)
{
    CTxDestination dest;
    dest=GetDestinationForKeyInner(key,type);

    if (auto id = boost::get<CKeyID>(&dest)) {
        id->recokey.resize(33);
        std::copy(key.begin(), key.end() , id->recokey.begin());
        id->nonprivate = (type == OutputType::NONPRIVATE);
        if (type == OutputType::WITHVIEWKEY)
        {
            id->hasviewkey = true;
            id->viewkey.resize(33);
            std::copy(viewkey.begin(), viewkey.end() , id->viewkey.begin());
        } else
        {
            id->hasviewkey = false;
        }
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        id->recokey.resize(33);
        std::copy(key.begin(), key.end() , id->recokey.begin());
    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        id->recokey.resize(33);
        std::copy(key.begin(), key.end() , id->recokey.begin());
    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        id->recokey.resize(33);
        std::copy(key.begin(), key.end() , id->recokey.begin());
    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        id->recokey.resize(33);
        std::copy(key.begin(), key.end() , id->recokey.begin());
    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        id->recokey.resize(33);
        std::copy(key.begin(), key.end() , id->recokey.begin());
    }
    return dest;   
}

CPubKey GetSecondPubKeyForDestination(const CTxDestination& dest)
{
    CPubKey key2;

    if (auto id = boost::get<CKeyID>(&dest)) {
        key2=CPubKey(id->recokey); 
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        key2=CPubKey(id->recokey); 
    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        key2=CPubKey(id->recokey); 
    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        key2=CPubKey(id->recokey); 
    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        key2=CPubKey(id->recokey); 
    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        key2=CPubKey(id->recokey); 
    }
    return key2;   
}

void SetSecondPubKeyForDestination(CTxDestination& dest, const CPubKey& key2)
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

CPubKey GetViewPubKeyForDestination(const CTxDestination& dest)
{
    CPubKey key2;

    if (auto id = boost::get<CKeyID>(&dest)) {
        key2=CPubKey(id->viewkey); 
    }
    return key2;   
}

bool GetHasViewKeyForDestination(const CTxDestination& dest)
{
    bool key2 = false;

    if (auto id = boost::get<CKeyID>(&dest)) {
        key2=id->hasviewkey;
    }
    return key2;   
}

void SetHasViewKeyForDestination(CTxDestination& dest, bool hasviewkey)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->hasviewkey = hasviewkey;
    }
}

void SetViewPubKeyForDestination(CTxDestination& dest, const CPubKey& key2)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->viewkey.resize(33);
        std::copy(key2.begin(), key2.end() , id->viewkey.begin());
    }
}

unsigned char GetCurrencyForDestination(const CTxDestination& dest)
{
    unsigned char key2 = 0;

    if (auto id = boost::get<CKeyID>(&dest)) {
        key2=id->currency;
    }
    if (auto id = boost::get<WitnessV0ScriptHash>(&dest)) {
        key2=id->currency; 
    }
    if (auto id = boost::get<WitnessV0KeyHash>(&dest)) {
        key2=id->currency; 
    }
    if (auto id = boost::get<CScriptID>(&dest)) {
        key2=id->currency; 
    }
    if (auto id = boost::get<WitnessUnknown>(&dest)) {
        key2=id->currency; 
    }
    if (auto id = boost::get<CNoDestination>(&dest)) {
        key2=id->currency;
    }
    return key2;   
}

bool GetNonPrivateForDestination(const CTxDestination& dest)
{
    bool key2 = false;

    if (auto id = boost::get<CKeyID>(&dest)) {
        key2=id->nonprivate;
    }
    return key2;   
}

void SetNonPrivateForDestination(CTxDestination& dest, bool isnonprivate)
{
    if (auto id = boost::get<CKeyID>(&dest)) {
        id->nonprivate = isnonprivate;
    }
}

void SetCurrencyForDestination(CTxDestination& dest, const unsigned char key2)
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

void SetDepositForDestination(CTxDestination& dest, const bool isdeposit)
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

bool GetDepositForDestination(const CTxDestination& dest)
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

std::vector<CTxDestination> GetAllDestinationsForKey(const CPubKey& key)
{    
    CKeyID keyid = key.GetID();
    CTxDestination normal = keyid;
    SetSecondPubKeyForDestination(normal,key);
    return std::vector<CTxDestination>{std::move(normal)};
}

CTxDestination CWallet::AddAndGetDestinationForScript(const CScript& script, OutputType type)
{
    // Note that scripts over 520 bytes are not yet supported.
    switch (type) {
    case OutputType::LEGACY:
        return CScriptID(script);
    case OutputType::NONPRIVATE:
        return CScriptID(script);
    case OutputType::P2SH_SEGWIT:
    case OutputType::BECH32: {
        WitnessV0ScriptHash hash;
        CSHA256().Write(script.data(), script.size()).Finalize(hash.begin());
        CTxDestination witdest = hash;
        CScript witprog = GetScriptForDestination(witdest);
        // Check if the resulting program is solvable (i.e. doesn't use an uncompressed key)
        if (!IsSolvable(*this, witprog)) return CScriptID(script);
        // Add the redeemscript, so that P2WSH and P2SH-P2WSH outputs are recognized as ours.
        AddCScript(witprog);
        if (type == OutputType::BECH32) {
            return witdest;
        } else {
            return CScriptID(witprog);
        }
    }
    default: assert(false);
    }
}
