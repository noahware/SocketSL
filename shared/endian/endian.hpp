#pragma once
#include <boost/endian/conversion.hpp>

namespace endian
{
	template <class t>
	t to_little(t x)
	{
		return boost::endian::little_to_native(x);
	}

	template <class t>
	t from_little(t x)
	{
		return boost::endian::native_to_little(x);
	}
}
