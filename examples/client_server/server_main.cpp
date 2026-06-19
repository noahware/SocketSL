#include <connection/session_manager.hpp>
#include <network/socket.hpp>

#include <message/message.hpp>
#include <router/router.hpp>
#include <schema/schema.hpp>
#include <schema/request_generated.h>
#include <schema/response_generated.h>

#include <log/log.hpp>

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

	void handle_valid_test_request(const std::shared_ptr<sl::session>& sess, const Client::TestRequest* const request_body)
	{
		LOG_INFO("test request key: 0x{:X}", request_body->key());

		constexpr std::uint64_t response_key = 0x56789;

		sl::msg::async_send(sess->socket(),
			Client::ResponseId_Test,
			[](const bool is_valid)
			{
				if (is_valid)
				{
					LOG_INFO("successfully sent response");
				}
				else
				{
					LOG_ERR("failed to send response");
				}
			},
			CREATION_WRAPPER(Client::CreateTestResponse), response_key);
	}

	constexpr sl::message_info<Client::TestRequest, sl::session> test_request{Client::RequestId_Test, handle_valid_test_request};

	using request_router = sl::message_router<test_request>;

	class client_connection final : public sl::session
	{
	public:
		using session::session;

	protected:
		void handle_message(const message_id_t id, const body_buffer_t body) override
		{
			if (!request_router::dispatch(id, shared_as<sl::session>(), *body))
			{
				LOG_ERR("unknown request type: {}", id);
			}
		}
	};
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

		const auto manager = std::make_shared<sl::boost_session_manager<client_connection>>(
			pool.get_executor(), client_ssl_context, 2457);

		manager->set_timeout(std::chrono::seconds(10));
		manager->async_wait_for_connection();

		boost::asio::signal_set signals(pool.get_executor(), SIGINT, SIGTERM);
		signals.async_wait(
			[manager](const boost::system::error_code&, int)
			{
				LOG_INFO("shutting down");

				manager->stop();
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
