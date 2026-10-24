#include "we/core/mapped_file.hpp"

#include <cerrno>
#include <cstring>
#include <string_view>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace we::core {
namespace {

[[nodiscard]] std::string make_system_message (
	std::string_view action, const std::filesystem::path& path, int error_number) {
	std::string message (action);
	message += " '";
	message += path.string ();
	message += "': ";
	message += std::strerror (error_number);

	return message;
}

} // namespace

MappedFile::~MappedFile () { this->reset (); }

MappedFile::MappedFile (MappedFile&& other) noexcept
	: data_ (std::exchange (other.data_, nullptr))
	, size_ (std::exchange (other.size_, 0))
	, mapped_ (std::exchange (other.mapped_, false))
	, owned_ (std::move (other.owned_)) { }

MappedFile& MappedFile::operator= (MappedFile&& other) noexcept {
	if (this != &other) {
		this->reset ();
		this->data_ = std::exchange (other.data_, nullptr);
		this->size_ = std::exchange (other.size_, 0);
		this->mapped_ = std::exchange (other.mapped_, false);
		this->owned_ = std::move (other.owned_);
	}

	return *this;
}

void MappedFile::reset () noexcept {
	if (this->mapped_ && this->data_ != nullptr) {
		::munmap (const_cast<std::byte*> (this->data_), this->size_);
	}

	this->data_ = nullptr;
	this->size_ = 0;
	this->mapped_ = false;
	this->owned_.clear ();
}

std::expected<MappedFile, Error> MappedFile::open (const std::filesystem::path& path) {
	const int descriptor = ::open (path.c_str (), O_RDONLY | O_CLOEXEC);

	if (descriptor < 0) {
		return fail (ErrorCode::io, make_system_message ("cannot open", path, errno));
	}

	struct stat metadata {};

	if (::fstat (descriptor, &metadata) != 0) {
		const int error_number = errno;
		::close (descriptor);
		return fail (ErrorCode::io, make_system_message ("cannot stat", path, error_number));
	}

	if (!S_ISREG (metadata.st_mode)) {
		::close (descriptor);
		return fail (ErrorCode::io, "not a regular file: '" + path.string () + "'");
	}

	if (metadata.st_size <= 0) {
		::close (descriptor);
		return MappedFile {};
	}

	const auto size = static_cast<std::size_t> (metadata.st_size);
	void* mapping = ::mmap (nullptr, size, PROT_READ, MAP_PRIVATE, descriptor, 0);
	const int error_number = errno;
	::close (descriptor);

	if (mapping == MAP_FAILED) {
		return fail (ErrorCode::io, make_system_message ("cannot map", path, error_number));
	}

	MappedFile result;
	result.data_ = static_cast<const std::byte*> (mapping);
	result.size_ = size;
	result.mapped_ = true;

	return result;
}

MappedFile MappedFile::from_owned (std::vector<std::byte> data) {
	MappedFile result;
	result.size_ = data.size ();
	result.owned_ = std::move (data);
	result.data_ = result.owned_.data ();

	return result;
}

std::span<const std::byte> MappedFile::bytes () const noexcept {
	return std::span<const std::byte> (this->data_, this->size_);
}

} // namespace we::core
