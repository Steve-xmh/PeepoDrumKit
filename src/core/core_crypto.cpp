#include "core_crypto.h"
#include <cstring>
#include <random>

#define STB_IMAGE_STATIC
#define STBI_ONLY_ZLIB
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

// CRC32 查找表（用于 gzip）
static u32 crc32_table[256];
static bool crc32_table_initialized = false;

static void init_crc32_table() {
    if (crc32_table_initialized) return;
    for (u32 i = 0; i < 256; i++) {
        u32 crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

static u32 compute_crc32(const u8* data, size_t size) {
    init_crc32_table();
    u32 crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

std::vector<u8> EncryptedFile::EncryptData(const u8* data, size_t size, const std::vector<u8>& key)
{
    if (size == 0) {
        return {};
    }

    // 使用 stb_image_write 的 zlib 压缩
    int compressedLen = 0;
    unsigned char* zlibData = stbi_zlib_compress(
        const_cast<unsigned char*>(data),
        static_cast<int>(size),
        &compressedLen,
        8  // quality/compression level
    );
    
    if (!zlibData || compressedLen < 2) {
        if (zlibData) STBIW_FREE(zlibData);
        return {};
    }
    
    // stbi_zlib_compress 返回带 zlib 头 (2字节) 和 adler32 尾 (4字节) 的数据
    // 我们需要提取 raw deflate 数据（去掉 zlib 头尾）
    size_t rawDeflateSize = compressedLen - 2 - 4;  // 去掉 2字节头 + 4字节 adler32
    const u8* rawDeflateData = zlibData + 2;
    
    // 计算 CRC32
    u32 crc32 = compute_crc32(data, size);
    
    // 构建 gzip 格式数据
    // gzip 头: 10 字节固定头
    // 压缩数据: raw deflate
    // gzip 尾: 4 字节 CRC32 + 4 字节原始大小
    std::vector<u8> gzipData;
    gzipData.reserve(10 + rawDeflateSize + 8);
    
    // Gzip 头
    gzipData.push_back(0x1f);  // magic
    gzipData.push_back(0x8b);  // magic
    gzipData.push_back(0x08);  // compression method (deflate)
    gzipData.push_back(0x00);  // flags (无额外字段)
    gzipData.push_back(0x00);  // mtime (4 bytes)
    gzipData.push_back(0x00);
    gzipData.push_back(0x00);
    gzipData.push_back(0x00);
    gzipData.push_back(0x00);  // extra flags
    gzipData.push_back(0xff);  // OS (unknown)
    
    // Raw deflate 数据
    gzipData.insert(gzipData.end(), rawDeflateData, rawDeflateData + rawDeflateSize);
    
    // CRC32 (小端序)
    gzipData.push_back(crc32 & 0xFF);
    gzipData.push_back((crc32 >> 8) & 0xFF);
    gzipData.push_back((crc32 >> 16) & 0xFF);
    gzipData.push_back((crc32 >> 24) & 0xFF);
    
    // 原始大小 (小端序, 取模 2^32)
    u32 origSize = static_cast<u32>(size);
    gzipData.push_back(origSize & 0xFF);
    gzipData.push_back((origSize >> 8) & 0xFF);
    gzipData.push_back((origSize >> 16) & 0xFF);
    gzipData.push_back((origSize >> 24) & 0xFF);
    
    STBIW_FREE(zlibData);
    
    // 生成随机 IV
    u8 iv[16];
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < 16; i++) {
        iv[i] = static_cast<u8>(dis(gen));
    }
    
    // 计算加密后大小（包含 PKCS7 填充）
    size_t paddedSize = ((gzipData.size() + 15) / 16) * 16;
    std::vector<u8> encrypted(paddedSize);
    
    // AES-256-CBC 加密
    plusaes::Error err = plusaes::encrypt_cbc(
        gzipData.data(),
        static_cast<unsigned long>(gzipData.size()),
        key.data(),
        static_cast<unsigned long>(key.size()),
        reinterpret_cast<const unsigned char(*)[16]>(&iv),
        encrypted.data(),
        static_cast<unsigned long>(encrypted.size()),
        true  // 使用 PKCS7 填充
    );
    
    if (err != plusaes::kErrorOk) {
        return {}; // 加密失败
    }
    
    // 构建最终结果: IV + 加密数据
    std::vector<u8> result;
    result.reserve(16 + encrypted.size());
    result.insert(result.end(), iv, iv + 16);
    result.insert(result.end(), encrypted.begin(), encrypted.end());
    
    return result;
}

std::vector<u8> EncryptedFile::DecryptData(const u8* data, size_t size, const std::vector<u8>& key)
{
    if (size < 16) {
        return {}; // 数据太短，至少需要 IV
    }

    // 读取 IV (前 16 字节)
    u8 iv[16];
    std::memcpy(iv, data, 16);
    
    const u8* encryptedData = data + 16;
    size_t encryptedSize = size - 16;
    
    if (encryptedSize == 0 || encryptedSize % 16 != 0) {
        return {}; // 加密数据大小必须是 16 的倍数
    }

    // AES-256-CBC 解密
    std::vector<u8> decrypted(encryptedSize);
    unsigned long paddedSize = 0;
    
    plusaes::Error err = plusaes::decrypt_cbc(
        encryptedData,
        static_cast<unsigned long>(encryptedSize),
        key.data(),
        static_cast<unsigned long>(key.size()),
        reinterpret_cast<const unsigned char(*)[16]>(&iv),
        decrypted.data(),
        static_cast<unsigned long>(decrypted.size()),
        &paddedSize
    );
    
    if (err != plusaes::kErrorOk) {
        return {}; // 解密失败
    }
    
    // plusaes 已经处理了 PKCS7 padding，调整大小
    decrypted.resize(encryptedSize - paddedSize);

    // 解析 Gzip 头并解压
    // Gzip 格式: 
    // - 2 bytes: magic (0x1f, 0x8b)
    // - 1 byte: compression method (8 = deflate)
    // - 1 byte: flags
    // - 4 bytes: modification time
    // - 1 byte: extra flags
    // - 1 byte: OS
    // - 可选字段（根据 flags）
    // - 压缩数据
    // - 4 bytes: CRC32
    // - 4 bytes: 原始大小
    
    if (decrypted.size() < 10) {
        return {}; // 数据太短，不是有效的 gzip
    }
    
    if (decrypted[0] != 0x1f || decrypted[1] != 0x8b) {
        return {}; // 不是 gzip magic number
    }
    
    if (decrypted[2] != 8) {
        return {}; // 不支持的压缩方法
    }
    
    u8 flags = decrypted[3];
    size_t offset = 10; // 跳过固定头部
    
    // FEXTRA
    if (flags & 0x04) {
        if (offset + 2 > decrypted.size()) return {};
        u16 extraLen = decrypted[offset] | (decrypted[offset + 1] << 8);
        offset += 2 + extraLen;
    }
    
    // FNAME (null-terminated string)
    if (flags & 0x08) {
        while (offset < decrypted.size() && decrypted[offset] != 0) {
            offset++;
        }
        offset++; // 跳过 null 终止符
    }
    
    // FCOMMENT (null-terminated string)
    if (flags & 0x10) {
        while (offset < decrypted.size() && decrypted[offset] != 0) {
            offset++;
        }
        offset++; // 跳过 null 终止符
    }
    
    // FHCRC (2 bytes)
    if (flags & 0x02) {
        offset += 2;
    }
    
    if (offset >= decrypted.size()) {
        return {}; // 头部超出数据范围
    }
    
    // 压缩数据（不包含末尾的 CRC32 和原始大小，各 4 字节）
    size_t compressedSize = decrypted.size() - offset - 8;
    if (compressedSize <= 0) {
        return {};
    }
    
    // 从 gzip 尾部读取原始大小（小端序）
    size_t originalSize = 
        decrypted[decrypted.size() - 4] |
        (decrypted[decrypted.size() - 3] << 8) |
        (decrypted[decrypted.size() - 2] << 16) |
        (decrypted[decrypted.size() - 1] << 24);
    
    // 使用 stb_image 的 zlib 解码（raw deflate，无头部）
    int outLen = 0;
    char* decompressed = stbi_zlib_decode_noheader_malloc(
        reinterpret_cast<const char*>(decrypted.data() + offset),
        static_cast<int>(compressedSize),
        &outLen
    );
    
    if (!decompressed) {
        return {}; // 解压失败
    }
    
    std::vector<u8> result(decompressed, decompressed + outLen);
    STBI_FREE(decompressed);
    
    return result;
}