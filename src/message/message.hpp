#pragma once
#include "../network/socket.hpp"
#include "../serialisation/serialisation.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
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

	// a completion handler is any callable invocable as void() or void(bool is_valid)
	template <class T>
	concept completion_handler = std::is_invocable_v<T> || std::is_invocable_v<T, bool>;

	[[nodiscard]] std::vector<std::uint8_t> make_header(message_id_t id, std::size_t body_size, bool is_system = false);

	[[nodiscard]] message_t make_from_body(message_id_t id, std::span<const std::uint8_t> body, bool is_system = false);

	// synchronous send: [u64 frame size][header][body]; returns whether both writes succeeded
	bool send_buffer(sl::socket& socket, std::span<const std::uint8_t> buffer, std::size_t header_size);

	// asynchronous send
	void async_send_buffer(sl::socket& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, std::size_t header_size, const async_callback_t& handler);

	// synchronous receive of one framed message; returns the message id, or nullopt on read/parse failure
	[[nodiscard]] std::optional<message_id_t> recv_buffer(sl::socket& socket, std::vector<std::uint8_t>& body_buffer);

	namespace detail
	{
		template <class... Args>
		inline constexpr bool leading_completion_handler = false;

		template <class First, class... Rest>
		inline constexpr bool leading_completion_handler<First, Rest...> = completion_handler<First>;

		// adapt a void() or void(bool) handler into the uniform async_callback_t
		template <completion_handler Handler>
		[[nodiscard]] async_callback_t adapt(Handler handler)
		{
			if constexpr (std::is_invocable_v<Handler, bool>)
			{
				return async_callback_t(std::move(handler));
			}
			else
			{
				return [handler = std::move(handler)](bool) { handler(); };
			}
		}
	}

	template <auto CreateFn, bool is_system = false, class... Args>
	[[nodiscard]] message_t make(const message_id_t id, Args&&... args)
	{
		const auto body = serialisation::serialise(serialisation::lift<CreateFn>(), std::forward<Args>(args)...);
		return make_from_body(id, body, is_system);
	}

	// synchronous send; returns whether the message was written in full
	template <auto CreateFn, bool is_system = false, class... Args>
	bool send(sl::socket& socket, const message_id_t id, Args&&... args)
	{
		const auto [header_size, buffer] = make<CreateFn, is_system>(id, std::forward<Args>(args)...);
		return send_buffer(socket, buffer, header_size);
	}

	// asynchronous send with a completion handler -- void() or void(bool is_valid)
	template <auto CreateFn, bool is_system = false, completion_handler Handler, class... Args>
	void async_send(sl::socket& socket, const message_id_t id, Handler handler, Args&&... args)
	{
		auto [header_size, buffer] = make<CreateFn, is_system>(id, std::forward<Args>(args)...);
		auto buffer_ptr = std::make_shared<std::vector<std::uint8_t>>(std::move(buffer));
		async_send_buffer(socket, buffer_ptr, header_size, detail::adapt(std::move(handler)));
	}

	// asynchronous send, fire and forget (no completion handler)
	template <auto CreateFn, bool is_system = false, class... Args>
		requires (!detail::leading_completion_handler<Args...>)
	void async_send(sl::socket& socket, const message_id_t id, Args&&... args)
	{
		auto [header_size, buffer] = make<CreateFn, is_system>(id, std::forward<Args>(args)...);
		auto buffer_ptr = std::make_shared<std::vector<std::uint8_t>>(std::move(buffer));
		async_send_buffer(socket, buffer_ptr, header_size, async_callback_t{});
	}

	// synchronous receive of one framed message, then deserialise;
	// returns nullptr on read failure or an invalid body buffer
	template <class T>
	[[nodiscard]] const T* recv(sl::socket& socket, std::vector<std::uint8_t>& buffer)
	{
		if (!recv_buffer(socket, buffer))
		{
			return nullptr;
		}

		if (!serialisation::is_valid<T>(buffer))
		{
			return nullptr;
		}

		return serialisation::deserialise<T>(buffer);
	}
}
