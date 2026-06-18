#include "connection.hpp"
#include "listener.hpp"
#include <serialisation/serialisation.hpp>
#include <response/response.hpp>
#include <router/router.hpp>
#include <endian/endian.hpp>
#include <schema/schema.hpp>

#include <schema/request_generated.h>
#include <schema/response_generated.h>

namespace sl
{
	connection::~connection()
	{
		socket_->close();
	}

	bool connection::handshake(const sl::socket::handshake_type type) const
	{
		return socket_->handshake(type);
	}

	void connection::async_handshake(const sl::socket::handshake_type type, const async_callback_t& handler) const
	{
		socket_->async_handshake(type, handler);
	}

	void connection::await_request()
	{
		read_request_header_size();
	}

	sl::socket& connection::socket() const noexcept
	{
		return *socket_;
	}

	void connection::close_self()
	{
		parent_listener_->remove_connection(this);
	}

	void connection::read_request_header_size()
	{
		auto header_size = std::make_shared<request::request_buffer_size_t>();

		socket_->async_read({reinterpret_cast<std::uint8_t*>(header_size.get()), sizeof(request::request_buffer_size_t)},
			[this, header_size](const bool is_valid)
			{
				if (is_valid)
				{
					read_request_header(endian::from_little(*header_size));
				}
				else
				{
					LOG_ERR("failed to read request header size");

					close_self();
				}
			}
		);
	}

	void connection::read_request_header(const std::size_t header_size)
	{
		auto header_buffer = std::make_shared<std::vector<std::uint8_t>>(header_size);

		socket_->async_read(*header_buffer,
			[this, header_buffer, header_size](const bool is_valid)
			{
				if (is_valid)
				{
					LOG_INFO("received request header ({})", header_size);

					if (serialisation::is_valid<RequestHeader>(*header_buffer))
					{
						const auto* request_header = serialisation::deserialise<RequestHeader>(*header_buffer);

						const request::request_id_t request_id = request_header->type();
						const std::size_t request_body_size = request_header->body_size();

						read_request_body(request_id, request_body_size);
					}
					else
					{
						LOG_ERR("request header is invalid");

						close_self();
					}
				}
				else
				{
					LOG_ERR("failed to read request header buffer from socket");

					close_self();
				}
			}
		);
	}

	void connection::read_request_body(const request::request_id_t request_id, const std::size_t body_size)
	{
		auto body_buffer = std::make_shared<std::vector<std::uint8_t>>(body_size);

		socket_->async_read(*body_buffer,
			[this, request_id, body_buffer](const bool is_valid)
			{
				if (is_valid)
				{
					LOG_INFO("received request buffer");

					handle_request(request_id, body_buffer);

					await_request();
				}
				else
				{
					LOG_ERR("failed to read request buffer from socket");

					close_self();
				}
			}
		);
	}

	namespace
	{
		void handle_valid_test_request(const std::shared_ptr<connection>& connection, const Client::TestRequest* const request_body)
		{
			LOG_INFO("test request key: 0x{:X}", request_body->key());

			constexpr std::uint64_t response_key = 0x56789;

			response::send(connection->socket(),
				Client::ResponseId_Test,
				[](const bool is_valid)
				{
					if (is_valid)
					{
						LOG_INFO("successfully sent response");
					}
					else
					{
						LOG_ERR("failed to send response");
					}
				},
				CREATION_WRAPPER(Client::CreateTestResponse), response_key);
		}

		constexpr message_info<Client::TestRequest, connection> test_request{Client::RequestId_Test, handle_valid_test_request};

		using router = message_router<test_request>;
	}

	void client_connection::handle_request(const request::request_id_t request_id, const std::shared_ptr<std::vector<std::uint8_t>> body_buffer)
	{
		if (!router::dispatch(request_id, shared_from_this(), *body_buffer))
		{
			LOG_ERR("unknown request type: {}", request_id);
		}
	}
}
