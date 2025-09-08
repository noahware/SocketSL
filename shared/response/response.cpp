#include "response.hpp"
#include "../request/request_def.hpp"
#include <schema/response_generated.h>
#include <schema/schema.hpp>

#include "../endian/endian.hpp"

void response::async_send_buffer(socket_t& socket, const std::shared_ptr<std::vector<std::uint8_t>>& buffer, const async_callback_t& handler)
{
	const request::request_buffer_size_t buffer_size = buffer->size();

	const auto buffer_size_begin = reinterpret_cast<const std::uint8_t*>(&buffer_size);

	buffer->insert(buffer->begin(), buffer_size_begin, buffer_size_begin + sizeof(buffer_size));

	socket.async_write(buffer->data(), buffer->size(),
		[handler, buffer](const std::uint8_t is_valid)
		{
			(void)buffer;

			handler(is_valid);
		}
	);
}

void response::read_buffer(socket_t& socket, std::vector<std::uint8_t>& buffer)
{
	request::request_buffer_size_t little_endian_buffer_size = 0;

	socket.read(little_endian_buffer_size);

	const request::request_buffer_size_t buffer_size = endian::from_little(little_endian_buffer_size);

	buffer.resize(buffer_size);

	socket.read(buffer.data(), buffer_size);
}

std::vector<std::uint8_t> response::construct::make_test_response(std::uint64_t key)
{
	return serialisation::serialise(CREATION_WRAPPER(Client::CreateTestResponse), key);
}
