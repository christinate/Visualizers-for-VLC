// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "catalog.hpp"
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/evp.h>
#endif
namespace native {
class FileDigest {
#ifdef __APPLE__
    CC_SHA256_CTX context{};
#else
    EVP_MD_CTX* context=nullptr;
#endif
public:
    FileDigest(){
#ifdef __APPLE__
        if(!CC_SHA256_Init(&context))throw std::runtime_error("Cannot initialize SHA-256");
#else
        context=EVP_MD_CTX_new();if(!context)throw std::runtime_error("Cannot allocate SHA-256 context");
        if(EVP_DigestInit_ex(context,EVP_sha256(),nullptr)!=1){EVP_MD_CTX_free(context);context=nullptr;throw std::runtime_error("Cannot initialize SHA-256");}
#endif
    }
    FileDigest(const FileDigest&)=delete;FileDigest& operator=(const FileDigest&)=delete;
    ~FileDigest(){
#ifndef __APPLE__
        EVP_MD_CTX_free(context);
#endif
    }
    void append(const void* data,size_t size){
#ifdef __APPLE__
        if(size>UINT32_MAX||!CC_SHA256_Update(&context,data,CC_LONG(size)))throw std::runtime_error("SHA-256 update failed");
#else
        if(EVP_DigestUpdate(context,data,size)!=1)throw std::runtime_error("SHA-256 update failed");
#endif
    }
    std::string finish(){
        std::array<unsigned char,32> digest{};
#ifdef __APPLE__
        if(!CC_SHA256_Final(digest.data(),&context))throw std::runtime_error("SHA-256 finish failed");
#else
        unsigned int length=0;if(EVP_DigestFinal_ex(context,digest.data(),&length)!=1||length!=digest.size())throw std::runtime_error("SHA-256 finish failed");
#endif
        std::ostringstream output;for(auto value:digest)output<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(value);return output.str();
    }
};
inline std::string contentDigest(const fs::path& file){
    FileDigest digest;
    if(fs::is_symlink(file)){
        const char prefix[]="symlink\0";digest.append(prefix,sizeof(prefix)-1);
        auto target=fs::read_symlink(file).native();digest.append(target.data(),target.size());return digest.finish();
    }
    std::ifstream input(file,std::ios::binary);if(!input)throw std::runtime_error("Cannot open file for hashing");
    std::array<char,65536> buffer{};while(input){input.read(buffer.data(),buffer.size());if(input.gcount())digest.append(buffer.data(),size_t(input.gcount()));}
    if(!input.eof())throw std::runtime_error("Cannot read file for hashing");return digest.finish();
}
}
