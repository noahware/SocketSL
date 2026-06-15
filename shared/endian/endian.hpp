#pragma once
#include <boost/endian/conversion.hpp>

namespace sl::endian
{
	template <class T>
	[[nodiscard]] T to_little(T x) noexcept
	{
		return boost::endian::native_to_little(x);
	}

	template <class T>
	[[nodiscard]] T from_little(T x) noexcept
	{
		return boost::endian::little_to_native(x);
	}
}
