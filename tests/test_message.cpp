#include <message/message.hpp>
#include <endian/endian.hpp>
#include <serialisation/serialisation.hpp>

#include <gtest/gtest.h>
#include <schema/test_message_generated.h>
#include <schema/message_generated.h>
#include <schema/system_generated.h>

#include "memory_socket.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
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

TEST(Message, MakeFromBodySeparatesHeaderAndBody)
{
	const std::vector<std::uint8_t> body = {1, 2, 3, 4};
	const sl::msg::message_t message = sl::msg::make_from_body(ping_id, body);

	EXPECT_FALSE(message.header.empty());
	EXPECT_EQ(message.body.size(), body.size());
	EXPECT_TRUE(std::equal(message.body.begin(), message.body.end(), body.begin()));
}

TEST(ZeroCopy, SendViewRoundTrip)
{
	test::memory_socket socket;

	const auto body = sl::serialisation::serialise(sl::serialisation::lift<TestMsg::CreatePing>(), 7u, 0xABCDull);

	const bool sent = sl::msg::send_view(socket, ping_id, body);
	EXPECT_TRUE(sent);

	std::vector<std::uint8_t> recv_body;
	const TestMsg::Ping* ping = sl::msg::recv<TestMsg::Ping>(socket, recv_body);

	ASSERT_NE(ping, nullptr);
	EXPECT_EQ(ping->seq(), 7u);
	EXPECT_EQ(ping->value(), 0xABCDull);
}

TEST(ZeroCopy, AsyncSendViewRoundTrip)
{
	test::memory_socket socket;

	const auto body = sl::serialisation::serialise(sl::serialisation::lift<TestMsg::CreatePing>(), 3u, 42ull);

	bool handler_called = false;
	bool handler_result = false;
	sl::msg::async_send_view(socket, ping_id,
		[&](const bool ok) { handler_called = true; handler_result = ok; },
		std::span<const std::uint8_t>(body));

	EXPECT_TRUE(handler_called);
	EXPECT_TRUE(handler_result);

	std::vector<std::uint8_t> recv_body;
	const TestMsg::Ping* ping = sl::msg::recv<TestMsg::Ping>(socket, recv_body);

	ASSERT_NE(ping, nullptr);
	EXPECT_EQ(ping->seq(), 3u);
	EXPECT_EQ(ping->value(), 42ull);
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

TEST(SystemFrame, RecvSwallowsSystemFrameAndReturnsAppMessage)
{
	test::memory_socket socket;

	// a system frame (a pong) precedes a real application message
	(void)sl::msg::send<System::CreateHbPongRequest, true>(socket, System::MessageId_HbPong);
	(void)sl::msg::send<TestMsg::CreatePing>(socket, ping_id, 7u, 0xABCDull);

	std::vector<std::uint8_t> body;
	const std::optional<sl::msg::message_id_t> id = sl::msg::recv_buffer(socket, body);

	// the system frame is intercepted (never surfaced); the app message comes through
	ASSERT_TRUE(id.has_value());
	EXPECT_EQ(*id, ping_id);
}

TEST(SystemFrame, RecvAnswersPingWithPong)
{
	test::memory_socket socket;

	// an inbound heartbeat ping, then a real application message
	(void)sl::msg::send<System::CreateHbPingRequest, true>(socket, System::MessageId_HbPing);
	(void)sl::msg::send<TestMsg::CreatePing>(socket, ping_id, 1u, 2ull);
	const std::size_t before_recv = socket.bytes().size();

	std::vector<std::uint8_t> body;
	const std::optional<sl::msg::message_id_t> id = sl::msg::recv_buffer(socket, body);

	// the ping is intercepted (not surfaced); the app message comes through
	ASSERT_TRUE(id.has_value());
	EXPECT_EQ(*id, ping_id);

	// and recv answered the ping by writing a pong control frame back
	const std::span<const std::uint8_t> all = socket.bytes();
	ASSERT_GT(all.size(), before_recv);

	const std::span<const std::uint8_t> reply = all.subspan(before_recv);
	sl::msg::frame_size_t le_header_size = 0;
	std::memcpy(&le_header_size, reply.data(), sizeof(le_header_size));
	const std::size_t header_size = sl::endian::from_little(le_header_size);
	const std::span<const std::uint8_t> reply_header = reply.subspan(sizeof(le_header_size), header_size);

	ASSERT_TRUE(sl::serialisation::is_valid<MessageHeader>(reply_header));
	const auto* reply_msg_header = sl::serialisation::deserialise<MessageHeader>(reply_header);
	EXPECT_TRUE(reply_msg_header->is_system());
	EXPECT_EQ(reply_msg_header->type(), System::MessageId_HbPong);
}
