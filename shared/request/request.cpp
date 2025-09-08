#include "request.hpp"

#include "../serialisation/serialisation.hpp"
#include "../schema/schema.hpp"
#include <schema/request_generated.h>

#include "../endian/endian.hpp"

void request::send_buffer(socket_t& socket, const void* const buffer, const request_buffer_size_t total_buffer_size, const request_buffer_size_t header_size)
{
	socket.write<request_buffer_size_t>(endian::to_little(header_size));

	socket.write(buffer, total_buffer_size);
}

void request::send_buffer(socket_t& socket, const std::vector<std::uint8_t>& buffer, const request_buffer_size_t header_size)
{
	send_buffer(socket, buffer.data(), buffer.size(), header_size);
}

std::vector<std::uint8_t> request::construct::make_request_header(const request_id_t request_id, const std::uint64_t body_size)
{
	return serialisation::serialise(CREATION_WRAPPER(CreateRequestHeader), request_id, body_size);
}

static void add_header_to_request(const request::request_id_t request_id, request::request_t& request)
{
	std::vector<std::uint8_t>& request_buffer = request.buffer;

	const std::vector<std::uint8_t> request_header = request::construct::make_request_header(request_id, request_buffer.size());

	request_buffer.insert(request_buffer.begin(), request_header.begin(), request_header.end());

	request.header_size = request_header.size();
}

static request::request_t make_request_from_body(const request::request_id_t request_id, const std::vector<std::uint8_t>& request_body)
{
	request::request_t request = { .header_size = 0, .buffer = request_body };

	add_header_to_request(request_id, request);

	return request;
}

request::request_t request::construct::make_test_request(const std::uint64_t key)
{
	const std::vector<std::uint8_t> request_body = serialisation::serialise(CREATION_WRAPPER(Client::CreateTestRequest), key);

	return make_request_from_body(Client::RequestId_Test, request_body);
}
