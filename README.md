# SocketSL

A modern C++ client-server model built using SSL sockets.

# Examples

## Client

```cpp
void send_test_request(sl::socket& socket, const std::uint64_t request_key)
{
	sl::request::send(socket, Client::RequestId_Test, CREATION_WRAPPER(Client::CreateTestRequest), request_key);
}

void receive_test_response(sl::socket& socket)
{
	std::vector<std::uint8_t> response_buffer = { };

	const auto test_response = sl::response::read<Client::TestResponse>(socket, response_buffer);

	LOG_INFO("test response key: 0x{:X}", test_response->key());
}

void connect_to_server(sl::socket& socket)
{
	if (socket.connect("127.0.0.1", "2457"))
	{
		if (socket.handshake(sl::socket::handshake_type::client))
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
void handle_valid_test_request(const std::shared_ptr<sl::connection>& connection, const Client::TestRequest* const request_body)
{
	LOG_INFO("test request key: 0x{:X}", request_body->key());

	constexpr std::uint64_t response_key = 0x56789;

	sl::response::send(connection->socket(),
		[](const bool is_valid)
		{
			if (is_valid)
			{
				LOG_INFO("successfully sent response");
			}
		},
		CREATION_WRAPPER(Client::CreateTestResponse), response_key);
}

constexpr sl::request::request_info<Client::TestRequest> test_request{Client::RequestId_Test, handle_valid_test_request};

using router = sl::request::request_router<test_request>;

void sl::client_connection::handle_request(const sl::request::request_id_t request_id, const std::shared_ptr<std::vector<std::uint8_t>> body_buffer)
{
	if (!router::dispatch(request_id, shared_from_this(), *body_buffer))
	{
		LOG_ERR("unknown request type: {}", request_id);
	}
}
```


# Specification

## Sockets

The sockets are TCP TLS connections. The socket library can be interchanged with ease, due to the socket implementation being abstracted. By default, the project uses [`boost-asio`](https://github.com/boostorg/asio) (with no modifications to its original source code, adhering to the [Boost Software License](https://www.boost.org/LICENSE_1_0.txt)).

## SSL

The SSL context is configurable by using the member functions of `sl::ssl_context`:

```cpp
virtual void disable_peer_verification() = 0;
virtual void require_peer_verification() = 0;

virtual void load_verify_file(const std::string& path_to_file) = 0;
virtual void add_certificate_authority(std::span<const std::uint8_t> buffer) = 0;

virtual void use_tmp_dh_file(const std::string& path_to_file) = 0;
virtual void use_tmp_dh(std::span<const std::uint8_t> buffer) = 0;

virtual void use_certificate(const std::string& path_to_certificate, crypto_file_format file_format) = 0;
virtual void use_certificate(std::span<const std::uint8_t> buffer, crypto_file_format file_format) = 0;

virtual void use_private_key(const std::string& path_to_key, crypto_file_format file_format) = 0;
virtual void use_private_key(std::span<const std::uint8_t> buffer, crypto_file_format file_format) = 0;
```

ASN1 and PEM certificates/keys are supported.

Mutual TLS is also supported, you can enforce peer verification by invoking the `require_peer_verification` routine in `sl::ssl_context`.

Boost's SSL context is implemented as `sl::boost_ssl_context`.

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

The `shared/serialisation/serialisation.hpp` header contains templated routines to serialise an object into a byte buffer. Heres the forward declaration of `sl::serialisation::serialise`:

```cpp
template <class CreateFn, class ...Args>
std::vector<std::uint8_t> serialise(const CreateFn& create_fn, Args&&... args)
```

Generic templates wrap serialisation for requests and responses, so you never call `serialise` directly:

```cpp
// client: send a request in one call
sl::request::send(socket, Client::RequestId_Test, CREATION_WRAPPER(Client::CreateTestRequest), key);

// server: send a response in one call
sl::response::send(socket, handler, CREATION_WRAPPER(Client::CreateTestResponse), key);
```

## Server connections/requests

The server holds a base `sl::connection` class which implements all of the request header / body parsing, all it requires the developer to implement is the `handle_request` routine:

```cpp
virtual void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) = 0;
```

Use `sl::request::request_info` to declare constexpr request descriptors that store the request ID, FlatBuffer type, and handler function pointer. This is linked into a 'router' which automatically processes the request when received.

```cpp
void handle_valid_test_request(const std::shared_ptr<sl::connection>& conn, const Client::TestRequest* body)
{
	/* act on body, send response */
}

constexpr sl::request::request_info<Client::TestRequest> test_request{Client::RequestId_Test, handle_valid_test_request};

using router = sl::request::request_router<test_request>;

void sl::client_connection::handle_request(const sl::request::request_id_t request_id, const std::shared_ptr<std::vector<std::uint8_t>> body_buffer)
{
	if (!router::dispatch(request_id, shared_from_this(), *body_buffer))
	{
		LOG_ERR("unknown request type: {}", request_id);
	}
}
```

Adding a new request type is just defining a `constexpr request_info` and adding it to the `request_router` template parameter list. An example is included in the project already.

## Server's connection listener

The connection listener takes in the port and type of connection it has to listen to, once a connection is created, it will handshake and instantiate a connection of the templated type.

Here is an example with the `boost-asio` connection listener:

```cpp
const auto io_context = std::make_shared<boost::asio::io_context>();
const auto client_listener = std::make_shared<sl::boost_connection_listener<sl::client_connection>>(io_context, client_ssl_context, 2457);

client_listener->async_wait_for_connection();
```

# Usage

The project at the moment uses `mutual TLS with temporary dhparams` as an example, this is configured in the `set_up_ssl_context` functions in the client and server. This is purely an example of an SSL setup and the project supports many more SSL configurations.

## Building

Run the following commands from the project root directory to build in Release:

```
cmake -S . -B build
cmake --build build --config Release
```

## Key generation

The following commands use `-days 730` to specify the validity of the certificates to be of 2 years and `-subj` to specify the certificate parameters. These should be changed adequately for production.

First generate Diffie-Hellman param keys and the certificate authority:

```
openssl dhparam -out dhparams.pem 4096

openssl req -x509 -newkey rsa:4096 -nodes -keyout certificate_authority_key.pem -out certificate_authority.pem -subj "/CN=ca"
```

Next, generate the client and server private keys and CSRs:

```
openssl req -newkey rsa:4096 -nodes -keyout server_private_key.pem -out server_csr.pem -subj "/CN=server" -days 730 
openssl req -newkey rsa:4096 -nodes -keyout client_private_key.pem -out client_csr.pem -subj "/CN=client" -days 730 
```

Finally, generate the client and server certificates:

```
openssl x509 -req -in server_csr.pem -out server_certificate.pem -CA certificate_authority.pem -CAkey certificate_authority_key.pem -CAcreateserial -days 730
openssl x509 -req -in client_csr.pem -out client_certificate.pem -CA certificate_authority.pem -CAkey certificate_authority_key.pem -CAcreateserial -days 730
```

## Where to place keys

The following keys should be accessible by the client (in the example they should be in same directory as the client is executed in):

- dhparams.pem
- certificate_authority.pem
- client_certificate.pem
- client_private_key.pem

The following keys should be accessible by the server (in the example they should be in same directory as the server is executed in):

- dhparams.pem
- certificate_authority.pem
- server_certificate.pem
- server_private_key.pem

## Running

To run the client and server successfully, make sure the keys are generated and placed correctly, as explained by previous steps.

# Credits

- [papstuc](https://github.com/papstuc/) for his serialization code as an example & help with theory
