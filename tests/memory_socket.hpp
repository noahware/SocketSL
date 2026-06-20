#pragma once
#include <network/socket.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace test
{
	// in-memory loopback socket for unit tests: write() appends to a buffer, read() consumes it
	// in FIFO order. lets us exercise the framing / send / recv / size-cap logic with no TLS or
	// real socket, by injecting a test double through the abstract sl::socket interface.
	class memory_socket final : public sl::socket
	{
	public:
		using sl::socket::read;
		using sl::socket::write;

		void set_timeout(timeout_duration) override {}

		void set_max_message_size(const std::size_t max_size) override
		{
			max_message_size_ = max_size;
		}

		[[nodiscard]] std::size_t max_message_size() const override
		{
			return max_message_size_;
		}

		void set_max_pending_writes(const std::size_t max_pending) override
		{
			max_pending_writes_ = max_pending;
		}

		[[nodiscard]] std::size_t max_pending_writes() const override
		{
			return max_pending_writes_;
		}

		[[nodiscard]] bool connect(std::string_view, std::string_view) override
		{
			return true;
		}

		[[nodiscard]] bool connect(std::uint32_t, std::uint16_t) override
		{
			return true;
		}

		void async_connect(std::string_view, std::string_view, const sl::async_callback_t& handler) override
		{
			if (handler)
			{
				handler(true);
			}
		}

		void close() override {}

		[[nodiscard]] bool handshake(handshake_type) override
		{
			return true;
		}

		void async_handshake(handshake_type, const sl::async_callback_t& handler) override
		{
			if (handler)
			{
				handler(true);
			}
		}

		[[nodiscard]] bool read(const std::span<std::uint8_t> buffer) override
		{
			if (read_pos_ + buffer.size() > buffer_.size())
			{
				return false;
			}

			std::copy_n(buffer_.begin() + static_cast<std::ptrdiff_t>(read_pos_), buffer.size(), buffer.begin());
			read_pos_ += buffer.size();

			return true;
		}

		void async_read(const std::span<std::uint8_t> buffer, const sl::async_callback_t& handler) override
		{
			const bool is_valid = read(buffer);

			if (handler)
			{
				handler(is_valid);
			}
		}

		[[nodiscard]] bool write(const std::span<const std::uint8_t> buffer) override
		{
			buffer_.insert(buffer_.end(), buffer.begin(), buffer.end());

			return true;
		}

		void async_write(const std::span<const std::uint8_t> buffer, const sl::async_callback_t& handler) override
		{
			const bool is_valid = write(buffer);

			if (handler)
			{
				handler(is_valid);
			}
		}

		[[nodiscard]] std::uint32_t ipv4_address() const override
		{
			return 0;
		}

		[[nodiscard]] std::uint16_t port() const override
		{
			return 0;
		}

		[[nodiscard]] std::string remote_address() const override
		{
			return "0.0.0.0";
		}

	private:
		std::vector<std::uint8_t> buffer_;
		std::size_t read_pos_ = 0;
		std::size_t max_message_size_ = sl::socket::default_max_message_size;
		std::size_t max_pending_writes_ = sl::socket::default_max_pending_writes;
	};
}
