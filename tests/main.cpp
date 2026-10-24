#include <exception>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "support/harness.hpp"

namespace we::test {

std::vector<Case>& registry () {
	static std::vector<Case> cases;
	return cases;
}

Registrar::Registrar (std::string name, std::function<void ()> run) {
	registry ().push_back (Case {.name = std::move (name), .run = std::move (run)});
}

void fail_check (std::string_view expression, const char* file, int line) {
	throw Failure (std::string (file) + ":" + std::to_string (line) + ": " + std::string (expression) + " is false");
}

} // namespace we::test

int main () {
	std::size_t failures = 0;

	for (const auto& test : we::test::registry ()) {
		try {
			test.run ();
			std::print ("PASS {}\n", test.name);
		} catch (const we::test::Failure& failure) {
			++failures;
			std::print (stderr, "FAIL {}\n  {}\n", test.name, failure.what ());
		} catch (const std::exception& exception) {
			++failures;
			std::print (stderr, "FAIL {}\n  unexpected exception: {}\n", test.name, exception.what ());
		}
	}

	std::print ("\n{} tests, {} failed\n", we::test::registry ().size (), failures);
	return failures == 0 ? 0 : 1;
}
