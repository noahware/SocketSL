#pragma once
#include "../network/socket.hpp"
#include "request_def.hpp"

namespace sl::request
{
	void send_buffer(sl::socket& socket, const void* buffer, std::size_t total_buffer_size, std::size_t header_size);
	void send_buffer(sl::socket& socket, const std::vector<std::uint8_t>& buffer, std::size_t header_size);

	[[nodiscard]] std::vector<std::uint8_t> make_request_header(request_id_t request_id, std::size_t body_size);

	[[nodiscard]] request_t make_test_request(std::uint64_t key);
}
