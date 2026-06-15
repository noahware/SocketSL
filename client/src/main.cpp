#include <request/request.hpp>
#include <response/response.hpp>
#include <schema/schema.hpp>
#include <schema/request_generated.h>
#include <schema/response_generated.h>

#include <log/log.hpp>

namespace
{
	void send_test_request(sl::socket& socket, const std::uint64_t request_key)
	{
		sl::request::send_request(socket, Client::RequestId_Test, CREATION_WRAPPER(Client::CreateTestRequest), request_key);
	}

	void receive_test_response(sl::socket& socket)
	{
		std::vector<std::uint8_t> response_buffer = { };

		const auto test_response = sl::response::read_response<Client::TestResponse>(socket, response_buffer);

		LOG_INFO("test response key: 0x{:X}", test_response->key());
	}

	void set_up_ssl_context(sl::ssl_context& context)
	{
		context.require_peer_verification();

		context.load_verify_file("certificate_authority.pem");
		context.use_certificate("client_certificate.pem", sl::ssl_context::crypto_file_format::pem);
		context.use_private_key("client_private_key.pem", sl::ssl_context::crypto_file_format::pem);
		context.use_tmp_dh_file("dhparams.pem");
	}

	void connect_to_server(sl::socket& socket)
	{
		if (socket.connect("127.0.0.1", "2457"))
		{
			if (socket.handshake(sl::socket::handshake_type::client))
			{
				LOG_INFO("handshake was successful");

				constexpr std::uint64_t request_key = 0x12345;

				send_test_request(socket, request_key);

				receive_test_response(socket);
			}
			else
			{
				LOG_ERR("failed to handshake");
			}
		}
		else
		{
			LOG_ERR("failed to connect to server");
		}
	}
}

std::int32_t main()
{
	try
	{
		LOG_INFO("client");

		const auto io_context = std::make_shared<boost::asio::io_context>();
		const auto ssl_ctx = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_client);

		set_up_ssl_context(*ssl_ctx);

		sl::boost_tcp_socket sock(io_context, ssl_ctx);

		connect_to_server(sock);
	}
	catch (const std::exception& e)
	{
		LOG_ERR(e.what());
	}

	return 0;
}
