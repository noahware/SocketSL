#include <spdlog/spdlog.h>

#include "connection/listener.hpp"
#include "network/socket.hpp"

static void set_up_ssl_context(ssl_context_t& ssl_context)
{
	ssl_context.disable_peer_verification();

	ssl_context.use_certificate("server_certificate.pem", ssl_context_t::crypto_file_format_t::pem);
	ssl_context.use_private_key("server_private_key.pem", ssl_context_t::crypto_file_format_t::pem);
}

std::int32_t main()
{
	try
	{
		spdlog::info("server");

		const auto client_ssl_context = std::make_shared<boost_ssl_context_t>(boost_ssl_context_t::ssl_method_t::tlsv12_server);

		set_up_ssl_context(*client_ssl_context);

		const auto io_context = std::make_shared<boost::asio::io_context>();
		const auto client_listener = std::make_shared<boost_connection_listener_t<client_connection_t>>(io_context, client_ssl_context, 2457);

		client_listener->async_wait_for_connection();

		io_context->run();
	}
	catch (const std::exception& e)
	{
		spdlog::error(e.what());
	}

	std::system("pause");

	return 0;
}
