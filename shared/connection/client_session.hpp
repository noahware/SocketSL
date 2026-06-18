#pragma once
#include "session.hpp"

#include <network/socket.hpp>
#include <response/response.hpp>

#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace sl
{
	// must be created as a shared_ptr (session uses shared_from_this in its async chain)
	class client_session : public session
	{
	public:
		using session::session;

		[[nodiscard]] bool connect(std::string_view host, std::string_view service);
		[[nodiscard]] bool handshake();

	protected:
		virtual void handle_response(response::response_id_t id, std::shared_ptr<std::vector<std::uint8_t>> body) = 0;

		[[nodiscard]] bool parse_header(std::span<const std::uint8_t> header, message_id_t& out_type, std::size_t& out_body_size) const override;
		void on_message(message_id_t type, body_buffer_t body) override;
	};
}
