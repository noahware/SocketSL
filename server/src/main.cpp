#include <log/log.hpp>

#include "connection/listener.hpp"
#include "network/socket.hpp"

namespace
{
	void set_up_ssl_context(sl::ssl_context& context)
	{
		context.require_peer_verification();

		context.load_verify_file("certificate_authority.pem");
		context.use_certificate("server_certificate.pem", sl::ssl_context::crypto_file_format::pem);
		context.use_private_key("server_private_key.pem", sl::ssl_context::crypto_file_format::pem);

		context.use_tmp_dh_file("dhparams.pem");
	}
}

std::int32_t main()
{
	try
	{
		LOG_INFO("server");

		const auto client_ssl_context = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_server);

		set_up_ssl_context(*client_ssl_context);

		const auto io_context = std::make_shared<boost::asio::io_context>();
		const auto client_listener = std::make_shared<sl::boost_connection_listener<sl::client_connection>>(io_context, client_ssl_context, 2457);

		client_listener->async_wait_for_connection();

		io_context->run();
	}
	catch (const std::exception& e)
	{
		LOG_ERR(e.what());
	}

	return 0;
}
