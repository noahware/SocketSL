#include <log/log.hpp>

#include <connection/listener.hpp>
#include <network/socket.hpp>

#include "connection/client_connection.hpp"

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
		const auto thread_count = std::thread::hardware_concurrency();
		LOG_INFO("server (thread pool: {} threads)", thread_count);

		const auto client_ssl_context = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_server);

		set_up_ssl_context(*client_ssl_context);

		boost::asio::thread_pool pool(std::thread::hardware_concurrency());

		const auto client_listener = std::make_shared<sl::boost_connection_listener<sl::client_connection>>(
			pool.get_executor(), client_ssl_context, 2457);

		client_listener->set_timeout(std::chrono::seconds(10));
		client_listener->async_wait_for_connection();

		boost::asio::signal_set signals(pool.get_executor(), SIGINT, SIGTERM);
		signals.async_wait(
			[&client_listener](const boost::system::error_code&, int)
			{
				LOG_INFO("shutting down");

				client_listener->stop();
			}
		);

		pool.join();
	}
	catch (const std::exception& e)
	{
		LOG_ERR(e.what());
	}

	return 0;
}
