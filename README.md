# SocketSL

A modern C++ client-server model built using SSL sockets. It can be used both synchronously or asynchronously.

# Examples

## Client-server model

### Client

```cpp
void send_test_request(sl::socket& socket, const std::uint64_t request_key)
{
	sl::msg::send<Client::CreateTestRequest>(socket, Client::RequestId_Test, request_key);
}

void receive_test_response(sl::socket& socket)
{
	std::vector<std::uint8_t> response_buffer = { };

	const auto test_response = sl::msg::recv<Client::TestResponse>(socket, response_buffer);

	if (test_response)
	{
		LOG_INFO("test response key: 0x{:X}", test_response->key());
	}
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

### Server

```cpp
void handle_valid_test_request(const std::shared_ptr<sl::session>& sess, const Client::TestRequest* const request_body)
{
	LOG_INFO("test request key: 0x{:X}", request_body->key());

	constexpr std::uint64_t response_key = 0x56789;

	sl::msg::async_send<Client::CreateTestResponse>(sess->socket(), Client::ResponseId_Test, response_key);
}

constexpr sl::message_info<Client::TestRequest, sl::session> test_request{Client::RequestId_Test, handle_valid_test_request};

using router = sl::message_router<test_request>;

void sl::client_connection::handle_message(const sl::session::message_id_t id, const sl::session::body_buffer_t body)
{
	if (!router::dispatch(id, shared_as<sl::session>(), *body))
	{
		LOG_ERR("unknown request type: {}", id);
	}
}
```

## Peer-to-peer model

```cpp
void handle_random_number(const std::shared_ptr<sl::session>&, const Peer::RandomNumber* message)
{
	LOG_INFO("received random number: {}", message->value());
}

constexpr sl::message_info<Peer::RandomNumber, sl::session> random_number{Peer::MessageId_Random, handle_random_number};

using peer_router = sl::message_router<random_number>;

// inbound or outbound, every peer connection is the same session
class peer_session final : public sl::session
{
	using session::session;

	void handle_message(const message_id_t id, const body_buffer_t body) override
	{
		peer_router::dispatch(id, shared_as<sl::session>(), *body);
	}
};
```

```cpp
const auto manager = std::make_shared<sl::boost_session_manager<peer_session>>(executor, server_ctx, port);

manager->async_wait_for_connection();                        // accept inbound peers

manager->connect("127.0.0.1", "5002");                 // dial an outbound peer

// broadcast to every connected peer (inbound + outbound), e.g. on a timer
manager->for_each_session([](const std::shared_ptr<sl::session>& sess)
{
	sl::msg::async_send<Peer::CreateRandomNumber>(sess->socket(), Peer::MessageId_Random, 0x1234);
});
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

The `src/serialisation/serialisation.hpp` header contains templated routines to serialise an object into a byte buffer. Heres the forward declaration of `sl::serialisation::serialise`:

```cpp
template <class CreateFn, class ...Args>
std::vector<std::uint8_t> serialise(const CreateFn& create_fn, Args&&... args)
```

Generic templates wrap serialisation for requests and responses, so you never call `serialise` directly:

```cpp
// client: send a request in one call
sl::msg::send<Client::CreateTestRequest>(socket, Client::RequestId_Test, key);

// server: send a response in one call (fire and forget)
sl::msg::async_send<Client::CreateTestResponse>(socket, Client::ResponseId_Test, key);

// an optional completion handler -- void() or void(bool is_valid) -- may precede the body args
sl::msg::async_send<Client::CreateTestResponse>(socket, Client::ResponseId_Test, [](bool ok) { /* ... */ }, key);
```

## Sessions / requests

The library exposes one base `sl::session` class (used for both directions) which implements all of the framing and header / body parsing; the developer only implements the `handle_message` routine:

```cpp
virtual void handle_message(session::message_id_t id, session::body_buffer_t body) = 0;
```

Use `sl::message_info` to declare constexpr message descriptors that store the message ID, FlatBuffer type, and handler function pointer. These are linked into a `message_router` which automatically dispatches the message when received.

```cpp
void handle_valid_test_request(const std::shared_ptr<sl::session>& sess, const Client::TestRequest* body)
{
	/* act on body, send response */
}

constexpr sl::message_info<Client::TestRequest, sl::session> test_request{Client::RequestId_Test, handle_valid_test_request};

using router = sl::message_router<test_request>;

void sl::client_connection::handle_message(const sl::session::message_id_t id, const sl::session::body_buffer_t body)
{
	if (!router::dispatch(id, shared_as<sl::session>(), *body))
	{
		LOG_ERR("unknown request type: {}", id);
	}
}
```

Adding a new message type is just defining a `constexpr message_info` and adding it to the `message_router` template parameter list. An example is included in the project already.

## Session manager

The session manager takes in the port and the session type it manages; for each accepted connection it handshakes and instantiates a session of the templated type.

Here is an example with the `boost-asio` session manager:

```cpp
const auto manager = std::make_shared<sl::boost_session_manager<sl::client_connection>>(
	executor, client_ssl_context, 2457);

manager->async_wait_for_connection();
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
