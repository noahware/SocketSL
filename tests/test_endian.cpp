#include <endian/endian.hpp>

#include <gtest/gtest.h>

#include <cstdint>

TEST(Endian, RoundTrips)
{
	constexpr std::uint64_t value = 0x0123456789ABCDEFull;

	EXPECT_EQ(sl::endian::from_little(sl::endian::to_little(value)), value);
}

TEST(Endian, ToLittleProducesLittleEndianByteOrder)
{
	const std::uint32_t value = 0x01020304u;
	const std::uint32_t little = sl::endian::to_little(value);

	const auto* bytes = reinterpret_cast<const std::uint8_t*>(&little);

	EXPECT_EQ(bytes[0], 0x04);
	EXPECT_EQ(bytes[1], 0x03);
	EXPECT_EQ(bytes[2], 0x02);
	EXPECT_EQ(bytes[3], 0x01);
}

TEST(Endian, SingleByteIsUnchanged)
{
	const std::uint8_t value = 0xAB;

	EXPECT_EQ(sl::endian::to_little(value), value);
	EXPECT_EQ(sl::endian::from_little(value), value);
}
