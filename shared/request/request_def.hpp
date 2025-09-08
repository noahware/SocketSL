#pragma once
#include <cstdint>

namespace request
{
	typedef std::uint8_t request_id_t;
	typedef std::uint64_t request_buffer_size_t;

	struct request_t
	{
		request_buffer_size_t header_size;
		std::vector<std::uint8_t> buffer;
	};
}
