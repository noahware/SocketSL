#pragma once
#include "../network/socket.hpp"
#include "../serialisation/serialisation.hpp"
#include <vector>

namespace sl::response
{
	void async_send_buffer(sl::socket& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, const async_callback_t& handler);
	void read_buffer(sl::socket& socket, std::vector<std::uint8_t>& buffer);

	template <class T>
	[[nodiscard]] const T* read_response(sl::socket& socket, std::vector<std::uint8_t>& buffer)
	{
		read_buffer(socket, buffer);

		return serialisation::deserialise<T>(buffer);
	}

	[[nodiscard]] std::vector<std::uint8_t> make_test_response(std::uint64_t key);
}
