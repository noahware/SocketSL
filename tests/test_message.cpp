#include <message/message.hpp>
#include <endian/endian.hpp>

#include <gtest/gtest.h>
#include <schema/test_message_generated.h>

#include "memory_socket.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace
{
	constexpr sl::msg::message_id_t ping_id = 42;
}

TEST(Message, SendRecvRoundTrip)
{
	test::memory_socket socket;

	const bool sent = sl::msg::send<TestMsg::CreatePing>(socket, ping_id, 7u, 0xABCDull);
	EXPECT_TRUE(sent);

	std::vector<std::uint8_t> body;
	const TestMsg::Ping* ping = sl::msg::recv<TestMsg::Ping>(socket, body);

	ASSERT_NE(ping, nullptr);
	EXPECT_EQ(ping->seq(), 7u);
	EXPECT_EQ(ping->value(), 0xABCDull);
}

TEST(Message, RecvBufferReturnsMessageId)
{
	test::memory_socket socket;
	(void)sl::msg::send<TestMsg::CreatePing>(socket, ping_id, 1u, 2ull);

	std::vector<std::uint8_t> body;
	const std::optional<sl::msg::message_id_t> id = sl::msg::recv_buffer(socket, body);

	ASSERT_TRUE(id.has_value());
	EXPECT_EQ(*id, ping_id);
}

TEST(Message, RecvOnEmptySocketFails)
{
	test::memory_socket socket;

	std::vector<std::uint8_t> body;
	EXPECT_FALSE(sl::msg::recv_buffer(socket, body).has_value());
}

TEST(Message, MakeFromBodyPrependsHeader)
{
	const std::vector<std::uint8_t> body = {1, 2, 3, 4};
	const sl::msg::message_t message = sl::msg::make_from_body(ping_id, body);

	EXPECT_GT(message.header_size, 0u);
	EXPECT_EQ(message.buffer.size(), message.header_size + body.size());

	// the body bytes sit immediately after the header
	const std::span<const std::uint8_t> tail(message.buffer.data() + message.header_size, body.size());
	EXPECT_TRUE(std::equal(tail.begin(), tail.end(), body.begin()));
}

TEST(MaxMessageSize, UnlimitedByDefaultAcceptsMessage)
{
	test::memory_socket socket;
	EXPECT_EQ(socket.max_message_size(), 0u);

	(void)sl::msg::send<TestMsg::CreatePing>(socket, ping_id, 5u, 6ull);

	std::vector<std::uint8_t> body;
	EXPECT_NE(sl::msg::recv<TestMsg::Ping>(socket, body), nullptr);
}

TEST(MaxMessageSize, RejectsOversizedHeader)
{
	test::memory_socket socket;
	socket.set_max_message_size(4); // smaller than any real MessageHeader flatbuffer

	(void)sl::msg::send<TestMsg::CreatePing>(socket, ping_id, 5u, 6ull);

	std::vector<std::uint8_t> body;
	EXPECT_FALSE(sl::msg::recv_buffer(socket, body).has_value());
}

TEST(MaxMessageSize, RejectsOversizedBodyBeforeAllocating)
{
	test::memory_socket socket;
	socket.set_max_message_size(1024);

	// craft a frame whose header claims a 1 GiB body, but write no body at all:
	// the cap must reject it at the body-size check, before resizing/reading the body
	const std::vector<std::uint8_t> header = sl::msg::make_header(ping_id, 1024ull * 1024 * 1024);

	(void)socket.write<sl::msg::frame_size_t>(
		sl::endian::to_little(static_cast<sl::msg::frame_size_t>(header.size())));
	(void)socket.write(header);

	std::vector<std::uint8_t> body;
	EXPECT_FALSE(sl::msg::recv_buffer(socket, body).has_value());
	EXPECT_TRUE(body.empty());
}
