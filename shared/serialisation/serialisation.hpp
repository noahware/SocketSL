#pragma once
#include <flatbuffers/flatbuffers.h>

#include <vector>
#include <span>

namespace sl::serialisation
{
	[[nodiscard]] static std::vector<std::uint8_t> builder_to_vector(const flatbuffers::FlatBufferBuilder& builder)
	{
		const std::uint8_t* const buffer = builder.GetBufferPointer();
		const std::size_t buffer_size = builder.GetSize();

		return { buffer, buffer + buffer_size };
	}

	template <class CreateFn, class ...Args>
	[[nodiscard]] static std::vector<std::uint8_t> serialise(flatbuffers::FlatBufferBuilder& builder, const CreateFn& create_fn, Args&&... args)
	{
		const auto request_header = create_fn(builder, std::forward<Args>(args)...);

		builder.Finish(request_header);

		return builder_to_vector(builder);
	}

	template <class CreateFn, class ...Args>
	[[nodiscard]] static std::vector<std::uint8_t> serialise(const CreateFn& create_fn, Args&&... args)
	{
		flatbuffers::FlatBufferBuilder builder;

		return serialise(builder, create_fn, std::forward<Args>(args)...);
	}

	template <class T>
	[[nodiscard]] static const T* deserialise(const void* const buffer) noexcept
	{
		return flatbuffers::GetRoot<T>(buffer);
	}

	template <class T>
	[[nodiscard]] static const T* deserialise(const std::span<std::uint8_t> buffer) noexcept
	{
		return deserialise<T>(buffer.data());
	}

	template <class T>
	[[nodiscard]] static bool is_valid(const void* const buffer, const std::size_t buffer_size)
	{
		flatbuffers::Verifier verifier(static_cast<const std::uint8_t*>(buffer), buffer_size);

		return verifier.VerifyBuffer<T>(nullptr);
	}

	template <class T>
	[[nodiscard]] static bool is_valid(const std::span<std::uint8_t> buffer)
	{
		return is_valid<T>(buffer.data(), buffer.size());
	}
}
