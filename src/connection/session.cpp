#include "session.hpp"
#include "session_manager.hpp"

#include <serialisation/serialisation.hpp>
#include <endian/endian.hpp>

#include <schema/message_generated.h>

namespace sl
{
	session::session(std::unique_ptr<sl::socket> socket, std::shared_ptr<session_manager> manager) noexcept
		:	socket_(std::move(socket)),
			manager_(std::move(manager)) {}

	session::~session()
	{
		socket_->close();
	}

	sl::socket& session::socket() const noexcept
	{
		return *socket_;
	}

	bool session::connect(const std::string_view host, const std::string_view service) const
	{
		return socket_->connect(host, service);
	}

	void session::async_connect(const std::string_view host, const std::string_view service, const async_callback_t& handler) const
	{
		socket_->async_connect(host, service, handler);
	}

	bool session::handshake(const sl::socket::handshake_type type) const
	{
		return socket_->handshake(type);
	}

	void session::async_handshake(const sl::socket::handshake_type type, const async_callback_t& handler) const
	{
		socket_->async_handshake(type, handler);
	}

	void session::start()
	{
		read_frame_size();
	}

	void session::stop()
	{
		socket_->close();
	}

	void session::on_error()
	{
		if (manager_)
		{
			manager_->remove_session(this);
		}
		else
		{
			stop();
		}
	}

	bool session::parse_header(const std::span<const std::uint8_t> header, message_id_t& out_type, std::size_t& out_body_size)
	{
		if (!serialisation::is_valid<MessageHeader>(header))
		{
			return false;
		}

		const auto* message_header = serialisation::deserialise<MessageHeader>(header);

		out_type = message_header->type();
		out_body_size = message_header->body_size();

		return true;
	}

	void session::read_frame_size()
	{
		const auto frame_size = std::make_shared<frame_size_t>();

		socket_->async_read({reinterpret_cast<std::uint8_t*>(frame_size.get()), sizeof(frame_size_t)},
			[self = shared_from_this(), frame_size](const bool is_valid)
			{
				if (is_valid)
				{
					self->read_header(endian::from_little(*frame_size));
				}
				else
				{
					self->on_error();
				}
			}
		);
	}

	void session::read_header(const std::size_t header_size)
	{
		const auto header_buffer = std::make_shared<std::vector<std::uint8_t>>(header_size);

		socket_->async_read(*header_buffer,
			[self = shared_from_this(), header_buffer](const bool is_valid)
			{
				if (!is_valid)
				{
					self->on_error();

					return;
				}

				message_id_t type = 0;
				std::size_t body_size = 0;

				if (!parse_header(*header_buffer, type, body_size))
				{
					self->on_error();

					return;
				}

				self->read_body(type, body_size);
			}
		);
	}

	void session::read_body(const message_id_t type, const std::size_t body_size)
	{
		const auto body_buffer = std::make_shared<std::vector<std::uint8_t>>(body_size);

		socket_->async_read(*body_buffer,
			[self = shared_from_this(), type, body_buffer](const bool is_valid)
			{
				if (!is_valid)
				{
					self->on_error();

					return;
				}

				self->handle_message(type, body_buffer);

				self->read_frame_size();
			}
		);
	}
}
