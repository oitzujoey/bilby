#include "parser.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include "cli-options.hpp"
#include "macros.hpp"
#include "types.hpp"
#include "backend.hpp"

int main(int argc, char *argv[]) {
	// Read file.
	std::string source;
	{
		using namespace CliOptions;
		auto options = parse(argc, argv);
		auto version = option_get(options, "version");
		auto file = option_get(options, "file");
		if (!file.valid) {
			file = option_get(options, "f");
		}
		if (!file.valid) {
			std::cout << "No file given." << std::endl;
			return 1;
		}
		std::cout << "file " << file.value << std::endl;

		std::ifstream fileStream(file.value);
		std::stringstream fileStringStream;
		fileStringStream << fileStream.rdbuf();
		source = fileStringStream.str();
	}
	Parser::Form form;
	{
		using namespace Parser;
		auto status = parse(ParserStream(source));
		if (status.valid != SUCCESS) {
			std::cout << "ERROR" << std::endl;
			for (auto &error: status.errors) {
				std::cout << error << std::endl;
			}
			return 1;
		}
		form = status.form;
		std::cout << status.prettyPrint() << std::endl;
	}
	std::cout << form.toString() << std::endl;
	{
		using namespace Macros;
		expandAll();
	}
	{
		using namespace Types;
		resolveAll(form);
	}
	{
		using namespace Backend;
		generate(form);
	}

	return 0;
}
