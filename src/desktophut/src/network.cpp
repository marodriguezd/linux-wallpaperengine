#include "we/desktophut/network.hpp"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <format>
#include <sys/wait.h>
#include <unistd.h>

namespace we::desktophut {

std::expected<std::string, core::Error> http_get (std::string_view url, std::chrono::seconds timeout) {
    if (url.empty ()) {
	return core::fail (core::ErrorCode::invalid_argument, "url cannot be empty");
    }

    int pipefd[2] = { -1, -1 };
    if (::pipe (pipefd) != 0) {
	return core::fail (core::ErrorCode::io, "failed to create pipe for curl");
    }

    const pid_t pid = ::fork ();
    if (pid < 0) {
	::close (pipefd[0]);
	::close (pipefd[1]);
	return core::fail (core::ErrorCode::io, "failed to fork curl process");
    }

    if (pid == 0) {
	// In child process
	::close (pipefd[0]);
	if (::dup2 (pipefd[1], STDOUT_FILENO) == -1) {
	    ::_exit (126);
	}
	::close (pipefd[1]);

	// Silence stderr
	const int null_fd = ::open ("/dev/null", O_WRONLY);
	if (null_fd != -1) {
	    ::dup2 (null_fd, STDERR_FILENO);
	    ::close (null_fd);
	}

	const std::string timeout_str = std::to_string (timeout.count ());
	const std::string url_str (url);

	std::array<const char*, 7> argv
	    = { "curl", "-fsSL", "--max-time", timeout_str.c_str (), url_str.c_str (), nullptr };

	::execvp ("curl", const_cast<char* const*> (argv.data ()));
	::_exit (127);
    }

    // In parent process
    ::close (pipefd[1]);

    std::string response;
    std::array<char, 8192> buffer {};
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read (pipefd[0], buffer.data (), buffer.size ())) > 0) {
	response.append (buffer.data (), static_cast<std::size_t> (bytes_read));
    }
    ::close (pipefd[0]);

    int status = 0;
    while (::waitpid (pid, &status, 0) < 0) {
	if (errno == EINTR) {
	    continue;
	}
	return core::fail (core::ErrorCode::io, "waitpid failed on curl process");
    }

    if (WIFEXITED (status)) {
	const int exit_code = WEXITSTATUS (status);
	if (exit_code != 0) {
	    return core::fail (
		core::ErrorCode::io, std::format ("curl failed with exit code {} for url: {}", exit_code, url)
	    );
	}
    } else {
	return core::fail (core::ErrorCode::io, "curl terminated abnormally");
    }

    return response;
}

std::expected<void, core::Error>
http_download (std::string_view url, const std::filesystem::path& destination, std::chrono::seconds timeout) {
    if (url.empty ()) {
	return core::fail (core::ErrorCode::invalid_argument, "url cannot be empty");
    }

    std::error_code ec;
    std::filesystem::create_directories (destination.parent_path (), ec);
    if (ec) {
	return core::fail (
	    core::ErrorCode::io,
	    std::format ("cannot create directory '{}': {}", destination.parent_path ().string (), ec.message ())
	);
    }

    std::filesystem::path temp_file = destination;
    temp_file += ".tmp";

    const pid_t pid = ::fork ();
    if (pid < 0) {
	return core::fail (core::ErrorCode::io, "failed to fork curl process");
    }

    if (pid == 0) {
	// Child process
	const int null_fd = ::open ("/dev/null", O_WRONLY);
	if (null_fd != -1) {
	    ::dup2 (null_fd, STDOUT_FILENO);
	    ::dup2 (null_fd, STDERR_FILENO);
	    ::close (null_fd);
	}

	const std::string timeout_str = std::to_string (timeout.count ());
	const std::string url_str (url);
	const std::string temp_str = temp_file.string ();

	std::array<const char*, 8> argv = { "curl", "-fsSL",           "--max-time",     timeout_str.c_str (),
					    "-o",   temp_str.c_str (), url_str.c_str (), nullptr };

	::execvp ("curl", const_cast<char* const*> (argv.data ()));
	::_exit (127);
    }

    // Parent process
    int status = 0;
    while (::waitpid (pid, &status, 0) < 0) {
	if (errno == EINTR) {
	    continue;
	}
	std::filesystem::remove (temp_file, ec);
	return core::fail (core::ErrorCode::io, "waitpid failed on curl process");
    }

    if (WIFEXITED (status)) {
	const int exit_code = WEXITSTATUS (status);
	if (exit_code != 0) {
	    std::filesystem::remove (temp_file, ec);
	    return core::fail (
		core::ErrorCode::io, std::format ("curl failed with exit code {} downloading {}", exit_code, url)
	    );
	}
    } else {
	std::filesystem::remove (temp_file, ec);
	return core::fail (core::ErrorCode::io, "curl terminated abnormally during download");
    }

    std::filesystem::rename (temp_file, destination, ec);
    if (ec) {
	std::filesystem::remove (temp_file, ec);
	return core::fail (
	    core::ErrorCode::io,
	    std::format ("failed to rename '{}' to '{}': {}", temp_file.string (), destination.string (), ec.message ())
	);
    }

    return {};
}

} // namespace we::desktophut
