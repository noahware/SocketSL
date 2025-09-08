#include <request/request.hpp>
#include <response/response.hpp>
#include <schema/response_generated.h>

#include <spdlog/spdlog.h>

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

static void set_up_ssl_context(ssl_context_t& ssl_context)
{
	ssl_context.require_peer_verification();

	ssl_context.load_verify_file("certificate_authority.pem");
	ssl_context.use_certificate("client_certificate.pem", ssl_context_t::crypto_file_format_t::pem);
	ssl_context.use_private_key("client_private_key.pem", ssl_context_t::crypto_file_format_t::pem);
}

static void connect_to_server(socket_t& socket)
{
	if (socket.connect("127.0.0.1", "2457"))
	{
		if (socket.handshake(socket_t::handshake_type_t::client))
		{
			spdlog::info("handshake was successful");

			constexpr std::uint64_t request_key = 0x12345;

			send_test_request(socket, request_key);

			receive_test_response(socket);
		}
		else
		{
			spdlog::error("failed to handshake");
		}
	}
	else
	{
		spdlog::error("failed to connect to server");
	}
}

std::int32_t main()
{
	try
	{
		spdlog::info("client");

		const auto io_context = std::make_shared<boost::asio::io_context>();
		const auto ssl_context = std::make_shared<boost_ssl_context_t>(boost_ssl_context_t::ssl_method_t::tlsv12_client);

		set_up_ssl_context(*ssl_context);

		boost_tcp_socket_t socket(io_context, ssl_context);

		connect_to_server(socket);

		std::system("pause");
	}
	catch (const std::exception& e)
	{
		spdlog::error(e.what());
	}

	return 0;
}
