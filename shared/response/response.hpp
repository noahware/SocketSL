#pragma once
#include "../network/socket.hpp"
#include "../serialisation/serialisation.hpp"

#include <vector>

namespace sl::response
{
	void async_send_buffer(sl::socket& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, const async_callback_t& handler);
	void read_buffer(sl::socket& socket, std::vector<std::uint8_t>& buffer);

	template <class T>
	[[nodiscard]] const T* read(sl::socket& socket, std::vector<std::uint8_t>& buffer)
	{
		read_buffer(socket, buffer);

		return serialisation::deserialise<T>(buffer);
	}

	template <class CreateFn, class... Args>
	[[nodiscard]] std::vector<std::uint8_t> make(CreateFn&& create_fn, Args&&... args)
	{
		return serialisation::serialise(std::forward<CreateFn>(create_fn), std::forward<Args>(args)...);
	}

	template <class CreateFn, class... Args>
	void send(sl::socket& socket, const async_callback_t& handler, CreateFn&& create_fn, Args&&... args)
	{
		auto buffer = std::make_shared<std::vector<std::uint8_t>>(
			make(std::forward<CreateFn>(create_fn), std::forward<Args>(args)...));
		async_send_buffer(socket, buffer, handler);
	}
}
