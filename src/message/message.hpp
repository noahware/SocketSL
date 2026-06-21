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
		std::vector<std::uint8_t> header = {};
		std::vector<std::uint8_t> body = {};
	};

	// a completion handler is any callable invocable as void() or void(bool is_valid)
	template <class T>
	concept completion_handler = std::is_invocable_v<T> || std::is_invocable_v<T, bool>;

	// builds just the MessageHeader flatbuffer (no frame-size prefix)
	[[nodiscard]] std::vector<std::uint8_t> make_header(message_id_t id, std::size_t body_size, bool is_system = false);

	// builds the complete wire prefix: [u64 frame_size LE] + [MessageHeader flatbuffer]
	[[nodiscard]] std::vector<std::uint8_t> make_frame_header(message_id_t id, std::size_t body_size, bool is_system = false);

	[[nodiscard]] message_t make_from_body(message_id_t id, std::span<const std::uint8_t> body, bool is_system = false);

	// synchronous send: writes header then body; returns whether both writes succeeded
	bool send_buffer(sl::socket& socket, std::span<const std::uint8_t> header, std::span<const std::uint8_t> body);

	// asynchronous scatter-gather send: header and body written atomically
	void async_send_buffer(sl::socket& socket, std::span<const std::uint8_t> header, std::span<const std::uint8_t> body, const async_callback_t& handler);

	// synchronous receive of one framed message; returns the message id, or nullopt on read/parse failure
	[[nodiscard]] std::optional<message_id_t> recv_buffer(sl::socket& socket, std::vector<std::uint8_t>& body_buffer);

	// send a heartbeat ping (a system control frame); synchronous, returns whether it was written in full.
	// the matching pong is sent automatically by recv / the async session when a ping is received.
	bool send_ping(sl::socket& socket);

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
		auto body = serialisation::serialise(serialisation::lift<CreateFn>(), std::forward<Args>(args)...);

		message_t msg;
		msg.header = make_frame_header(id, body.size(), is_system);
		msg.body = std::move(body);
		return msg;
	}

	// synchronous send; returns whether the message was written in full
	template <auto CreateFn, bool is_system = false, class... Args>
	bool send(sl::socket& socket, const message_id_t id, Args&&... args)
	{
		const auto [header, body] = make<CreateFn, is_system>(id, std::forward<Args>(args)...);
		return send_buffer(socket, header, body);
	}

	// asynchronous send with a completion handler -- void() or void(bool is_valid)
	template <auto CreateFn, bool is_system = false, completion_handler Handler, class... Args>
	void async_send(sl::socket& socket, const message_id_t id, Handler handler, Args&&... args)
	{
		auto [header, body] = make<CreateFn, is_system>(id, std::forward<Args>(args)...);
		auto owned = std::make_shared<message_t>(std::move(header), std::move(body));
		async_send_buffer(socket, owned->header, owned->body,
			[handler = detail::adapt(std::move(handler)), owned](const bool is_valid)
			{
				handler(is_valid);
			});
	}

	// asynchronous send, fire and forget (no completion handler)
	template <auto CreateFn, bool is_system = false, class... Args>
		requires (!detail::leading_completion_handler<Args...>)
	void async_send(sl::socket& socket, const message_id_t id, Args&&... args)
	{
		auto [header, body] = make<CreateFn, is_system>(id, std::forward<Args>(args)...);
		auto owned = std::make_shared<message_t>(std::move(header), std::move(body));
		async_send_buffer(socket, owned->header, owned->body,
			[owned](bool) {});
	}

	// zero-copy synchronous send: the body is written directly without copying
	template <bool is_system = false>
	bool send_view(sl::socket& socket, const message_id_t id, const std::span<const std::uint8_t> body)
	{
		const auto header = make_frame_header(id, body.size(), is_system);
		return send_buffer(socket, header, body);
	}

	// zero-copy asynchronous send with completion handler: the body must remain valid
	// until the handler fires (the handler's invocation is the "release" signal)
	template <bool is_system = false, completion_handler Handler>
	void async_send_view(sl::socket& socket, const message_id_t id, Handler handler, const std::span<const std::uint8_t> body)
	{
		auto header = std::make_shared<std::vector<std::uint8_t>>(make_frame_header(id, body.size(), is_system));
		async_send_buffer(socket, *header, body,
			[handler = detail::adapt(std::move(handler)), header](const bool is_valid)
			{
				handler(is_valid);
			});
	}

	// zero-copy asynchronous send, fire and forget: the body must remain valid until the
	// write completes (the internal completion of the write is the implicit release)
	template <bool is_system = false>
	void async_send_view(sl::socket& socket, const message_id_t id, const std::span<const std::uint8_t> body)
	{
		auto header = std::make_shared<std::vector<std::uint8_t>>(make_frame_header(id, body.size(), is_system));
		async_send_buffer(socket, *header, body,
			[header](bool) {});
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
