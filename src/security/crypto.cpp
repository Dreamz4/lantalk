#include "crypto.hpp"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace lantalk {

namespace {

std::string bytesToHex(const unsigned char* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

} // namespace

std::vector<uint8_t> Crypto::randomBytes(size_t count) {
    std::vector<uint8_t> buffer(count);
    if (RAND_bytes(buffer.data(), static_cast<int>(count)) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }
    return buffer;
}

std::string Crypto::generateUUID() {
    auto bytes = randomBytes(16);
    // UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // Set version to 4
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    // Set variant to 10
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            ss << "-";
        }
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

std::string Crypto::sha256(const std::string& input) {
    return sha256(std::vector<uint8_t>(input.begin(), input.end()));
}

std::string Crypto::sha256(const std::vector<uint8_t>& input) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) throw std::runtime_error("Failed to create context");

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    if (1 != EVP_DigestInit_ex(context, EVP_sha256(), nullptr) ||
        1 != EVP_DigestUpdate(context, input.data(), input.size()) ||
        1 != EVP_DigestFinal_ex(context, hash, &lengthOfHash)) {
        EVP_MD_CTX_free(context);
        throw std::runtime_error("Failed to compute SHA256");
    }

    EVP_MD_CTX_free(context);
    return bytesToHex(hash, lengthOfHash);
}

std::string Crypto::hmacSha256(const std::string& key, const std::string& message) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;
    
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0);
    params[1] = OSSL_PARAM_construct_end();

    if (1 != EVP_MAC_init(ctx, reinterpret_cast<const unsigned char*>(key.c_str()), key.length(), params) ||
        1 != EVP_MAC_update(ctx, reinterpret_cast<const unsigned char*>(message.c_str()), message.length()) ||
        1 != EVP_MAC_final(ctx, hash, reinterpret_cast<size_t*>(&lengthOfHash), sizeof(hash))) {
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        throw std::runtime_error("Failed to compute HMAC");
    }
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
#else
    HMAC_CTX* ctx = HMAC_CTX_new();
    if (1 != HMAC_Init_ex(ctx, key.c_str(), key.length(), EVP_sha256(), nullptr) ||
        1 != HMAC_Update(ctx, reinterpret_cast<const unsigned char*>(message.c_str()), message.length()) ||
        1 != HMAC_Final(ctx, hash, &lengthOfHash)) {
        HMAC_CTX_free(ctx);
        throw std::runtime_error("Failed to compute HMAC");
    }
    HMAC_CTX_free(ctx);
#endif
    return bytesToHex(hash, lengthOfHash);
}

std::string Crypto::generateChallenge() {
    return base64Encode(randomBytes(32));
}

std::string Crypto::base64Encode(const std::vector<uint8_t>& data) {
    BIO* bio = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    bio = BIO_push(bio, bmem);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(bio, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(bio);
    return result;
}

std::vector<uint8_t> Crypto::base64Decode(const std::string& encoded) {
    BIO* bio = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    bio = BIO_push(bio, bmem);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines
    std::vector<uint8_t> buffer(encoded.size());
    int len = BIO_read(bio, buffer.data(), static_cast<int>(buffer.size()));
    BIO_free_all(bio);
    if (len < 0) {
        return {};
    }
    buffer.resize(len);
    return buffer;
}

bool Crypto::secureCompare(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) return false;
    return CRYPTO_memcmp(a.data(), b.data(), a.length()) == 0;
}

bool Crypto::generateSelfSignedCert(
    const std::filesystem::path& certFile,
    const std::filesystem::path& keyFile,
    const std::string& commonName) {

    // Generate 2048-bit RSA key using modern EVP_PKEY_CTX API (OpenSSL 3.x)
    EVP_PKEY* pkey = nullptr;
    {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        if (!ctx) return false;
        if (EVP_PKEY_keygen_init(ctx) <= 0 ||
            EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
            EVP_PKEY_keygen(ctx, &pkey) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        EVP_PKEY_CTX_free(ctx);
    }
    if (!pkey) return false;

    X509* x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 315360000L); // 10 years

    X509_set_pubkey(x509, pkey);

    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, 
                               reinterpret_cast<const unsigned char*>(commonName.c_str()), 
                               -1, -1, 0);
    X509_set_issuer_name(x509, name);

    if (!X509_sign(x509, pkey, EVP_sha256())) {
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return false;
    }

    // Write key
    FILE* f = fopen(keyFile.string().c_str(), "wb");
    if (!f) return false;
    PEM_write_PrivateKey(f, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(f);

    // Write cert
    f = fopen(certFile.string().c_str(), "wb");
    if (!f) return false;
    PEM_write_X509(f, x509);
    fclose(f);

    X509_free(x509);
    EVP_PKEY_free(pkey);
    return true;
}

std::pair<std::filesystem::path, std::filesystem::path>
Crypto::getOrCreateTlsCredentials(const std::filesystem::path& configDir,
                                  const std::string& deviceId) {
    
    std::filesystem::path certPath = configDir / "lantalk_cert.pem";
    std::filesystem::path keyPath = configDir / "lantalk_key.pem";

    if (!std::filesystem::exists(certPath) || !std::filesystem::exists(keyPath)) {
        std::filesystem::create_directories(configDir);
        if (!generateSelfSignedCert(certPath, keyPath, deviceId)) {
            throw std::runtime_error("Failed to generate TLS credentials");
        }
    }

    return {certPath, keyPath};
}

} // namespace lantalk
