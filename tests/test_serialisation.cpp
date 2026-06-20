#include <serialisation/serialisation.hpp>

#include <gtest/gtest.h>
#include <schema/test_message_generated.h>

#include <cstdint>
#include <vector>

TEST(Serialisation, RoundTripsTableFields)
{
	const auto bytes = sl::serialisation::serialise(
		sl::serialisation::lift<TestMsg::CreatePing>(), 7u, 0xDEADBEEFull);

	ASSERT_TRUE(sl::serialisation::is_valid<TestMsg::Ping>(bytes));

	const TestMsg::Ping* ping = sl::serialisation::deserialise<TestMsg::Ping>(bytes);

	ASSERT_NE(ping, nullptr);
	EXPECT_EQ(ping->seq(), 7u);
	EXPECT_EQ(ping->value(), 0xDEADBEEFull);
}

TEST(Serialisation, IsValidRejectsGarbage)
{
	const std::vector<std::uint8_t> garbage(16, 0xFF);

	EXPECT_FALSE(sl::serialisation::is_valid<TestMsg::Ping>(garbage));
}

TEST(Serialisation, IsValidRejectsEmptyBuffer)
{
	const std::vector<std::uint8_t> empty;

	EXPECT_FALSE(sl::serialisation::is_valid<TestMsg::Ping>(empty));
}
