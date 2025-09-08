#include "connection.hpp"
#include "listener.hpp"
#include <serialisation/serialisation.hpp>
#include <response/response.hpp>
#include <endian/endian.hpp>

#include <schema/request_generated.h>

connection_t::~connection_t()
{
	socket_->close();
}

std::uint8_t connection_t::handshake(const socket_t::handshake_type_t type) const
{
	return socket_->handshake(type);
}

void connection_t::async_handshake(const socket_t::handshake_type_t type, const async_callback_t& handler) const
{
	socket_->async_handshake(type, handler);
}

void connection_t::await_request()
{
	read_request_header_size();
}

void connection_t::close_self()
{
	parent_listener_->remove_connection(this);
}

void connection_t::read_request_header_size()
{
	auto header_size = std::make_shared<request::request_buffer_size_t>();

	socket_->async_read(header_size.get(), sizeof(request::request_buffer_size_t),
		[this, header_size](const std::uint8_t is_valid)
		{
			if (is_valid)
			{
				read_request_header(endian::from_little(*header_size));
			}
			else
			{
				spdlog::error("failed to read request header size");

				close_self();
			}
		}
	);
}

void connection_t::read_request_header(const request::request_buffer_size_t header_size)
{
	auto header_buffer = std::make_shared<std::vector<std::uint8_t>>(header_size);

	socket_->async_read(header_buffer->data(), header_size,
		[this, header_buffer, header_size](const std::uint8_t is_valid)
		{
			if (is_valid)
			{
				spdlog::info("received request header ({})", header_size);

				if (serialisation::is_valid<RequestHeader>(*header_buffer))
				{
					const auto* request_header = serialisation::deserialise<RequestHeader>(*header_buffer);

					const request::request_id_t request_id = request_header->type();
					const std::uint64_t request_body_size = request_header->body_size();

					read_request_body(request_id, request_body_size);
				}
				else
				{
					spdlog::error("request header is invalid");

					close_self();
				}
			}
			else
			{
				spdlog::error("failed to read request header buffer from socket");

				close_self();
			}
		}
	);
}

void connection_t::read_request_body(const request::request_id_t request_id, const request::request_buffer_size_t body_size)
{
	auto body_buffer = std::make_shared<std::vector<std::uint8_t>>(body_size);

	socket_->async_read(body_buffer->data(), body_size,
		[this, request_id, body_buffer](const std::uint8_t is_valid)
		{
			if (is_valid)
			{
				spdlog::info("received request buffer");

				handle_request(request_id, body_buffer);

				await_request();
			}
			else
			{
				spdlog::error("failed to read request buffer from socket");

				close_self();
			}
		}
	);
}

void handle_valid_test_request(socket_t& socket, const Client::TestRequest* const request_body)
{
	spdlog::info("test request key: 0x{:X}", request_body->key());

	constexpr std::uint64_t response_key = 0x45678;
	const auto response_body = std::make_shared<std::vector<std::uint8_t>>(response::construct::make_test_response(response_key));

	response::async_send_buffer(socket, response_body,
		[response_key](const std::uint8_t is_valid)
		{
			if (is_valid)
			{
				spdlog::info("successfully sent test response (key: 0x{:X})", response_key);
			}
			else
			{
				spdlog::error("failed to send test response");
			}
		}
	);
}

void client_connection_t::handle_request(const request::request_id_t request_id, const std::shared_ptr<std::vector<std::uint8_t>> body_buffer)
{
	if (request_id == Client::RequestId_Test)
	{
		if (serialisation::is_valid<Client::TestRequest>(*body_buffer))
		{
			const auto* request_body = serialisation::deserialise<Client::TestRequest>(*body_buffer);

			handle_valid_test_request(*socket_, request_body);
		}
		else
		{
			spdlog::error("failed to verify test request body validity");
		}
	}
	else
	{
		spdlog::error("unknown request type");
	}
}
