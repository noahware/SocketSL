#pragma once
#include <boost/asio.hpp>

#include "ssl.hpp"

#include <span>
#include <type_traits>

namespace sl
{
	using async_callback_t = std::function<void(bool is_valid)>;

	class socket
	{
	public:
		enum class handshake_type : std::uint8_t
		{
			client,
			server
		};

		socket() = default;
		virtual ~socket() = default;

		[[nodiscard]] virtual bool connect(std::string_view host, std::string_view service) = 0;
		[[nodiscard]] virtual bool connect(std::uint32_t ipv4_address, std::uint16_t port) = 0;
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
		using asio_context_type = boost::asio::io_context;
		using resolver_type = boost::asio::ip::tcp::resolver;
		using asio_socket_type = boost::asio::ip::tcp::socket;
		using asio_stream_type = boost::asio::ssl::stream<asio_socket_type>;
		using asio_handshake_type = boost::asio::ssl::stream_base::handshake_type;
		using asio_endpoint_type = asio_socket_type::endpoint_type;

		explicit boost_tcp_socket(std::shared_ptr<asio_context_type> io_context, std::shared_ptr<boost_ssl_context> ssl_context)
			:	io_context_(std::move(io_context)),
				ssl_context_(std::move(ssl_context)),
				stream_(std::make_unique<asio_stream_type>(*io_context_, ssl_context_->native_handle())) {}

		explicit boost_tcp_socket(std::shared_ptr<asio_context_type> io_context, asio_socket_type asio_socket, std::shared_ptr<boost_ssl_context> ssl_context)
			:	io_context_(std::move(io_context)),
				ssl_context_(std::move(ssl_context)),
				stream_(std::make_unique<asio_stream_type>(std::move(asio_socket), ssl_context_->native_handle())) {}

		bool connect(std::string_view host, std::string_view service) override;
		bool connect(std::uint32_t ipv4_address, std::uint16_t port) override;

		void close() override;

		bool handshake(handshake_type type) override;
		void async_handshake(handshake_type type, const async_callback_t& handler) override;

		bool read(std::span<std::uint8_t> buffer) override;
		void async_read(std::span<std::uint8_t> buffer, const async_callback_t& handler) override;

		bool write(std::span<const std::uint8_t> buffer) override;
		void async_write(std::span<const std::uint8_t> buffer, const async_callback_t& handler) override;

		std::uint32_t ipv4_address() const override;
		std::uint16_t port() const override;

	protected:
		[[nodiscard]] std::optional<resolver_type::results_type> resolve_host(std::string_view host, std::string_view service) const;
		[[nodiscard]] asio_endpoint_type remote_endpoint() const;
		[[nodiscard]] asio_endpoint_type local_endpoint() const;

		[[nodiscard]] static asio_handshake_type to_asio_handshake_type(handshake_type type) noexcept;

		std::shared_ptr<asio_context_type> io_context_;
		std::shared_ptr<boost_ssl_context> ssl_context_;
		std::unique_ptr<asio_stream_type> stream_;
	};
}
