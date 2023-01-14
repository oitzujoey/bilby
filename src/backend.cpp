#include "backend.hpp"
#include <cstddef>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <system_error>
#include <vector>
#include <iostream>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include "parser.hpp"

namespace Backend {
	using namespace llvm;
	static std::unique_ptr<LLVMContext> llvmContext;
	static std::unique_ptr<Module> llvmModule;
	static std::unique_ptr<IRBuilder<>> irBuilder;
	static std::unique_ptr<legacy::FunctionPassManager> llvmFpm;

	Type *toType(std::string typeName) {
		if (typeName == "ui64") {
			return Type::getInt64Ty(*llvmContext);
		}
		else if (typeName == "ui32") {
			return Type::getInt32Ty(*llvmContext);
		}
		else if (typeName == "ui16") {
			return Type::getInt16Ty(*llvmContext);
		}
		else if (typeName == "ui8") {
			return Type::getInt8Ty(*llvmContext);
		}
		else {
			return nullptr;
		}
	}

	Value *generateForm(Parser::Form form);

	Value *generateInteger(Parser::Form form) {
		return ConstantInt::get(*llvmContext, APInt(32, form.integer));
	}

	Value *generateProgn(Parser::Forms forms) {
		Value *returnValue = nullptr;
		Function *function = irBuilder->GetInsertBlock()->getParent();
		BasicBlock *progn = BasicBlock::Create(*llvmContext, "progn", function);
		BasicBlock *endProgn = BasicBlock::Create(*llvmContext, "prognCont");
		irBuilder->SetInsertPoint(progn);
		irBuilder->CreateBr(progn);

		for (std::ptrdiff_t i = 1; i < forms.size(); i++) {
			returnValue = generateForm(forms[i]);
		}
		progn = irBuilder->GetInsertBlock();
		function->getBasicBlockList().push_back(endProgn);
		irBuilder->SetInsertPoint(endProgn);
		return Constant::getNullValue(Type::getInt32Ty(*llvmContext));
	}

	Function *generateExternFunction(std::string keyword, Parser::Forms forms) {
		if (forms.size() != 3) {
			// Error.
		}
		auto nameForm = forms[1];
		if (nameForm.type != Parser::IDENTIFIER) {
			// Error.
		}
		auto name = *nameForm.identifier;
		auto pattern = forms[2];
		if (pattern.type != Parser::FORM) {
			// Error.
		}
		if (pattern.typeAnnotation == nullptr) {
			// Error.
		}
		auto patternTypeAnnotation = *pattern.typeAnnotation;
		if (patternTypeAnnotation.type != Parser::IDENTIFIER) {
			// Error.
		}
		auto returnType = toType(*patternTypeAnnotation.identifier);
		auto parameters = *pattern.forms;
		std::vector<Type *> parameterTypes({});
		for (auto &parameter: parameters) {
			if (parameter.typeAnnotation == nullptr) {
				// Error.
			}
			auto typeAnnotation = *parameter.typeAnnotation;
			Type *type;
			if (typeAnnotation.type != Parser::IDENTIFIER) {
				// Error.
			}
			auto typeName = *typeAnnotation.identifier;
			type = toType(typeName);
			parameterTypes.push_back(type);
		}
		FunctionType *functionType = FunctionType::get(returnType, parameterTypes, false);
		Function *function = Function::Create(functionType, Function::ExternalLinkage, name, llvmModule.get());

		std::ptrdiff_t index = 0;
		for (auto &arg: function->args()) {
			auto parameter = parameters[index];
			if (parameter.type != Parser::IDENTIFIER) {
				// Error.
			}
			auto identifier = *parameter.identifier;
			arg.setName(identifier);
		}

		return function;
	}

	Value *generateExtern(std::string keyword, Parser::Forms forms) {
		Value *value = nullptr;
		if (forms.size() != 1) {
			// Error.
		}
		Parser::Form prototypeForm = forms[1];
		if (prototypeForm.type != Parser::FORM) {
			// Error.
		}
		auto prototype = *prototypeForm.forms;
		if (prototype.size() == 0) {
			// Error.
		}
		{
			auto keywordForm = prototype[0];
			if (keywordForm.type != Parser::IDENTIFIER) {
				// Error.
			}
			auto keyword = *keywordForm.identifier;
			if (keyword == "defun") {
				value = generateExternFunction(keyword, prototype);
			}
			else {
				// Error.
			}
		}
		return value;
	}

	Value *generateCall(std::string name, Parser::Forms forms) {
		Function *calleeFunction = llvmModule->getFunction(name);
		if (calleeFunction == nullptr) {
			// Error.
			return nullptr;
		}
		else {
			if (calleeFunction->arg_size() != (forms.size() - 1)) {
				// Error.
			}
			std::vector<Value *> args;
			for (std::ptrdiff_t i = 1, top = forms.size(); i < top; i++){
				auto form = forms[i];
				auto arg = generateForm(form);
				if (arg == nullptr) {
					std::cout << "generateCall arg null" << std::endl;
					// Error.
				}
				args.push_back(arg);
			}
			return irBuilder->CreateCall(calleeFunction, args, "calltmp");
		}
	}

	Function *generateFunction(std::string keyword, Parser::Forms forms) {
		Value *value = nullptr;
		if (forms.size() < 3) {
			// Error.
		}
		auto nameForm = forms[1];
		if (nameForm.type != Parser::IDENTIFIER) {
			// Error.
		}
		auto name = *nameForm.identifier;
		// Bug: Type of declaration may not match type of definition.
		Function *function = llvmModule->getFunction(name);
		if (function == nullptr) {
			function = generateExternFunction(keyword, forms);
		}
		if (function == nullptr) {
			// Error.
		}
		if (!function->empty()) {
			// Error.
		}
		BasicBlock *functionBlock = BasicBlock::Create(*llvmContext, "entry", function);
		irBuilder->SetInsertPoint(functionBlock);

		for (std::ptrdiff_t i = 1; i < forms.size(); i++) {
			value = generateForm(forms[i]);
		}
		if (value == nullptr) {
			function->eraseFromParent();
			// Error.
			return nullptr;
		}
		irBuilder->CreateRet(value);
		verifyFunction(*function);
		llvmFpm->run(*function);
		return function;
	}

	Value *generateToplevel(std::string keyword, Parser::Forms forms) {
		Value *value = nullptr;
		if (forms.size() < 1) {
			// Error.
		}
		for (std::ptrdiff_t i = 1; i < forms.size(); i++) {
			value = generateForm(forms[i]);
			if (value == nullptr) {
				std::cout << "Form returned null." << std::endl;
			}
			else {
				value->print(errs());
			}
		}
		return value;
	}

	Value *generateForm(Parser::Form form) {
		Value *value = nullptr;
		if (form.type == Parser::INTEGER) {
			value = generateInteger(form);
		}
		else if (form.type == Parser::FORM) {
			if (form.forms->size() > 0) {
				auto forms = *form.forms;
				auto keywordForm = forms[0];
				if (keywordForm.type == Parser::IDENTIFIER) {
					std::string keyword = *keywordForm.identifier;
					if (keyword == "toplevel") {
						value = generateToplevel(keyword, forms);
					}
					else if (keyword == "progn") {
						value = generateProgn(forms);
					}
					else if (keyword == "extern") {
						value = generateExtern(keyword, forms);
					}
					else if (keyword == "defun") {
						value = generateFunction(keyword, forms);
					}
					else {
						value = generateCall(keyword, forms);
					}
				}
				else {
					// Error.
				}
			}
		}
		return value;
	}

	void generate(Parser::Form form) {
		std::cout << "--------------------------------------------------------------------------------" << std::endl;
		llvmContext = std::make_unique<LLVMContext>();
		llvmModule = std::make_unique<Module>("Bilby", *llvmContext);
		irBuilder = std::make_unique<IRBuilder<>>(*llvmContext);
		llvmFpm = std::make_unique<legacy::FunctionPassManager>(llvmModule.get());
		llvmFpm->add(createInstructionCombiningPass());
		llvmFpm->add(createReassociatePass());
		llvmFpm->add(createGVNPass());
		llvmFpm->add(createCFGSimplificationPass());
		llvmFpm->doInitialization();
		auto value = generateForm(form);
		std::cout << "--------------------------------------------------------------------------------" << std::endl;
		if (value == nullptr) {
			std::cout << "Top level returned null." << std::endl;
		}
		else {
			value->print(errs());
		}
		auto targetTriple = sys::getDefaultTargetTriple();
		InitializeAllTargetInfos();
		InitializeAllTargets();
		InitializeAllTargetMCs();
		InitializeAllAsmParsers();
		InitializeAllAsmPrinters();
		std::string errorString;
		auto target = TargetRegistry::lookupTarget(targetTriple, errorString);
		if (target == nullptr) {
			errs() << errorString;
			return;
		}
		auto cpu = "generic";
		auto features = "";
		TargetOptions targetOptions;
		auto relocModel = Optional<Reloc::Model>();
		auto targetMachine = target->createTargetMachine(targetTriple, cpu, features, targetOptions, relocModel);
		llvmModule->setDataLayout(targetMachine->createDataLayout());
		llvmModule->setTargetTriple(targetTriple);
		auto filename = "output.o";
		std::error_code errorCode;
		raw_fd_ostream dest(filename, errorCode, sys::fs::OF_None);
		if (errorCode) {
			errs() << "Could not open file: " << errorCode.message();
			return;
		}
		legacy::PassManager pass;
		auto fileType = CGFT_ObjectFile;
		if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
			errs() << "TargetMachine can emit a file of this type.";
			return;
		}
		pass.run(*llvmModule);
		dest.flush();
	}
}
