#include "client_session.hpp"

#include <serialisation/serialisation.hpp>

#include <schema/response_generated.h>

namespace sl
{
	bool client_session::connect(const std::string_view host, const std::string_view service)
	{
		return socket_->connect(host, service);
	}

	bool client_session::handshake()
	{
		return socket_->handshake(sl::socket::handshake_type::client);
	}

	bool client_session::parse_header(const std::span<const std::uint8_t> header, message_id_t& out_type, std::size_t& out_body_size) const
	{
		if (!serialisation::is_valid<ResponseHeader>(header))
		{
			return false;
		}

		const auto* response_header = serialisation::deserialise<ResponseHeader>(header);

		out_type = response_header->type();
		out_body_size = response_header->body_size();

		return true;
	}

	void client_session::on_message(const message_id_t type, body_buffer_t body)
	{
		handle_response(type, std::move(body));
	}
}
