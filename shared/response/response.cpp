#include "response.hpp"
#include "../request/request_def.hpp"
#include <schema/response_generated.h>
#include <schema/schema.hpp>

#include "../endian/endian.hpp"

namespace sl::response
{
	void async_send_buffer(sl::socket& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, const async_callback_t& handler)
	{
		const request::request_buffer_size_t wire_buffer_size = endian::to_little(static_cast<request::request_buffer_size_t>(buffer->size()));

		const auto wire_buffer_size_bytes = reinterpret_cast<const std::uint8_t*>(&wire_buffer_size);

		buffer->insert(buffer->begin(), wire_buffer_size_bytes, wire_buffer_size_bytes + sizeof(wire_buffer_size));

		socket.async_write(buffer->data(), buffer->size(),
			[handler, buffer](const bool is_valid)
			{
				(void)buffer;

				handler(is_valid);
			}
		);
	}

	void read_buffer(sl::socket& socket, std::vector<std::uint8_t>& buffer)
	{
		request::request_buffer_size_t little_endian_buffer_size = 0;

		(void)socket.read(little_endian_buffer_size);

		const std::size_t buffer_size = endian::from_little(little_endian_buffer_size);

		buffer.resize(buffer_size);

		(void)socket.read(buffer.data(), buffer_size);
	}

	std::vector<std::uint8_t> make_test_response(const std::uint64_t key)
	{
		return serialisation::serialise(CREATION_WRAPPER(Client::CreateTestResponse), key);
	}
}
