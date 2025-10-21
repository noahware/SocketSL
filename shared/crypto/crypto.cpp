#include "crypto.hpp"

#include <openssl/evp.h>
#include <spdlog/spdlog.h>

static void free_evp_md_context(evp_md_ctx_st* context)
{
	EVP_MD_CTX_free(context);
}

std::vector<std::uint8_t> do_evp_hash(const std::span<std::uint8_t>& input, const EVP_MD* type)
{
	constexpr std::int32_t success_status = 1;

	evp_md_ctx_st* const context = EVP_MD_CTX_new();

	if (!context)
	{
		spdlog::error("unable to create EVP MD context");

		return { };
	}

	if (EVP_DigestInit_ex(context, type, nullptr) != success_status)
	{
		spdlog::error("EVP digest initialisation failed");

		free_evp_md_context(context);

		return { };
	}

	if (EVP_DigestUpdate(context, input.data(), input.size()) != success_status)
	{
		spdlog::error("EVP digest update failed");

		free_evp_md_context(context);

		return { };
	}

	std::array<std::uint8_t, EVP_MAX_MD_SIZE> hash = { };

	std::uint32_t length_of_hash = 0;

	if (EVP_DigestFinal(context, hash.data(), &length_of_hash) != success_status)
	{
		spdlog::error("EVP digest final failed");

		free_evp_md_context(context);

		return { };
	}

	free_evp_md_context(context);

	return { hash.data(), hash.data() + length_of_hash };
}

crypto::sha256::hash_t crypto::sha256::hash(const std::span<std::uint8_t>& input, std::uint8_t& status_out)
{
	const std::vector<std::uint8_t> hash = do_evp_hash(input, EVP_sha256());

	if (hash.size() != hash_size)
	{
		spdlog::error("sha256 hash size ({}) is incorrect", hash.size());

		status_out = 0;

		return { };
	}

	std::array<std::uint8_t, hash_size> buffer = { };

	std::copy_n(hash.data(), hash_size, buffer.begin());

	status_out = 1;

	return buffer;
}
