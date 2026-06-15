#pragma once
#include "../network/socket.hpp"
#include "../serialisation/serialisation.hpp"
#include "request_def.hpp"

#include <span>

namespace sl::request
{
	void send_buffer(sl::socket& socket, std::span<const std::uint8_t> buffer, std::size_t header_size);

	[[nodiscard]] std::vector<std::uint8_t> make_header(request_id_t request_id, std::size_t body_size);

	[[nodiscard]] request_t make_from_body(request_id_t request_id, std::span<const std::uint8_t> body);

	template <class CreateFn, class... Args>
	[[nodiscard]] request_t make(const request_id_t request_id, CreateFn&& create_fn, Args&&... args)
	{
		const auto body = serialisation::serialise(std::forward<CreateFn>(create_fn), std::forward<Args>(args)...);
		return make_from_body(request_id, body);
	}

	template <class CreateFn, class... Args>
	void send(sl::socket& socket, const request_id_t request_id, CreateFn&& create_fn, Args&&... args)
	{
		const auto [header_size, buffer] = make(request_id, std::forward<CreateFn>(create_fn), std::forward<Args>(args)...);
		send_buffer(socket, buffer, header_size);
	}
}
