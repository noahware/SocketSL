#pragma once
#include <flatbuffers/flatbuffers.h>

#include <vector>
#include <span>

namespace sl::serialisation
{
	[[nodiscard]] inline std::vector<std::uint8_t> builder_to_vector(const flatbuffers::FlatBufferBuilder& builder)
	{
		const std::uint8_t* const buffer = builder.GetBufferPointer();
		const std::size_t buffer_size = builder.GetSize();

		return { buffer, buffer + buffer_size };
	}

	template <class CreateFn, class ...Args>
	[[nodiscard]] std::vector<std::uint8_t> serialise(flatbuffers::FlatBufferBuilder& builder, const CreateFn& create_fn, Args&&... args)
	{
		const auto request_header = create_fn(builder, std::forward<Args>(args)...);

		builder.Finish(request_header);

		return builder_to_vector(builder);
	}

	template <class CreateFn, class ...Args>
	[[nodiscard]] std::vector<std::uint8_t> serialise(const CreateFn& create_fn, Args&&... args)
	{
		flatbuffers::FlatBufferBuilder builder;

		return serialise(builder, create_fn, std::forward<Args>(args)...);
	}

	// lift a flatc CreateX free function into a callable for serialise<>;
	// passing it as a template argument keeps the call site free of wrapper boilerplate
	template <auto CreateFn>
	[[nodiscard]] auto lift()
	{
		return []<class ...Args>(flatbuffers::FlatBufferBuilder& builder, Args&&... args)
		{
			return CreateFn(builder, std::forward<Args>(args)...);
		};
	}

	template <class T>
	[[nodiscard]] const T* deserialise(std::span<const std::uint8_t> buffer) noexcept
	{
		return flatbuffers::GetRoot<T>(buffer.data());
	}

	template <class T>
	[[nodiscard]] bool is_valid(std::span<const std::uint8_t> buffer)
	{
		flatbuffers::Verifier verifier(buffer.data(), buffer.size());

		return verifier.VerifyBuffer<T>(nullptr);
	}
}
