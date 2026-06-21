#include <connection/session_manager.hpp>

#include <gtest/gtest.h>

#include "memory_socket.hpp"

#include <cstdint>
#include <memory>

namespace
{
	// the base session_manager is abstract only in async_wait_for_connection, which these tests
	// don't exercise -- everything they need (add_session / admit / session_count) is in the base
	class test_manager final : public sl::session_manager
	{
	public:
		void async_wait_for_connection() override {}
	};

	class noop_session final : public sl::session
	{
	public:
		using sl::session::session;

	protected:
		void handle_message(message_id_t, body_buffer_t) override {}
	};

	// a session over an in-memory socket reporting the given source ip (no real handshake/socket)
	std::shared_ptr<noop_session> make_session(const std::uint32_t ip)
	{
		auto socket = std::make_unique<test::memory_socket>();
		socket->set_ipv4_address(ip);

		return std::make_shared<noop_session>(std::move(socket));
	}
}

TEST(SessionCaps, MaxSessionsRejectsBeyondLimit)
{
	test_manager manager;
	manager.set_max_sessions(2);

	manager.add_session(make_session(0x0A000001));
	manager.add_session(make_session(0x0A000002));
	manager.add_session(make_session(0x0A000003)); // over the cap -> rejected before registering

	EXPECT_EQ(manager.session_count(), 2u);
}

TEST(SessionCaps, PerIpLimitIsPerSourceAddress)
{
	test_manager manager;
	manager.set_max_connections_per_ip(1);

	manager.add_session(make_session(0x0A000001)); // ip A -> admitted
	manager.add_session(make_session(0x0A000001)); // ip A again -> over the per-ip limit -> rejected
	manager.add_session(make_session(0x0A000002)); // ip B -> admitted (limit is per source, not total)

	EXPECT_EQ(manager.session_count(), 2u);
}
