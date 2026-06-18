#include "connection.hpp"
#include "listener.hpp"

#include <serialisation/serialisation.hpp>

#include <schema/request_generated.h>

namespace sl
{
	bool connection::handshake(const sl::socket::handshake_type type) const
	{
		return socket_->handshake(type);
	}

	void connection::async_handshake(const sl::socket::handshake_type type, const async_callback_t& handler) const
	{
		socket_->async_handshake(type, handler);
	}

	void connection::close_self()
	{
		parent_listener_->remove_connection(this);
	}

	bool connection::parse_header(const std::span<const std::uint8_t> header, message_id_t& out_type, std::size_t& out_body_size) const
	{
		if (!serialisation::is_valid<RequestHeader>(header))
		{
			return false;
		}

		const auto* request_header = serialisation::deserialise<RequestHeader>(header);

		out_type = request_header->type();
		out_body_size = request_header->body_size();

		return true;
	}

	void connection::on_message(const message_id_t type, body_buffer_t body)
	{
		handle_request(type, std::move(body));
	}

	void connection::on_error()
	{
		close_self();
	}
}
