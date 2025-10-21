#pragma once
#include <flatbuffers/flatbuffers.h>

#include <vector>
#include <span>

namespace serialisation
{
	static std::vector<std::uint8_t> builder_to_vector(const flatbuffers::FlatBufferBuilder& builder)
	{
		const std::uint8_t* const buffer = builder.GetBufferPointer();
		const std::uint64_t buffer_size = builder.GetSize();

		return { buffer, buffer + buffer_size };
	}

	template <class creation_function_t, class ...arguments_t>
	static std::vector<std::uint8_t> serialise(flatbuffers::FlatBufferBuilder& builder, const creation_function_t& creation_function, arguments_t&&... arguments)
	{
		const auto request_header = creation_function(builder, std::forward<arguments_t>(arguments)...);

		builder.Finish(request_header);

		return builder_to_vector(builder);
	}

	template <class creation_function_t, class ...arguments_t>
	static std::vector<std::uint8_t> serialise(const creation_function_t& creation_function, arguments_t&&... arguments)
	{
		flatbuffers::FlatBufferBuilder builder;

		return serialise(builder, creation_function, std::forward<arguments_t>(arguments)...);
	}

	template <class t>
	static const t* deserialise(const void* const buffer)
	{
		return flatbuffers::GetRoot<t>(buffer);
	}

	template <class t>
	static const t* deserialise(const std::span<std::uint8_t> buffer)
	{
		return deserialise<t>(buffer.data());
	}

	template <typename t>
	static std::uint8_t is_valid(const void* const buffer, const std::uint64_t buffer_size)
	{
		flatbuffers::Verifier verifier(static_cast<const std::uint8_t*>(buffer), buffer_size);

		return verifier.VerifyBuffer<t>(nullptr);
	}

	template<typename t>
	static std::uint8_t is_valid(const std::span<std::uint8_t> buffer)
	{
		return is_valid<t>(buffer.data(), buffer.size());
	}
}
