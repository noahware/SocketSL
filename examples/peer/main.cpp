#include <connection/session_manager.hpp>
#include <message/message.hpp>
#include <router/router.hpp>
#include <schema/peer_generated.h>
#include <network/socket.hpp>

#include "../common/log.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <thread>

namespace
{
	// a peer needs two identities for mutual TLS: a server one to accept inbound
	// dials, and a client one to dial out. they share the same trust anchor (CA).
	void set_up_ssl_context(sl::ssl_context& context, const std::string& certificate, const std::string& private_key)
	{
		context.require_peer_verification();

		context.load_verify_file("certificate_authority.pem");
		context.use_certificate(certificate, sl::ssl_context::crypto_file_format::pem);
		context.use_private_key(private_key, sl::ssl_context::crypto_file_format::pem);
		context.use_tmp_dh_file("dhparams.pem");
	}

	void handle_random_number([[maybe_unused]] const std::shared_ptr<sl::session>& sess, const Peer::RandomNumber* const message)
	{
		LOG_INFO("received random number: {}", message->value());
	}

	constexpr sl::message_info<Peer::RandomNumber, sl::session> random_number{Peer::MessageId_Random, handle_random_number};

	using peer_router = sl::message_router<random_number>;

	// every peer connection -- inbound (accepted) or outbound (dialled) -- is the same session
	class peer_session final : public sl::session
	{
	public:
		using session::session;

	protected:
		void handle_message(const message_id_t id, const body_buffer_t body) override
		{
			if (!peer_router::dispatch(id, shared_as<sl::session>(), *body))
			{
				LOG_ERR("unknown message type: {}", id);
			}
		}
	};

	// broadcasts a fresh random number to every connected peer once per second
	class broadcaster final : public std::enable_shared_from_this<broadcaster>
	{
	public:
		broadcaster(boost::asio::any_io_executor executor, std::shared_ptr<sl::session_manager> manager)
				:	timer_(std::move(executor)),
					manager_(std::move(manager)),
					rng_(std::random_device{}()) {}

		void start()
		{
			schedule();
		}

		void stop()
		{
			timer_.cancel();
		}

	private:
		void schedule()
		{
			timer_.expires_after(std::chrono::seconds(1));

			timer_.async_wait(
				[self = shared_from_this()](const boost::system::error_code& error_code)
				{
					if (error_code)
					{
						return;
					}

					self->broadcast();
					self->schedule();
				}
			);
		}

		void broadcast()
		{
			const std::uint64_t value = dist_(rng_);

			LOG_INFO("broadcasting random number {} to {} peer(s)", value, manager_->session_count());

			manager_->for_each_session(
				[value](const std::shared_ptr<sl::session>& sess)
				{
					sl::msg::async_send<Peer::CreateRandomNumber>(sess->socket(), Peer::MessageId_Random, value);
				}
			);
		}

		boost::asio::steady_timer timer_;
		std::shared_ptr<sl::session_manager> manager_;
		std::mt19937_64 rng_;
		std::uniform_int_distribution<std::uint64_t> dist_;
	};
}

std::int32_t main(int argc, char** argv)
{
	try
	{
		if (argc < 2)
		{
			LOG_ERR("usage: peer <listen-port> [host:port ...]");

			return 1;
		}

		const auto listen_port = static_cast<std::uint16_t>(std::stoul(argv[1]));

		const auto thread_count = std::thread::hardware_concurrency();
		LOG_INFO("peer starting (thread pool: {} threads)", thread_count);

		boost::asio::thread_pool pool(thread_count);

		const auto server_context = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_server);
		set_up_ssl_context(*server_context, "server_certificate.pem", "server_private_key.pem");

		const auto client_context = std::make_shared<sl::boost_ssl_context>(sl::boost_ssl_context::ssl_method_type::tlsv12_client);
		set_up_ssl_context(*client_context, "client_certificate.pem", "client_private_key.pem");

		const auto manager = std::make_shared<sl::boost_session_manager<peer_session>>(pool.get_executor(), server_context, listen_port);

		manager->set_timeout(std::chrono::seconds(10));

		manager->on_connect([](const std::shared_ptr<sl::session>& sess) { LOG_INFO("peer connected: {}:{}", sess->socket().remote_address(), sess->socket().port()); });
		manager->on_disconnect([](const std::shared_ptr<sl::session>& sess) { LOG_INFO("peer disconnected: {}:{}", sess->socket().remote_address(), sess->socket().port()); });

		manager->async_wait_for_connection();

		LOG_INFO("peer listening on port {}", listen_port);

		for (int i = 2; i < argc; ++i)
		{
			const std::string address = argv[i];
			const auto separator = address.rfind(':');

			if (separator == std::string::npos)
			{
				LOG_WARN("skipping malformed peer address '{}' (expected host:port)", address);

				continue;
			}

			const std::string host = address.substr(0, separator);
			const std::string service = address.substr(separator + 1);

			LOG_INFO("connecting to peer {}:{}", host, service);

			manager->connect(host, service, client_context);
		}

		const auto random_broadcaster = std::make_shared<broadcaster>(pool.get_executor(), manager);
		random_broadcaster->start();

		boost::asio::signal_set signals(pool.get_executor(), SIGINT, SIGTERM);
		signals.async_wait(
			[manager, random_broadcaster](const boost::system::error_code&, int)
			{
				LOG_INFO("shutting down");

				random_broadcaster->stop();
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
