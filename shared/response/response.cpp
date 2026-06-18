#include "response.hpp"

#include "../serialisation/serialisation.hpp"
#include "../schema/schema.hpp"
#include "../endian/endian.hpp"

#include <schema/response_generated.h>

namespace sl::response
{
	namespace
	{
		void add_header_to_response(const response_id_t id, response_t& response)
		{
			std::vector<std::uint8_t>& response_buffer = response.buffer;

			const std::vector<std::uint8_t> response_header = make_header(id, response_buffer.size());

			response_buffer.insert(response_buffer.begin(), response_header.begin(), response_header.end());

			response.header_size = response_header.size();
		}
	}

	std::vector<std::uint8_t> make_header(const response_id_t id, const std::size_t body_size)
	{
		return serialisation::serialise(CREATION_WRAPPER(CreateResponseHeader), id, body_size);
	}

	response_t make_from_body(const response_id_t id, const std::span<const std::uint8_t> body)
	{
		response_t response = { .header_size = 0, .buffer = {body.begin(), body.end()} };

		add_header_to_response(id, response);

		return response;
	}

	void async_send_buffer(sl::socket& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, const std::size_t header_size, const async_callback_t& handler)
	{
		const response_buffer_size_t wire_header_size = endian::to_little(static_cast<response_buffer_size_t>(header_size));

		const auto wire_header_size_bytes = reinterpret_cast<const std::uint8_t*>(&wire_header_size);

		buffer->insert(buffer->begin(), wire_header_size_bytes, wire_header_size_bytes + sizeof(wire_header_size));

		socket.async_write(*buffer,
			[handler, buffer](const bool is_valid)
			{
				handler(is_valid);
			}
		);
	}

	response_id_t read_buffer(sl::socket& socket, std::vector<std::uint8_t>& body_buffer)
	{
		response_buffer_size_t le_header_size = 0;
		(void)socket.read(le_header_size);

		const std::size_t header_size = endian::from_little(le_header_size);

		std::vector<std::uint8_t> header_buffer(header_size);
		(void)socket.read(header_buffer);

		const auto* header = serialisation::deserialise<ResponseHeader>(header_buffer);
		const response_id_t id = header->type();
		const std::size_t body_size = header->body_size();

		body_buffer.resize(body_size);
		(void)socket.read(body_buffer);

		return id;
	}
}
