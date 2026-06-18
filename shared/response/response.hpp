#pragma once
#include "../network/socket.hpp"
#include "../serialisation/serialisation.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace sl::response
{
	using response_id_t = std::uint8_t;
	using response_buffer_size_t = std::uint64_t;

	struct response_t
	{
		std::size_t header_size = 0;
		std::vector<std::uint8_t> buffer = {};
	};

	void async_send_buffer(sl::socket& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, std::size_t header_size, const async_callback_t& handler);

	[[nodiscard]] std::vector<std::uint8_t> make_header(response_id_t id, std::size_t body_size);
	[[nodiscard]] response_t make_from_body(response_id_t id, std::span<const std::uint8_t> body);

	response_id_t read_buffer(sl::socket& socket, std::vector<std::uint8_t>& body_buffer);

	template <class T>
	[[nodiscard]] const T* read(sl::socket& socket, std::vector<std::uint8_t>& buffer)
	{
		(void)read_buffer(socket, buffer);

		return serialisation::deserialise<T>(buffer);
	}

	template <class CreateFn, class... Args>
	[[nodiscard]] response_t make(const response_id_t id, CreateFn&& create_fn, Args&&... args)
	{
		const auto body = serialisation::serialise(std::forward<CreateFn>(create_fn), std::forward<Args>(args)...);
		return make_from_body(id, body);
	}

	template <class CreateFn, class... Args>
	void send(sl::socket& socket, const response_id_t id, const async_callback_t& handler, CreateFn&& create_fn, Args&&... args)
	{
		auto [header_size, buffer] = make(id, std::forward<CreateFn>(create_fn), std::forward<Args>(args)...);
		auto buffer_ptr = std::make_shared<std::vector<std::uint8_t>>(std::move(buffer));
		async_send_buffer(socket, buffer_ptr, header_size, handler);
	}
}
