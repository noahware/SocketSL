#pragma once
#include <vector>

#include <openssl/x509.h>

namespace crypto::sha256
{
	constexpr std::uint64_t hash_size = 32;
	typedef std::array<std::uint8_t, hash_size> hash_t;

	hash_t hash(const std::vector<std::uint8_t>& input, std::uint8_t& status_out);
}
