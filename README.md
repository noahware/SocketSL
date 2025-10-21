# SocketSL

A modern C++ client-server model built using SSL sockets.

# Examples

## Client

```cpp
void send_test_request(socket_t& socket, const std::uint64_t request_key)
{
	const auto [request_header_size, request_buffer] = request::construct::make_test_request(request_key);

	request::send_buffer(socket, request_buffer, request_header_size);
}

void receive_test_response(socket_t& socket)
{
	std::vector<std::uint8_t> response_buffer = { };

	const auto test_response = response::read_response<Client::TestResponse>(socket, response_buffer);

	spdlog::info("test response key: 0x{:X}", test_response->key());
}

void connect_to_server(socket_t& socket)
{
	if (socket.connect("127.0.0.1", "2457"))
	{
		if (socket.handshake(socket_t::handshake_type_t::client))
		{
			constexpr std::uint64_t request_key = 0x12345;

			send_test_request(socket, request_key);

			receive_test_response(socket);
		}
	}
}
```

## Server

```cpp
void handle_valid_test_request(socket_t& socket, const Client::TestRequest* const request_body)
{
	constexpr std::uint64_t response_key = 0x56789;
	const auto response_body = std::make_shared<std::vector<std::uint8_t>>(response::construct::make_test_response(response_key));

	response::async_send_buffer(socket, response_body,
		[response_key](const std::uint8_t is_valid)
		{
			if (is_valid)
			{
				spdlog::info("successfully sent test response (key: 0x{:X})", response_key);
			}
		}
	);
}

void client_connection_t::handle_request(const request::request_id_t request_id, const std::shared_ptr<std::vector<std::uint8_t>> body_buffer)
{
	if (request_id == Client::RequestId_Test)
	{
		if (serialisation::is_valid<Client::TestRequest>(*body_buffer))
		{
			const auto* request_body = serialisation::deserialise<Client::TestRequest>(*body_buffer);

			handle_valid_test_request(*socket_, request_body);
		}
	}
}
```


# Specification

## Sockets:

The sockets are TCP TLS connections. The socket library can be interchanged with ease, due to the socket implementation being abstracted. By default, the project uses [`boost-asio`](https://github.com/boostorg/asio) (with no modifications to its original source code, adhering to the [Boost Software License](https://www.boost.org/LICENSE_1_0.txt)).

## SSL:

The SSL context is configurable by using the member functions of `ssl_context_t`:

```cpp
virtual void disable_peer_verification() = 0;
virtual void require_peer_verification() = 0;

virtual void load_verify_file(const std::string& path_to_file) = 0;
virtual void add_certificate_authority(std::span<std::uint8_t> buffer) = 0;

virtual void use_tmp_dh_file(const std::string& path_to_file) = 0;
virtual void use_tmp_dh(std::span<std::uint8_t> buffer) = 0;

virtual void use_certificate(const std::string& path_to_certificate, crypto_file_format_t file_format) = 0;
virtual void use_certificate(std::span<std::uint8_t> buffer, crypto_file_format_t file_format) = 0;

virtual void use_private_key(const std::string& path_to_key, crypto_file_format_t file_format) = 0;
virtual void use_private_key(std::span<std::uint8_t> buffer, crypto_file_format_t file_format) = 0;
```

ASN1 and PEM certificates/keys are supported.

Mutual TLS is also supported, you can enforce peer verification by invoking the `require_peer_verification` routine in `ssl_context_t`.

Boost's SSL context is implemented as `boost_ssl_context_t`.

## Serialization

[`Flatbuffers`](https://github.com/google/flatbuffers) is used for serializing information to be set over the socket in an endian-friendly way. The raw packet size is also written always in little endian and converted to the system's native form. This ensures that any system no matter of its endianness can decode information sent to it.

Requests & responses are implemented in `.fbs` files as such, these are linked by the `Custom Build Tool` provided by `MSVC`.

```fbs
enum RequestId : uint8
{
    Test = 0
} 

table TestRequest
{
    key: uint64;
}

table TestResponse
{
    key: uint64;
}
```

The `shared/serialisation/serialisation.hpp` header contains templated routines to serialise an object into a byte buffer. Heres the forward declaration of `serialisation::serialise`:

```cpp
template <class creation_function_t, class ...arguments_t>
std::vector<std::uint8_t> serialise(const creation_function_t& creation_function, arguments_t&&... arguments)
```

This serialisation routine can be used as such:

```cpp
request::request_t request::construct::make_test_request(const std::uint64_t key)
{
	const std::vector<std::uint8_t> request_body = serialisation::serialise(CREATION_WRAPPER(Client::CreateTestRequest), key);

	/* return header + body */
}
```

That raw byte buffer can then be sent to the peer for processing.

## Server connections/requests

The server holds a base `connection_t` class which implements all of the request header / body parsing, all it requires the developer to implement is the `handle_request` routine:

```cpp
virtual void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) = 0;
```

You may define different types of connections as such:

```cpp
class client_connection_t final : public connection_t
{
public:
	explicit client_connection_t(std::unique_ptr<socket_t> socket, std::shared_ptr<connection_listener_t> parent_listener)
		: connection_t(std::move(socket), std::move(parent_listener)) {}

protected:
	void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) override
  {
    if (request_id == Client::RequestId_Test)
  	{
  		if (serialisation::is_valid<Client::TestRequest>(*body_buffer))
  		{
  			const auto* request_body = serialisation::deserialise<Client::TestRequest>(*body_buffer);
  
  			/* act on request_body */
  		}
  	}
  }
};
```

A working example is included in the project already.

## Server's connection listener

The connection listener takes in the port and type of connection it has to listen to, once a connection is created, it will handshake and instantiate a connection of the templated type.

Here is an example with the `boost-asio` connection listener:

```cpp
const auto io_context = std::make_shared<boost::asio::io_context>();
const auto client_listener = std::make_shared<boost_connection_listener_t<client_connection_t>>(io_context, client_ssl_context, 2457);

client_listener->async_wait_for_connection();
```

# Credits

- [papstuc](https://github.com/papstuc/) for his serialization code as an example & help with theory
