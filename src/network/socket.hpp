#pragma once
#include <boost/asio.hpp>

#include "ssl.hpp"

#include <chrono>
#include <deque>
#include <mutex>
#include <span>
#include <string>
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

		// default caps; 0 = unlimited (see the setters below)
		static constexpr std::size_t default_max_message_size = 0;
		static constexpr std::size_t default_max_pending_writes = 0;

		socket() = default;
		virtual ~socket() = default;

		virtual void set_idle_timeout(timeout_duration timeout) = 0;
		virtual void set_heartbeat_timeout(timeout_duration timeout) = 0;
		virtual void set_handshake_timeout(timeout_duration timeout) = 0;

		// receive-side cap on a single message's header and body; 0 = unlimited
		virtual void set_max_message_size(std::size_t max_size) = 0;
		[[nodiscard]] virtual std::size_t max_message_size() const = 0;

		// cap on outbound messages buffered per connection (including the in-flight one); when the
		// queue is full further sends fail instead of growing memory. 0 = unlimited
		virtual void set_max_pending_writes(std::size_t max_pending) = 0;
		[[nodiscard]] virtual std::size_t max_pending_writes() const = 0;

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

		// scatter-gather: write head + body as one atomic operation (no interleaving with other writes)
		virtual void async_write(std::span<const std::uint8_t> head, std::span<const std::uint8_t> body, const async_callback_t& handler) = 0;

		[[nodiscard]] virtual std::uint32_t ipv4_address() const = 0;
		[[nodiscard]] virtual std::uint16_t port() const = 0;
		[[nodiscard]] virtual std::string remote_address() const = 0;

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
				stream_(std::make_unique<asio_stream_type>(std::move(asio_socket), ssl_context_->native_handle()))
		{
			capture_remote_endpoint();
		}

		void set_idle_timeout(timeout_duration timeout) override;
		void set_heartbeat_timeout(timeout_duration timeout) override;
		void set_handshake_timeout(timeout_duration timeout) override;
		void set_max_message_size(std::size_t max_size) override;
		[[nodiscard]] std::size_t max_message_size() const override;
		void set_max_pending_writes(std::size_t max_pending) override;
		[[nodiscard]] std::size_t max_pending_writes() const override;

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
		void async_write(std::span<const std::uint8_t> head, std::span<const std::uint8_t> body, const async_callback_t& handler) override;

		[[nodiscard]] std::uint32_t ipv4_address() const override;
		[[nodiscard]] std::uint16_t port() const override;
		[[nodiscard]] std::string remote_address() const override;

	protected:
		struct pending_write
		{
			std::span<const std::uint8_t> head;
			std::span<const std::uint8_t> body;
			async_callback_t handler;
		};

		// inactivity deadline: (re)armed whenever the peer makes progress (connect / handshake /
		// a completed read); if it fires, the peer has gone silent for the timeout and we close
		void reset_idle_timer();

		// heartbeat timer: on inactivity, ping once; if still no traffic next interval, close
		void reset_heartbeat_timer();
		void arm_heartbeat();

		// handshake deadline: armed for the duration of an async handshake, cancelled on completion;
		// if it fires, the peer stalled the handshake and we close
		void reset_handshake_timer();
		void cancel_handshake_timer();

		[[nodiscard]] std::optional<resolver_type::results_type> resolve_host(std::string_view host, std::string_view service) const;
		void capture_remote_endpoint();

		// drains write_queue_ one write at a time so async writes never overlap on the SSL stream
		void write_next();

		[[nodiscard]] static asio_handshake_type to_asio_handshake_type(handshake_type type) noexcept;

		executor_type executor_;
		std::shared_ptr<boost_ssl_context> ssl_context_;
		std::unique_ptr<asio_stream_type> stream_;
		timeout_duration idle_timeout_{};
		timeout_duration heartbeat_timeout_{};
		timeout_duration handshake_timeout_{};
		bool heartbeat_sent_ = false;
		std::size_t max_message_size_ = default_max_message_size;
		std::size_t max_pending_writes_ = default_max_pending_writes;
		std::unique_ptr<boost::asio::steady_timer> idle_timer_;
		std::unique_ptr<boost::asio::steady_timer> heartbeat_timer_;
		std::unique_ptr<boost::asio::steady_timer> handshake_timer_;
		asio_endpoint_type remote_endpoint_{};
		std::mutex write_mutex_;
		std::deque<pending_write> write_queue_;
	};
}
