#pragma once
#include "../network/socket.hpp"
#include "request_def.hpp"

#include <span>

namespace sl::request
{
	void send_buffer(sl::socket& socket, std::span<const std::uint8_t> buffer, std::size_t header_size);

	[[nodiscard]] std::vector<std::uint8_t> make_request_header(request_id_t request_id, std::size_t body_size);

	[[nodiscard]] request_t make_test_request(std::uint64_t key);
}
