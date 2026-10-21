#include "game_data.h"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/evp.h>
#endif

namespace n3::data {
std::string Sha256(std::span<const uint8_t> bytes) {
  std::array<uint8_t, 32> digest{};
#ifdef _WIN32
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    throw std::runtime_error("Cannot initialize SHA-256");
  auto status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
  while (status >= 0 && !bytes.empty()) {
    const auto count = static_cast<ULONG>(std::min<size_t>(bytes.size(), std::numeric_limits<ULONG>::max()));
    status = BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()), count, 0);
    bytes = bytes.subspan(count);
  }
  if (status >= 0) status = BCryptFinishHash(hash, digest.data(), digest.size(), 0);
  if (hash) BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (status < 0) throw std::runtime_error("SHA-256 calculation failed");
#elif defined(__APPLE__)
  CC_SHA256_CTX context;
  if (!CC_SHA256_Init(&context)) throw std::runtime_error("Cannot initialize SHA-256");
  while (!bytes.empty()) {
    const auto count = static_cast<CC_LONG>(std::min<size_t>(bytes.size(), std::numeric_limits<CC_LONG>::max()));
    if (!CC_SHA256_Update(&context, bytes.data(), count))
      throw std::runtime_error("SHA-256 calculation failed");
    bytes = bytes.subspan(count);
  }
  if (!CC_SHA256_Final(digest.data(), &context))
    throw std::runtime_error("SHA-256 calculation failed");
#else
  unsigned int size = 0;
  if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &size, EVP_sha256(), nullptr) != 1 ||
      size != digest.size()) throw std::runtime_error("SHA-256 calculation failed");
#endif
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(digest.size() * 2);
  for (auto value : digest) { result += hex[value >> 4]; result += hex[value & 15]; }
  return result;
}
}
