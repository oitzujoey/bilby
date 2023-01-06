#include "cli-options.hpp"
#include <cstddef>
#include <functional>

namespace CliOptions {

	typedef std::function<Options(int argc, char *argv[])> ParseLambda;

	ParseLambda parseToken(char *arg) {
		static auto cons = [](Option option, Options options) -> Options {
			options.push_back(option);
			return options;
		};
		static ParseLambda error = [](int argc, char *argv[]) -> Options {
			auto option = Option();
			option.valid = false;
			return Options({option});
		};
		static ParseLambda empty = [](int argc, char *argv[]) -> Options {
			return cons(Option(), parse(argc, argv));
		};
		static ParseLambda parseSingle = [](int argc, char *argv[]) -> Options {
			const char *arg = argv[0];
			const char key[2] = {arg[1], '\0'};
			const char *value = &arg[2];
			return cons(Option(key, value), parse(argc, argv));
		};
		static ParseLambda parseDouble = [](int argc, char *argv[]) -> Options {
			const char *arg = argv[0];
			std::string key = &arg[2];
			const char *value = "";
			for (std::ptrdiff_t i = 0; arg[i] != '\0'; i++) {
				if (arg[i] == '=') {
					key = std::string(&arg[2], i - 2);
					value = &arg[i + 1];
					break;
				}
			}
			return cons(Option(key, value), parse(argc, argv));
		};
		static ParseLambda parseArg = [](int argc, char *argv[]) -> Options {
			const char *arg = argv[0];
			const char *value = &arg[0];
			return cons(Option("", value), parse(argc, argv));
		};

		if (arg == nullptr) {
			return error;
		}
		else {
			if (arg[0] == '\0') {
				return empty;
			}
			else if (arg[0] == '-') {
				if (arg[1] == '\0') {
					return empty;
				}
				else if (arg[1] == '-') {
					if (arg[2] == '\0') {
						return empty;
					}
					else {
						return parseDouble;
					}
				}
				else {
					return parseSingle;
				}
			}
			else {
				return parseArg;
			}
		}
	}

	Options parse(int argc, char *argv[]) {
		if (argc > 0) {
			return parseToken(argv[1])(argc - 1, &argv[1]);
		}
		else {
			return Options({});
		}
	}

	Option option_get(const Options options, const std::string key) {
		for (auto &option: options) {
			if (option.valid && (key == option.key)) {
				return option;
			}
		}
		auto option = Option();
		option.valid = false;
		return option;
	}

	std::string option_toString(Option option) {
		std::string s = "";
		s += "{";
		s += "key: \"";
		s += option.key;
		s += "\", ";
		s += "value: \"";
		s += option.value;
		s += "\", ";
		s += "valid: ";
		s += option.valid ? "true" : "false";
		s += "}";
		return s;
	}

	std::string options_toString(Options options) {
		std::string s = "";
		for (auto option: options) {
			s += option_toString(option);
			s += "\n";
		}
		return s;
	}
}
