#include "message.hpp"

#include "../endian/endian.hpp"

#include <schema/message_generated.h>

#include <optional>

namespace sl::msg
{
	namespace
	{
		void add_header(const message_id_t id, message_t& message, const bool is_system)
		{
			std::vector<std::uint8_t>& buffer = message.buffer;

			const std::vector<std::uint8_t> header = make_header(id, buffer.size(), is_system);

			buffer.insert(buffer.begin(), header.begin(), header.end());

			message.header_size = header.size();
		}
	}

	std::vector<std::uint8_t> make_header(const message_id_t id, const std::size_t body_size, const bool is_system)
	{
		return serialisation::serialise(serialisation::lift<CreateMessageHeader>(), id, body_size, is_system);
	}

	message_t make_from_body(const message_id_t id, const std::span<const std::uint8_t> body, const bool is_system)
	{
		message_t message = { .header_size = 0, .buffer = {body.begin(), body.end()} };

		add_header(id, message, is_system);

		return message;
	}

	bool send_buffer(sl::socket& socket, const std::span<const std::uint8_t> buffer, const std::size_t header_size)
	{
		if (!socket.write<frame_size_t>(endian::to_little(static_cast<frame_size_t>(header_size))))
		{
			return false;
		}

		return socket.write(buffer);
	}

	void async_send_buffer(sl::socket& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, const std::size_t header_size, const async_callback_t& handler)
	{
		const frame_size_t wire_header_size = endian::to_little(static_cast<frame_size_t>(header_size));

		const auto wire_header_size_bytes = reinterpret_cast<const std::uint8_t*>(&wire_header_size);

		buffer->insert(buffer->begin(), wire_header_size_bytes, wire_header_size_bytes + sizeof(wire_header_size));

		socket.async_write(*buffer,
			[handler, buffer](const bool is_valid)
			{
				if (handler)
				{
					handler(is_valid);
				}
			}
		);
	}

	std::optional<message_id_t> recv_buffer(sl::socket& socket, std::vector<std::uint8_t>& body_buffer)
	{
		const std::size_t max_message_size = socket.max_message_size();

		frame_size_t le_header_size = 0;
		if (!socket.read(le_header_size))
		{
			return std::nullopt;
		}

		const std::size_t header_size = endian::from_little(le_header_size);

		if (max_message_size != 0 && header_size > max_message_size)
		{
			return std::nullopt;
		}

		std::vector<std::uint8_t> header_buffer(header_size);
		if (!socket.read(header_buffer))
		{
			return std::nullopt;
		}

		if (!serialisation::is_valid<MessageHeader>(header_buffer))
		{
			return std::nullopt;
		}

		const auto* header = serialisation::deserialise<MessageHeader>(header_buffer);
		const message_id_t id = header->type();
		const std::size_t body_size = header->body_size();

		if (max_message_size != 0 && body_size > max_message_size)
		{
			return std::nullopt;
		}

		body_buffer.resize(body_size);
		if (!socket.read(body_buffer))
		{
			return std::nullopt;
		}

		return id;
	}
}
