#pragma once
#include <boost/asio.hpp>

#include "ssl.hpp"

#include <chrono>
#include <span>
#include <type_traits>

namespace sl
{
	using async_callback_t = std::function<void(bool is_valid)>;

	class socket
	{
	public:
		using timeout_duration = std::chrono::steady_clock::duration;

		enum class handshake_type : std::uint8_t
		{
			client,
			server
		};

		socket() = default;
		virtual ~socket() = default;

		virtual void set_timeout(timeout_duration timeout) = 0;

		[[nodiscard]] virtual bool connect(std::string_view host, std::string_view service) = 0;
		[[nodiscard]] virtual bool connect(std::uint32_t ipv4_address, std::uint16_t port) = 0;
		virtual void async_connect(std::string_view host, std::string_view service, const async_callback_t& handler) = 0;
		virtual void close() = 0;

		[[nodiscard]] virtual bool handshake(handshake_type type) = 0;
		virtual void async_handshake(handshake_type type, const async_callback_t& handler) = 0;

		void erase(std::size_t size);
		void async_erase(std::size_t size, const async_callback_t& handler);

		[[nodiscard]] virtual bool read(std::span<std::uint8_t> buffer) = 0;
		virtual void async_read(std::span<std::uint8_t> buffer, const async_callback_t& handler) = 0;

		[[nodiscard]] virtual bool write(std::span<const std::uint8_t> buffer) = 0;
		virtual void async_write(std::span<const std::uint8_t> buffer, const async_callback_t& handler) = 0;

		[[nodiscard]] virtual std::uint32_t ipv4_address() const = 0;
		[[nodiscard]] virtual std::uint16_t port() const = 0;

		template <class T>
			requires std::is_trivially_copyable_v<T>
		[[nodiscard]] bool read(T& value)
		{
			return this->read({reinterpret_cast<std::uint8_t*>(&value), sizeof(T)});
		}

		template <class T>
			requires std::is_trivially_copyable_v<T>
		[[nodiscard]] bool write(T value)
		{
			return this->write({reinterpret_cast<const std::uint8_t*>(&value), sizeof(T)});
		}
	};

	class boost_tcp_socket final : public socket
	{
	public:
		using executor_type = boost::asio::any_io_executor;
		using resolver_type = boost::asio::ip::tcp::resolver;
		using asio_socket_type = boost::asio::ip::tcp::socket;
		using asio_stream_type = boost::asio::ssl::stream<asio_socket_type>;
		using asio_handshake_type = boost::asio::ssl::stream_base::handshake_type;
		using asio_endpoint_type = asio_socket_type::endpoint_type;

		explicit boost_tcp_socket(executor_type executor, std::shared_ptr<boost_ssl_context> ssl_context)
			:	executor_(std::move(executor)),
				ssl_context_(std::move(ssl_context)),
				stream_(std::make_unique<asio_stream_type>(executor_, ssl_context_->native_handle())) {}

		explicit boost_tcp_socket(executor_type executor, asio_socket_type asio_socket, std::shared_ptr<boost_ssl_context> ssl_context)
			:	executor_(std::move(executor)),
				ssl_context_(std::move(ssl_context)),
				stream_(std::make_unique<asio_stream_type>(std::move(asio_socket), ssl_context_->native_handle())) {}

		void set_timeout(timeout_duration timeout) override;

		[[nodiscard]] bool connect(std::string_view host, std::string_view service) override;
		[[nodiscard]] bool connect(std::uint32_t ipv4_address, std::uint16_t port) override;
		void async_connect(std::string_view host, std::string_view service, const async_callback_t& handler) override;

		void close() override;

		[[nodiscard]] bool handshake(handshake_type type) override;
		void async_handshake(handshake_type type, const async_callback_t& handler) override;

		[[nodiscard]] bool read(std::span<std::uint8_t> buffer) override;
		void async_read(std::span<std::uint8_t> buffer, const async_callback_t& handler) override;

		[[nodiscard]] bool write(std::span<const std::uint8_t> buffer) override;
		void async_write(std::span<const std::uint8_t> buffer, const async_callback_t& handler) override;

		[[nodiscard]] std::uint32_t ipv4_address() const override;
		[[nodiscard]] std::uint16_t port() const override;

	protected:
		void start_deadline();
		void cancel_deadline();

		[[nodiscard]] std::optional<resolver_type::results_type> resolve_host(std::string_view host, std::string_view service) const;
		[[nodiscard]] asio_endpoint_type remote_endpoint() const;
		[[nodiscard]] asio_endpoint_type local_endpoint() const;

		[[nodiscard]] static asio_handshake_type to_asio_handshake_type(handshake_type type) noexcept;

		executor_type executor_;
		std::shared_ptr<boost_ssl_context> ssl_context_;
		std::unique_ptr<asio_stream_type> stream_;
		timeout_duration timeout_{};
		std::unique_ptr<boost::asio::steady_timer> timer_;
	};
}
