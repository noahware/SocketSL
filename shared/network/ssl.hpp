#pragma once
#include <boost/asio/ssl.hpp>

#include <span>

namespace sl
{
	class ssl_context
	{
	public:
		enum class crypto_file_format : std::uint8_t
		{
			asn1,
			pem
		};

		ssl_context() = default;
		virtual ~ssl_context() = default;

		virtual void disable_peer_verification() = 0;
		virtual void require_peer_verification() = 0;

		virtual void load_verify_file(const std::string& path_to_file) = 0;
		virtual void add_certificate_authority(std::span<std::uint8_t> buffer) = 0;

		virtual void use_tmp_dh_file(const std::string& path_to_file) = 0;
		virtual void use_tmp_dh(std::span<std::uint8_t> buffer) = 0;

		virtual void use_certificate(const std::string& path_to_certificate, crypto_file_format file_format) = 0;
		virtual void use_certificate(std::span<std::uint8_t> buffer, crypto_file_format file_format) = 0;

		virtual void use_private_key(const std::string& path_to_key, crypto_file_format file_format) = 0;
		virtual void use_private_key(std::span<std::uint8_t> buffer, crypto_file_format file_format) = 0;
	};

	class boost_ssl_context final : public ssl_context
	{
	public:
		using asio_ssl_type = boost::asio::ssl::context;
		using ssl_method_type = boost::asio::ssl::context::method;
		using ssl_options_type = boost::asio::ssl::context::options;
		using ssl_file_format_type = boost::asio::ssl::context::file_format;

		boost_ssl_context() = delete;

		explicit boost_ssl_context(const ssl_method_type ssl_method)
				:	native_handle_(std::make_unique<asio_ssl_type>(ssl_method)) { }

		void disable_peer_verification() override;
		void require_peer_verification() override;

		void load_verify_file(const std::string& path_to_file) override;
		void add_certificate_authority(std::span<std::uint8_t> buffer) override;

		void use_tmp_dh_file(const std::string& path_to_file) override;
		void use_tmp_dh(std::span<std::uint8_t> buffer) override;

		void use_certificate(const std::string& path_to_certificate, crypto_file_format file_format) override;
		void use_certificate(std::span<std::uint8_t> buffer, crypto_file_format file_format) override;

		void use_private_key(const std::string& path_to_key, crypto_file_format file_format) override;
		void use_private_key(std::span<std::uint8_t> buffer, crypto_file_format file_format) override;

		void set_options(ssl_options_type options);
		void clear_options(ssl_options_type options);

		[[nodiscard]] asio_ssl_type& native_handle() const noexcept;

	protected:
		[[nodiscard]] static ssl_file_format_type ssl_file_format(crypto_file_format file_format) noexcept;

		std::unique_ptr<asio_ssl_type> native_handle_;
	};
}
