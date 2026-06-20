#include <router/router.hpp>
#include <serialisation/serialisation.hpp>

#include <gtest/gtest.h>
#include <schema/test_message_generated.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace
{
	struct fake_session {};

	int g_call_count = 0;
	std::uint32_t g_last_seq = 0;

	void on_ping(const std::shared_ptr<fake_session>&, const TestMsg::Ping* ping)
	{
		++g_call_count;
		g_last_seq = ping->seq();
	}

	constexpr sl::message_info<TestMsg::Ping, fake_session> ping_info{5, on_ping};
	using ping_router = sl::message_router<ping_info>;

	std::vector<std::uint8_t> make_ping(const std::uint32_t seq)
	{
		return sl::serialisation::serialise(sl::serialisation::lift<TestMsg::CreatePing>(), seq, 0ull);
	}
}

TEST(Router, DispatchesMatchingIdToHandler)
{
	g_call_count = 0;
	g_last_seq = 0;

	const auto body = make_ping(123u);
	const auto session = std::make_shared<fake_session>();

	EXPECT_TRUE(ping_router::dispatch(5, session, body));
	EXPECT_EQ(g_call_count, 1);
	EXPECT_EQ(g_last_seq, 123u);
}

TEST(Router, UnknownIdIsNotHandled)
{
	g_call_count = 0;

	const auto body = make_ping(1u);
	const auto session = std::make_shared<fake_session>();

	EXPECT_FALSE(ping_router::dispatch(99, session, body));
	EXPECT_EQ(g_call_count, 0);
}

TEST(Router, InvalidBodyIsConsumedButHandlerNotCalled)
{
	g_call_count = 0;

	const std::vector<std::uint8_t> garbage(16, 0xFF);
	const auto session = std::make_shared<fake_session>();

	// id matches but the body fails validation: dispatch reports it handled (consumed),
	// yet the typed handler must not run on an invalid buffer
	EXPECT_TRUE(ping_router::dispatch(5, session, garbage));
	EXPECT_EQ(g_call_count, 0);
}
