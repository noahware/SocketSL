#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sl::request
{
	using request_id_t = std::uint8_t;
	using request_buffer_size_t = std::uint64_t;

	// kept _t suffix: `request` would shadow the enclosing namespace
	struct request_t
	{
		std::size_t header_size;
		std::vector<std::uint8_t> buffer;
	};
}
