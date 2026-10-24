#pragma once

#include <functional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace we::test {

struct Failure : std::runtime_error {
	using std::runtime_error::runtime_error;
};

struct Case {
	std::string name;
	std::function<void ()> run;
};

[[nodiscard]] std::vector<Case>& registry ();

struct Registrar {
	Registrar (std::string name, std::function<void ()> run);
};

void fail_check (std::string_view expression, const char* file, int line);

template <typename T>
[[nodiscard]] std::string describe (const T& value) {
	if constexpr (requires (std::ostream& stream, const T& item) { stream << item; }) {
		std::ostringstream stream;
		stream << value;
		return stream.str ();
	} else {
		return "<unprintable>";
	}
}

template <typename Expected, typename Actual>
void check_eq (
	const Expected& expected, const Actual& actual, std::string_view expression, const char* file, int line) {
	if (!(expected == actual)) {
		std::ostringstream message;
		message << expression << " failed: expected '" << describe (expected) << "', got '" << describe (actual) << "'";
		throw Failure (std::string (file) + ":" + std::to_string (line) + ": " + message.str ());
	}
}

template <typename Actual>
void check_true (const Actual& value, std::string_view expression, const char* file, int line) {
	if (!static_cast<bool> (value)) {
		throw Failure (std::string (file) + ":" + std::to_string (line) + ": " + std::string (expression) + " is false");
	}
}

} // namespace we::test

#define TEST(Name)                                                                                                     \
	static void Name ();                                                                                               \
	static const ::we::test::Registrar registrar_##Name (#Name, Name);                                                 \
	static void Name ()

#define CHECK(condition)                                                                                               \
	do {                                                                                                               \
		if (!(condition)) {                                                                                            \
			::we::test::fail_check (#condition, __FILE__, __LINE__);                                                   \
		}                                                                                                              \
	} while (false)

#define CHECK_EQ(expected, actual)                                                                                     \
	::we::test::check_eq ((expected), (actual), #expected " == " #actual, __FILE__, __LINE__)

#define CHECK_TRUE(value) ::we::test::check_true ((value), #value, __FILE__, __LINE__)
