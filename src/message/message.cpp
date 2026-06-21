#include "message.hpp"

#include "../endian/endian.hpp"

#include <schema/message_generated.h>
#include <schema/system_generated.h>

#include <cstring>
#include <optional>

namespace sl::msg
{
	std::vector<std::uint8_t> make_header(const message_id_t id, const std::size_t body_size, const bool is_system)
	{
		return serialisation::serialise(serialisation::lift<CreateMessageHeader>(), id, body_size, is_system);
	}

	std::vector<std::uint8_t> make_frame_header(const message_id_t id, const std::size_t body_size, const bool is_system)
	{
		const std::vector<std::uint8_t> header_fb = make_header(id, body_size, is_system);
		const frame_size_t wire_size = endian::to_little(static_cast<frame_size_t>(header_fb.size()));

		std::vector<std::uint8_t> result(sizeof(wire_size) + header_fb.size());
		std::memcpy(result.data(), &wire_size, sizeof(wire_size));
		std::copy(header_fb.begin(), header_fb.end(), result.data() + sizeof(wire_size));
		return result;
	}

	message_t make_from_body(const message_id_t id, const std::span<const std::uint8_t> body, const bool is_system)
	{
		message_t msg;
		msg.header = make_frame_header(id, body.size(), is_system);
		msg.body.assign(body.begin(), body.end());
		return msg;
	}

	bool send_buffer(sl::socket& socket, const std::span<const std::uint8_t> header, const std::span<const std::uint8_t> body)
	{
		if (!socket.write(header))
		{
			return false;
		}

		if (body.empty())
		{
			return true;
		}

		return socket.write(body);
	}

	void async_send_buffer(sl::socket& socket, const std::span<const std::uint8_t> header, const std::span<const std::uint8_t> body, const async_callback_t& handler)
	{
		socket.async_write(header, body, handler);
	}

	std::optional<message_id_t> recv_buffer(sl::socket& socket, std::vector<std::uint8_t>& body_buffer)
	{
		const std::size_t max_message_size = socket.max_message_size();

		for (;;)
		{
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
			const bool is_system = header->is_system();

			if (max_message_size != 0 && body_size > max_message_size)
			{
				return std::nullopt;
			}

			body_buffer.resize(body_size);
			if (!socket.read(body_buffer))
			{
				return std::nullopt;
			}

			if (!is_system)
			{
				return id;
			}

			if (id == System::MessageId_HbPing)
			{
				(void)send<System::CreateHbPongRequest, true>(socket, System::MessageId_HbPong);
			}
		}
	}

	bool send_ping(sl::socket& socket)
	{
		return send<System::CreateHbPingRequest, true>(socket, System::MessageId_HbPing);
	}
}
