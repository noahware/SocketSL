#include <spdlog/spdlog.h>

#include "connection/listener.hpp"
#include "network/socket.hpp"

std::int32_t main()
{
	try
	{
		spdlog::info("server");

		const auto io_context = std::make_shared<boost::asio::io_context>();

		constexpr std::uint16_t port = 2457;

		const auto client_ssl_context = std::make_shared<boost_ssl_context_t>(boost_ssl_context_t::ssl_method_t::tlsv12_server);
		const auto client_listener = std::make_shared<boost_connection_listener_t<client_connection_t>>(io_context, client_ssl_context, port);

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
