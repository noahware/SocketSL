#pragma once
#include "../network/socket.hpp"
#include "request_def.hpp"

namespace request
{
	void send_buffer(socket_t& socket, const void* buffer, request_buffer_size_t total_buffer_size, request_buffer_size_t header_size);
	void send_buffer(socket_t& socket, const std::vector<std::uint8_t>& buffer, request_buffer_size_t header_size);

	namespace construct
	{
		std::vector<std::uint8_t> make_request_header(request_id_t request_id, std::uint64_t body_size);

		request_t make_test_request(std::uint64_t key);
	}
}
