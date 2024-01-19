#include "Options.h"
#include "StringUtils.h"
#include <algorithm>
#include <sstream>

using NamedParams = std::vector<std::pair<std::string, std::string>>;
NamedParams ParseNamedArguments(const std::vector<std::string>& args)
{
	NamedParams result;

	for (const auto& arg : args)
	{
		if (arg.find('=') == arg.npos)
		{
			continue;
		}

		auto split = StringUtils::Split(arg, "=");
		result.push_back(std::make_pair(StringUtils::ToLower(split[0]), std::string(split[1])));
	}

	return result;
}

OptionWrapper::OptionWrapper(std::string name, Required required, std::string helpText, ValidValuesFunc validValuesFunc)
	: Name(StringUtils::ToLower(name)), IsRequired(required), HelpText(helpText), GetValidValuesFunc(validValuesFunc) {
}

bool OptionCollection::TryParseArgs(const std::vector<std::string>& args, std::ostream& outErrors) {
	NamedParams params = ParseNamedArguments(args);
	if (args.size() != params.size()) {
		outErrors << "Argument was provided in the wrong format";
		return false;
	}

	for (const auto& [name, value] : params) {
		bool foundOption = false;
		for (auto& option : Options) {
			if (name == option->Name) {
				foundOption = true;
				if (!CheckValidValues(*option, value, outErrors)) {
					return false;
				}
				if (!option->TryParse(value, outErrors)) {
					return false;
				}
				option->Populated = true;
			}
		}
		if (!foundOption) {
			outErrors << "Unrecognized argument '" << name << "'";
			return false;
		}
	}

	if (!CheckRequiredValues(outErrors)) {
		return false;
	}

	return ValidateParsedArgs(outErrors);
}

bool OptionCollection::CheckValidValues(OptionWrapper& option, std::string_view value, std::ostream& outErrors) {
	if (option.GetValidValuesFunc) {
		auto validValues = option.GetValidValuesFunc();
		std::transform(validValues.begin(), validValues.end(), validValues.begin(), [](const auto& v) { return StringUtils::ToLower(v); });
		if (std::find(validValues.begin(), validValues.end(), value) == validValues.end()) {
			outErrors << "value \"" << value << "\" is not in [";
			for (const auto& v : validValues) {
				outErrors << "\"" << v << "\",";
			}
			outErrors.seekp(-1, std::ios_base::end);
			outErrors << ']';
			return false;
		}
	}
	return true;
}

bool OptionCollection::CheckRequiredValues(std::ostream& outErrors) {
	bool success = true;
	for (const auto& option : Options) {
		if (option->IsRequired == Required::True && !option->Populated) {
			outErrors << "Missing Required parameter: " << option->Name << "\n";
			success = false;
		}
	}

	return success;
}

std::string OptionCollection::Describe() {
	std::stringstream stream;
	for (const auto& option : Options) {
		stream << "Required=" << (option->IsRequired == Required::True ? "true" : "false")
			<< " Name=" << option->Name
			<< " HelpText=" << option->HelpText << std::endl;
	}

	return stream.str();
}

std::string OptionCollection::ToParsableArgs() {
	std::stringstream stream;
	
	for (const auto& option : Options) {
		if (option->IsRequired==Required::True || !option->HasDefaultValue()) {
			option->AppendParsableArg(stream);
			stream << " ";
		}
	}

	if (stream.peek()) {
		stream.seekp(-1, std::ios_base::end);
	}
	return stream.str();
}
