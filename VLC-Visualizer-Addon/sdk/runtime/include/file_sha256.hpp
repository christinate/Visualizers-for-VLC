// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <array>
#include <sstream>
#include <iomanip>
#include <stdexcept>
namespace filehash {
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE handle):value(handle){}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    Handle(Handle&& other) noexcept:value(other.value){other.value=INVALID_HANDLE_VALUE;}
    ~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
inline std::string sha256(HANDLE file){
    LARGE_INTEGER start{};if(!SetFilePointerEx(file,start,nullptr,FILE_BEGIN))throw std::runtime_error("Cannot rewind file for SHA-256");
    struct Hash {
        BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE digest=nullptr;
        ~Hash(){if(digest)BCryptDestroyHash(digest);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    } state;
    if(BCryptOpenAlgorithmProvider(&state.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||
       BCryptCreateHash(state.algorithm,&state.digest,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("SHA-256 provider failed");
    std::array<unsigned char,65536> buffer{};DWORD count=0;
    for(;;){
        if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&count,nullptr))throw std::runtime_error("Cannot read file for SHA-256");
        if(!count)break;
        if(BCryptHashData(state.digest,buffer.data(),count,0)<0)throw std::runtime_error("SHA-256 update failed");
    }
    std::array<unsigned char,32> digest{};
    if(BCryptFinishHash(state.digest,digest.data(),DWORD(digest.size()),0)<0)throw std::runtime_error("SHA-256 failed");
    std::ostringstream output;for(auto c:digest)output<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c);return output.str();
}
inline std::string sha256(const std::filesystem::path& path){
    auto native=path;native.make_preferred();
    Handle file(CreateFileW(native.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open file for SHA-256 (Win32 "+std::to_string(GetLastError())+")");
    return sha256(file.value);
}
}
