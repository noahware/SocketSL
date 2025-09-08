#pragma once
#include <random>
#include <span>

namespace random
{
	std::mt19937_64& get_engine()
	{
		thread_local std::mt19937_64 engine(std::random_device{}());

		return engine;
	}

	template <class t>
	t uniform_int(const t min, const t max)
	{
		return std::uniform_int_distribution<t>(min, max)(get_engine());
	}

	template <class list_t>
	auto list_iterator(list_t& list)
	{
		const std::uint64_t list_size = list.size();

		auto iterator = list.begin();

		if (list_size <= 1)
		{
			return iterator;
		}

		const std::uint64_t index = uniform_int<std::uint64_t>(0, list_size);

		std::advance(iterator, index);

		return iterator;
	}

	template <class t>
	std::span<t>::iterator span_iterator(const std::span<t>& list)
	{
		return list_iterator(list);
	}

	template <class t>
	t& span_entry(const std::span<t>& list)
	{
		const auto iterator = span_iterator(list);

		return *iterator;
	}
}
