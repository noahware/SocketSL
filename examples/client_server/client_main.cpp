#include <connection/session.hpp>
#include <connection/session_manager.hpp>

#include <message/message.hpp>
#include <router/router.hpp>
#include <schema/request_generated.h>
#include <schema/response_generated.h>

#include "../common/log.hpp"

#include <chrono>
#include <csignal>
#include <string_view>
#include <thread>

namespace
{
	void set_up_ssl_context(sl::ssl_context& context)
	{
		context.require_peer_verification();

		context.load_verify_file("certificate_authority.pem");
		context.use_certificate("client_certificate.pem", sl::ssl_context::crypto_file_format::pem);
		context.use_private_key("client_private_key.pem", sl::ssl_context::crypto_file_format::pem);
		context.use_tmp_dh_file("dhparams.pem");
	}

	void run_sync_client()
	{
		LOG_INFO("sync client");

		boost::asio::io_context io_context;
		const auto ssl_ctx = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_client);
		set_up_ssl_context(*ssl_ctx);

		sl::boost_tcp_socket sock(io_context.get_executor(), ssl_ctx);

		if (!sock.connect("127.0.0.1", "2457"))
		{
			LOG_ERR("failed to connect to server");
			return;
		}

		if (!sock.handshake(sl::socket::handshake_type::client))
		{
			LOG_ERR("failed to handshake");
			return;
		}

		LOG_INFO("handshake was successful");

		constexpr std::uint64_t request_key = 0x12345;
		sl::msg::send<Client::CreateTestRequest>(sock, Client::RequestId_Test, request_key);

		std::vector<std::uint8_t> response_buffer = {};
		const auto* response = sl::msg::recv<Client::TestResponse>(sock, response_buffer);

		if (!response)
		{
			LOG_ERR("failed to receive response");

			return;
		}

		LOG_INFO("test response key: 0x{:X}", response->key());
	}

	void handle_test_response([[maybe_unused]] const std::shared_ptr<sl::session>& sess, const Client::TestResponse* response)
	{
		LOG_INFO("test response key: 0x{:X}", response->key());
	}

	constexpr sl::message_info<Client::TestResponse, sl::session> test_response{Client::ResponseId_Test, handle_test_response};

	using response_router = sl::message_router<test_response>;

	class async_client final : public sl::session
	{
	public:
		using session::session;

	protected:
		void handle_message(const message_id_t id, const body_buffer_t body) override
		{
			if (!response_router::dispatch(id, shared_as<sl::session>(), *body))
			{
				LOG_ERR("unknown response type: {}", id);
			}
		}
	};

	void run_async_client()
	{
		LOG_INFO("async client");

		boost::asio::io_context io_context;
		const auto ssl_ctx = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_client);
		set_up_ssl_context(*ssl_ctx);

		auto socket = std::make_unique<sl::boost_tcp_socket>(io_context.get_executor(), ssl_ctx);
		auto session = std::make_shared<async_client>(std::move(socket));

		if (!session->connect("127.0.0.1", "2457"))
		{
			LOG_ERR("failed to connect to server");
			return;
		}

		if (!session->handshake(sl::socket::handshake_type::client))
		{
			LOG_ERR("failed to handshake");
			return;
		}

		LOG_INFO("handshake was successful");

		constexpr std::uint64_t request_key = 0x12345;
		sl::msg::send<Client::CreateTestRequest>(session->socket(), Client::RequestId_Test, request_key);

		session->start();
		io_context.run();
	}

	void run_silent_client()
	{
		LOG_INFO("silent client (connect + handshake, then idle without answering pings)");

		boost::asio::io_context io_context;
		const auto ssl_ctx = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_client);
		set_up_ssl_context(*ssl_ctx);

		sl::boost_tcp_socket sock(io_context.get_executor(), ssl_ctx);

		if (!sock.connect("127.0.0.1", "2457"))
		{
			LOG_ERR("failed to connect to server");
			return;
		}

		if (!sock.handshake(sl::socket::handshake_type::client))
		{
			LOG_ERR("failed to handshake");
			return;
		}

		LOG_INFO("handshake ok; idling silently for 12s (never reading, so never answering pings)");

		std::this_thread::sleep_for(std::chrono::seconds(12));

		LOG_INFO("silent client done (was alive the whole time)");
	}

	void run_reconnect_client()
	{
		LOG_INFO("reconnect client (auto-reconnect with backoff)");

		boost::asio::io_context io_context;
		const auto ssl_ctx = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_client);
		set_up_ssl_context(*ssl_ctx);

		const auto manager = std::make_shared<sl::boost_session_manager<async_client>>(
			io_context.get_executor(), ssl_ctx, 0);

		manager->on_connect([](const std::shared_ptr<sl::session>& sess)
		{
			LOG_INFO("connected to {}:{}", sess->socket().remote_address(), sess->socket().port());

			constexpr std::uint64_t request_key = 0x12345;
			sl::msg::send<Client::CreateTestRequest>(sess->socket(), Client::RequestId_Test, request_key);
		});

		manager->on_disconnect([](const std::shared_ptr<sl::session>&)
		{
			LOG_INFO("disconnected -- waiting for auto-reconnect");
		});

		sl::reconnect_policy policy;
		policy.initial_delay = std::chrono::seconds(1);
		policy.max_delay = std::chrono::seconds(5);
		policy.idle_timeout = std::chrono::seconds(5);

		manager->connect("127.0.0.1", "2457", ssl_ctx, policy);

		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait(
			[&manager](const boost::system::error_code&, int)
			{
				LOG_INFO("shutting down");
				manager->stop();
			}
		);

		io_context.run();
	}
}

std::int32_t main(const int argc, const char* const* argv)
{
	try
	{
		const auto mode = argc > 1 ? std::string_view(argv[1]) : std::string_view{};

		if (mode == "silent")
		{
			run_silent_client();
		}
		else if (mode == "reconnect")
		{
			run_reconnect_client();
		}
		else
		{
			run_sync_client();
			run_async_client();
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERR(e.what());
	}

	return 0;
}
