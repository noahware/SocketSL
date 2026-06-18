#include "session.hpp"

#include <endian/endian.hpp>
#include <log/log.hpp>

namespace sl
{
	session::session(std::unique_ptr<sl::socket> socket) noexcept
		: socket_(std::move(socket)) {}

	session::~session()
	{
		socket_->close();
	}

	sl::socket& session::socket() const noexcept
	{
		return *socket_;
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
		stop();
	}

	void session::read_frame_size()
	{
		auto frame_size = std::make_shared<frame_size_t>();

		socket_->async_read({reinterpret_cast<std::uint8_t*>(frame_size.get()), sizeof(frame_size_t)},
			[self = shared_from_this(), frame_size](const bool is_valid)
			{
				if (is_valid)
				{
					self->read_header(endian::from_little(*frame_size));
				}
				else
				{
					LOG_ERR("failed to read frame size");

					self->on_error();
				}
			}
		);
	}

	void session::read_header(const std::size_t header_size)
	{
		auto header_buffer = std::make_shared<std::vector<std::uint8_t>>(header_size);

		socket_->async_read(*header_buffer,
			[self = shared_from_this(), header_buffer](const bool is_valid)
			{
				if (!is_valid)
				{
					LOG_ERR("failed to read message header");

					self->on_error();

					return;
				}

				message_id_t type = 0;
				std::size_t body_size = 0;

				if (!self->parse_header(*header_buffer, type, body_size))
				{
					LOG_ERR("message header is invalid");

					self->on_error();

					return;
				}

				self->read_body(type, body_size);
			}
		);
	}

	void session::read_body(const message_id_t type, const std::size_t body_size)
	{
		auto body_buffer = std::make_shared<std::vector<std::uint8_t>>(body_size);

		socket_->async_read(*body_buffer,
			[self = shared_from_this(), type, body_buffer](const bool is_valid)
			{
				if (!is_valid)
				{
					LOG_ERR("failed to read message body");

					self->on_error();

					return;
				}

				self->on_message(type, body_buffer);

				self->read_frame_size();
			}
		);
	}
}
