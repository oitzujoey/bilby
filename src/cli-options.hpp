#include <vector>
#include <string>

namespace CliOptions {
	struct Option {
		std::string key;
		std::string value;
		bool valid;
		Option() {
			this->key = "";
			this->value = "";
			this->valid = true;
		};
		Option(const std::string key, const std::string value) {
			this->key = key;
			this->value = value;
			this->valid = true;
		};
	};
	typedef std::vector<Option> Options;
	Options parse(int argc, char *argv[]);
	Option option_get(const Options options, const std::string key);
	std::string option_toString(Option option);
	std::string options_toString(Options options);
}
