#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>
#include <set>
#include <vector>

namespace Parser {

	class ParserStream {
		std::string string = "";
		std::ptrdiff_t index = 0;
		std::ptrdiff_t &parent_index;
		std::ptrdiff_t startLineNumber = 0;
		std::ptrdiff_t startColumnNumber = 0;
		std::ptrdiff_t lineNumber = 0;
		std::ptrdiff_t columnNumber = 0;
	public:
		ParserStream(std::string &string) : parent_index(index) {
			this->string = string;
		}
		ParserStream(ParserStream &stream) : parent_index(stream.index) {
			string = stream.string;
			index = stream.index;
			lineNumber = stream.lineNumber;
			columnNumber = stream.columnNumber;
			startLineNumber = stream.lineNumber;
			startColumnNumber = stream.columnNumber;
		}
		char isFinished() {
			return (index >= string.length());
		}
		char peek () {
			if (isFinished()) {
				// This is the string that never ends...
				return ' ';
			}
			return string[index];
		}
		char read() {
			char c = peek();
			index++;
			if (c == '\n') {
				lineNumber++;
				columnNumber = 0;
			}
			else {
				columnNumber++;
			}
			return c;
		}
		void consume() {
			parent_index = index;
			startLineNumber = lineNumber;
			startColumnNumber = columnNumber;
		}
		std::string coordsToString() {
			std::string s = "";
			s += std::to_string(startLineNumber + 1);
			s += ":";
			s += std::to_string(startColumnNumber + 1);
			return s;
		}
	};

	enum FormType {
		INTEGER,
		FORM,
		IDENTIFIER
	};

	struct Form {
		FormType type;
		Form *typeAnnotation = nullptr;
		union {
			uint64_t integer;
			std::vector<Form> *forms;
			std::string *identifier;
		};
		std::string prettyPrint() {
			
			std::string s = "";
			s += "{";
			s += "types: ";
			s += ((type == INTEGER)
			      ? "Integer"
			      : (type == FORM)
			      ? "Form"
			      : (type == IDENTIFIER)
			      ? "Identifier"
			      : "INVALID");
			s += ", ";
			s += ((type == INTEGER)
			      ? "integer"
			      : (type == FORM)
			      ? "form"
			      : (type == IDENTIFIER)
			      ? "identifier"
			      : "INVALID");
			s += ": ";
			if (type == INTEGER) {
				s += std::to_string(integer);
			}
			else if (type == FORM) {
				s += "[";
				bool first = true;
				for (auto &form: *forms) {
					if (first) {
						first = false;
					}
					else {
						s += " ";
					}
					s += form.prettyPrint();
				}
				s += "]";
			}
			else if (type == IDENTIFIER) {
				s += "'";
				s += *identifier;
			}
			else {
				s += "INVALID";
			}
			if (typeAnnotation != nullptr) {
				s += ", ";
				s += "typeAnnotation: ";
				s += typeAnnotation->prettyPrint();
			}
			s += "}";
			return s;
		};
		std::string toString() {
			std::string s = "";
			if (type == INTEGER) {
				s += std::to_string(integer);
			}
			else if (type == FORM) {
				s += "(";
				bool first = true;
				for (auto &form: *forms) {
					if (first) {
						first = false;
					}
					else {
						s += " ";
					}
					s += form.toString();
				}
				s += ")";
			}
			else if (type == IDENTIFIER) {
				s += *identifier;
			}
			else {
				s += "INVALID";
			}
			if (typeAnnotation != nullptr) {
				s += "::";
				s += typeAnnotation->toString();
			}
			return s;
		};
	};

	typedef std::vector<Form> Forms;

	enum ParserStatusValue {
		SUCCESS,
		NEXT,
		FAIL
	};

	struct ParserStatus {
		Form form;
		ParserStatusValue valid;
		std::vector<std::string> errors;
		ParserStatus() {
			valid = NEXT;
		}
		
		std::string prettyPrint() {
			std::string s = "";
			s += "{";
			s += "form: ";
			s += form.prettyPrint();
			s += ", ";
			s += "valid: ";
			s += ((valid == FAIL)
			      ? "FAIL"
			      : (valid == NEXT)
			      ? "NEXT"
			      : (valid == SUCCESS)
			      ? "SUCCESS"
			      : "INVALID");
			s += "}";
			return s;
		};
	};

	ParserStatus parse(ParserStream source);
}
