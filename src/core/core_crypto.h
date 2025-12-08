#pragma once
#include "core_types.h"
#include "plusaes/plusaes.hpp"
#include <vector>

class EncryptedFile {
protected:
    const std::vector<u8> EncryptionKey = {};
    
public:
    // 解密数据并解压 gzip，返回解压后的数据
    // 数据格式: [16字节IV][AES-256-CBC加密的gzip数据(PKCS7填充)]
    static std::vector<u8> DecryptData(const u8* data, size_t size, const std::vector<u8>& key);
    
    // 压缩数据并加密，返回加密后的数据
    // 数据格式: [16字节IV][AES-256-CBC加密的gzip数据(PKCS7填充)]
    static std::vector<u8> EncryptData(const u8* data, size_t size, const std::vector<u8>& key);
};

class EncryptedDataTable : public EncryptedFile {
public:
    static inline const std::vector<u8> EncryptionKey = plusaes::key_from_string(&"500BB263557B418A951483FC2FFB15E4");
    
    static std::vector<u8> Decrypt(const u8* data, size_t size) {
        return DecryptData(data, size, EncryptionKey);
    }
    
    static std::vector<u8> Encrypt(const u8* data, size_t size) {
        return EncryptData(data, size, EncryptionKey);
    }
};

class EncryptedFumenV2 : public EncryptedFile {
public:
    static inline const std::vector<u8> EncryptionKey = plusaes::key_from_string(&"D4B9F85708BC4C80038CDA2439751580");
    
    static std::vector<u8> Decrypt(const u8* data, size_t size) {
        return DecryptData(data, size, EncryptionKey);
    }
    
    static std::vector<u8> Encrypt(const u8* data, size_t size) {
        return EncryptData(data, size, EncryptionKey);
    }
};
