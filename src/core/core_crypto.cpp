#include "core_crypto.h"
#include <cstring>
#include <random>
#include <gzip/compress.hpp>
#include <gzip/decompress.hpp>

std::vector<u8> EncryptedFile::EncryptData(const u8 *data, size_t size, const std::vector<u8> &key)
{
    if (size == 0)
    {
        return {};
    }

    // 使用 gzip-hpp 压缩
    std::string compressed = gzip::compress(
        reinterpret_cast<const char *>(data),
        size);

    if (compressed.empty())
    {
        return {};
    }

    // gzip::compress 直接返回完整的 gzip 格式数据
    std::vector<u8> gzipData(compressed.begin(), compressed.end());

    // 生成随机 IV
    u8 iv[16];
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < 16; i++)
    {
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
        reinterpret_cast<const unsigned char (*)[16]>(&iv),
        encrypted.data(),
        static_cast<unsigned long>(encrypted.size()),
        true // 使用 PKCS7 填充
    );

    if (err != plusaes::kErrorOk)
    {
        return {}; // 加密失败
    }

    // 构建最终结果: IV + 加密数据
    std::vector<u8> result;
    result.reserve(16 + encrypted.size());
    result.insert(result.end(), iv, iv + 16);
    result.insert(result.end(), encrypted.begin(), encrypted.end());

    return result;
}

std::vector<u8> EncryptedFile::DecryptData(const u8 *data, size_t size, const std::vector<u8> &key)
{
    if (size < 16)
    {
        return {}; // 数据太短，至少需要 IV
    }

    // 读取 IV (前 16 字节)
    u8 iv[16];
    std::memcpy(iv, data, 16);

    const u8 *encryptedData = data + 16;
    size_t encryptedSize = size - 16;

    if (encryptedSize == 0 || encryptedSize % 16 != 0)
    {
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
        reinterpret_cast<const unsigned char (*)[16]>(&iv),
        decrypted.data(),
        static_cast<unsigned long>(decrypted.size()),
        &paddedSize);

    if (err != plusaes::kErrorOk)
    {
        return {}; // 解密失败
    }

    // plusaes 已经处理了 PKCS7 padding，调整大小
    decrypted.resize(encryptedSize - paddedSize);

    // 使用 gzip-hpp 解压完整的 gzip 数据
    std::string decompressed = gzip::decompress(
        reinterpret_cast<const char *>(decrypted.data()),
        decrypted.size());

    if (decompressed.empty())
    {
        return {}; // 解压失败
    }

    std::vector<u8> result(decompressed.begin(), decompressed.end());

    return result;
}