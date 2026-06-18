#include "client_session.hpp"

#include <endian/endian.hpp>
#include <serialisation/serialisation.hpp>
#include <log/log.hpp>

#include <schema/response_generated.h>

namespace sl
{
	client_session::client_session(std::unique_ptr<sl::socket> socket) noexcept
		: socket_(std::move(socket)) {}

	client_session::~client_session()
	{
		socket_->close();
	}

	bool client_session::connect(const std::string_view host, const std::string_view service)
	{
		return socket_->connect(host, service);
	}

	bool client_session::handshake()
	{
		return socket_->handshake(sl::socket::handshake_type::client);
	}

	void client_session::start()
	{
		read_response_header_size();
	}

	void client_session::stop()
	{
		socket_->close();
	}

	sl::socket& client_session::socket() noexcept
	{
		return *socket_;
	}

	void client_session::read_response_header_size()
	{
		auto header_size = std::make_shared<response::response_buffer_size_t>();

		socket_->async_read({reinterpret_cast<std::uint8_t*>(header_size.get()), sizeof(response::response_buffer_size_t)},
			[self = shared_from_this(), header_size](const bool is_valid)
			{
				if (is_valid)
				{
					self->read_response_header(endian::from_little(*header_size));
				}
				else
				{
					LOG_ERR("failed to read response header size");

					self->stop();
				}
			}
		);
	}

	void client_session::read_response_header(const std::size_t header_size)
	{
		auto header_buffer = std::make_shared<std::vector<std::uint8_t>>(header_size);

		socket_->async_read(*header_buffer,
			[self = shared_from_this(), header_buffer](const bool is_valid)
			{
				if (is_valid)
				{
					if (serialisation::is_valid<ResponseHeader>(*header_buffer))
					{
						const auto* response_header = serialisation::deserialise<ResponseHeader>(*header_buffer);

						const response::response_id_t response_id = response_header->type();
						const std::size_t body_size = response_header->body_size();

						self->read_response_body(response_id, body_size);
					}
					else
					{
						LOG_ERR("response header is invalid");

						self->stop();
					}
				}
				else
				{
					LOG_ERR("failed to read response header");

					self->stop();
				}
			}
		);
	}

	void client_session::read_response_body(const response::response_id_t id, const std::size_t body_size)
	{
		auto body_buffer = std::make_shared<std::vector<std::uint8_t>>(body_size);

		socket_->async_read(*body_buffer,
			[self = shared_from_this(), id, body_buffer](const bool is_valid)
			{
				if (is_valid)
				{
					self->handle_response(id, body_buffer);

					self->read_response_header_size();
				}
				else
				{
					LOG_ERR("failed to read response body");

					self->stop();
				}
			}
		);
	}
}
