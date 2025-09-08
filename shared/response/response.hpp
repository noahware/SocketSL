#pragma once
#include "../network/socket.hpp"
#include "../serialisation/serialisation.hpp"
#include <vector>

namespace response
{
	void async_send_buffer(socket_t& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, const async_callback_t& handler);
	void read_buffer(socket_t& socket, std::vector<std::uint8_t>& buffer);

	template <class t>
	const t* read_response(socket_t& socket, std::vector<std::uint8_t>& buffer)
	{
		response::read_buffer(socket, buffer);

		return serialisation::deserialise<t>(buffer);
	}

	namespace construct
	{
		std::vector<std::uint8_t> make_test_response(std::uint64_t key);
	}
}
