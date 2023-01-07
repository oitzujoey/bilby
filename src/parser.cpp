#include "parser.hpp"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iostream>

/* Provides a function scope instead of a normal scope. This means you can return a value from it. */
#define SCOPE(SCOPE_body) (([&]() { SCOPE_body; })())
#define LOOP while (true)

namespace Parser {

	bool isSpecial(char c) {
		return (c == '(') || (c == ')') || (c == ':');
	}

	unsigned int decimalCharToDigit(char c) {
		return c - '0';
	}

	ParserStatus parseCompoundForm(ParserStream &source);

	ParserStatus parseWhitespace(ParserStream &source) {
		ParserStatus status;
		status.valid = SCOPE(
		                     char c;
		                     LOOP {
			                     c = source.peek();
			                     if (!std::isspace(c)) {
				                     return SUCCESS;
			                     }
			                     if (source.isFinished()) {
				                     return FAIL;
			                     }
			                     source.read();
		                     });
		source.consume();
		return status;
	}

	ParserStatus parseDecimal(ParserStream &source) {
		ParserStatus status;
		uint64_t integer = 0;

		// How to `goto` without using `goto`. ;)
		status.valid = SCOPE(
		                     char c = source.peek();
		                     if (!std::isdigit(c)) {
			                     return NEXT;
		                     }
		                     // Definitely a digit.
		                     source.read();
		                     integer = decimalCharToDigit(c);
		                     LOOP {
			                     c = source.peek();
			                     if (!std::isdigit(c)) {
				                     return SUCCESS;
			                     }
			                     uint64_t decimalDigit = decimalCharToDigit(c);
			                     auto newInteger = (integer * 10) + decimalDigit;
			                     if (((newInteger - decimalDigit) / 10) != integer) {
				                     status.errors.push_back(source.coordsToString()
				                                             + ": Integer larger than 64 bits.");
				                     return FAIL;
			                     }
			                     integer = newInteger;
			                     source.read();
		                     });

		if (status.valid == SUCCESS) {
			source.consume();
		}
		status.form.integer = integer;
		return status;
	}

	ParserStatus parseHexadecimal(ParserStream &source) {
		ParserStatus status;
		uint64_t integer = 0;
		status.valid = NEXT;
		if (status.valid == SUCCESS) {
			source.consume();
		}
		return status;
	}

	ParserStatus parseInt(ParserStream &source) {

		/* An integer is defined as matching this regex, [0-9]+, and having a value that fits in 64 bits.
		   Assume the target machine will use two's complement, even if it doesn't. */

		ParserStatus status;
		std::vector<ParserStatus(*)(ParserStream&)> parsers = {parseDecimal,
		                                                       parseHexadecimal};

		for (std::ptrdiff_t i = 0; i < parsers.size(); i++) {
			status = parsers[i](source);
			if (status.valid != NEXT) {
				break;
			}
		}

		if (status.valid == SUCCESS) {
			source.consume();
			status.form.type = FormType::INTEGER;
		}
		return status;
	}

	ParserStatus parseForm(ParserStream &source) {
		ParserStatus status;
		Forms *forms = new Forms();

		status.valid = SCOPE(
		                     char c;
		                     c = source.peek();
		                     if (c != '(') {
			                     return NEXT;
		                     }
		                     source.read();

		                     LOOP {
			                     status = parseWhitespace(source);
			                     if (source.isFinished()) {
				                     status.errors.push_back(source.coordsToString() + ": Unmatched parentheses.");
				                     return FAIL;
			                     }
			                     c = source.peek();
			                     if (c == ')') {
				                     source.read();
				                     return SUCCESS;
			                     }

			                     status = parseCompoundForm(source);
			                     if (status.valid != SUCCESS) {
				                     return FAIL;
			                     }
			                     forms->push_back(status.form);
		                     });

		if (status.valid == SUCCESS) {
			source.consume();
			status.form.forms = forms;
			status.form.type = FormType::FORM;
		}
		else {
			delete forms;
		}
		return status;
	}

	ParserStatus parseIdentifier(ParserStream &source) {
		ParserStatus status;

		std::string *identifier = new std::string("");

		status.valid = SCOPE(
		                     char c;
		                     c = source.peek();
		                     if (!std::isprint(c) || ((c != ':') && isSpecial(c))) {
			                     return NEXT;
		                     }
		                     // Definitely an identifier.
		                     source.read();
		                     *identifier += c;

		                     LOOP {
			                     c = source.peek();
			                     if (std::isspace(c) || isSpecial(c)) {
				                     return SUCCESS;
			                     }
			                     if (!std::isprint(c)) {
				                     return NEXT;
			                     }
			                     source.read();
			                     *identifier += c;
		                     });

		if (status.valid == SUCCESS) {
			source.consume();
			status.form.identifier = identifier;
			status.form.type = FormType::IDENTIFIER;
		}
		else {
			delete identifier;
		}
		return status;
	}

	ParserStatus parseCompoundForm(ParserStream &source) {
		ParserStatus status;
		auto parsers = {parseInt,
		                parseIdentifier,
		                parseForm};
		for (auto &parser: parsers) {
			status = parser(source);
			if (status.valid != NEXT) {
				break;
			}
		}
		if (status.valid == SUCCESS) {
			// Check for type annotations.
			status.valid = SCOPE(
			                     ParserStream source2{source};
			                     ParserStatus status2;
			                     char c;
			                     status2 = parseWhitespace(source2);
			                     if (status2.valid != SUCCESS) {
				                     // It's fine to end the form here.
				                     return SUCCESS;
			                     }
			                     c = source2.peek();
			                     if (c != ':') {
				                     // No annotation.
				                     return SUCCESS;
			                     }
			                     source2.read();
			                     c = source2.peek();
			                     if (c != ':') {
				                     // Might be a keyword argument.
				                     return SUCCESS;
			                     }
			                     source2.read();
			                     status2 = parseWhitespace(source2);
			                     if (status2.valid != SUCCESS) {
				                     status.errors.push_back(source.coordsToString() + ": Expected type annotation.");
				                     return FAIL;
			                     }
			                     status2 = parseCompoundForm(source2);
			                     if (status2.valid != SUCCESS) {
				                     return FAIL;
			                     }
			                     source2.consume();
			                     status.form.typeAnnotation = new Form();
			                     *status.form.typeAnnotation = status2.form;
			                     return SUCCESS;);
		}
		if (status.valid == SUCCESS) {
			source.consume();
		}
		return status;
	}

	ParserStatus parse(ParserStream source) {
		ParserStatus status;
		Forms *forms = new Forms();
		status.valid = SCOPE(
		                     LOOP {
			                     status = parseWhitespace(source);
			                     if (status.valid != SUCCESS) {
				                     // End of file, but all forms are complete.
				                     return SUCCESS;
			                     }
			                     status = parseCompoundForm(source);
			                     if (status.valid != SUCCESS) {
				                     return FAIL;
			                     }
			                     forms->push_back(status.form);
		                     });
		if (status.valid == SUCCESS) {
			source.consume();
			status.form.forms = forms;
			status.form.type = FormType::FORM;
		}
		else {
			delete forms;
		}
		return status;
	}
}
