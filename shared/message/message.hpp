#pragma once
#include "../network/socket.hpp"
#include "../serialisation/serialisation.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace sl::msg
{
	using message_id_t = std::uint8_t;
	using frame_size_t = std::uint64_t;

	struct message_t
	{
		std::size_t header_size = 0;
		std::vector<std::uint8_t> buffer = {};
	};

	[[nodiscard]] std::vector<std::uint8_t> make_header(message_id_t id, std::size_t body_size);

	[[nodiscard]] message_t make_from_body(message_id_t id, std::span<const std::uint8_t> body);

	// synchronous send: [u64 frame size][header][body]
	void send_buffer(sl::socket& socket, std::span<const std::uint8_t> buffer, std::size_t header_size);

	// asynchronous send
	void async_send_buffer(sl::socket& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, std::size_t header_size, const async_callback_t& handler);

	// synchronous receive of one framed message; returns the message id
	message_id_t recv_buffer(sl::socket& socket, std::vector<std::uint8_t>& body_buffer);

	template <class CreateFn, class... Args>
	[[nodiscard]] message_t make(const message_id_t id, CreateFn&& create_fn, Args&&... args)
	{
		const auto body = serialisation::serialise(std::forward<CreateFn>(create_fn), std::forward<Args>(args)...);
		return make_from_body(id, body);
	}

	// synchronous send
	template <class CreateFn, class... Args>
	void send(sl::socket& socket, const message_id_t id, CreateFn&& create_fn, Args&&... args)
	{
		const auto [header_size, buffer] = make(id, std::forward<CreateFn>(create_fn), std::forward<Args>(args)...);
		send_buffer(socket, buffer, header_size);
	}

	// asynchronous send
	template <class CreateFn, class... Args>
	void async_send(sl::socket& socket, const message_id_t id, const async_callback_t& handler, CreateFn&& create_fn, Args&&... args)
	{
		auto [header_size, buffer] = make(id, std::forward<CreateFn>(create_fn), std::forward<Args>(args)...);
		auto buffer_ptr = std::make_shared<std::vector<std::uint8_t>>(std::move(buffer));
		async_send_buffer(socket, buffer_ptr, header_size, handler);
	}

	// synchronous receive of one framed message, then deserialise
	template <class T>
	[[nodiscard]] const T* recv(sl::socket& socket, std::vector<std::uint8_t>& buffer)
	{
		(void)recv_buffer(socket, buffer);

		return serialisation::deserialise<T>(buffer);
	}
}
