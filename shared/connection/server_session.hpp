#pragma once
#include "session.hpp"

#include <network/socket.hpp>
#include <request/request_def.hpp>

#include <memory>
#include <span>
#include <vector>

namespace sl
{
	class server;

	class server_session : public session
	{
	public:
		explicit server_session(std::unique_ptr<sl::socket> socket, std::shared_ptr<server> parent_listener) noexcept
				:	session(std::move(socket)),
					parent_listener_(std::move(parent_listener)) {}

		[[nodiscard]] bool handshake(sl::socket::handshake_type type) const;
		void async_handshake(sl::socket::handshake_type type, const async_callback_t& handler) const;

	protected:
		virtual void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) = 0;

		void close_self();

		[[nodiscard]] bool parse_header(std::span<const std::uint8_t> header, message_id_t& out_type, std::size_t& out_body_size) const override;
		void on_message(message_id_t type, body_buffer_t body) override;
		void on_error() override;

		std::shared_ptr<server> parent_listener_;
	};
}
