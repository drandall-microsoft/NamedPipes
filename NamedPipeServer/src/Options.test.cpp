#include "Options.test.h"
#include "Options.h"
#include <sstream>


template<typename T>
struct OptionsTest {
    T request;
    std::stringstream errors;
    std::vector<std::string> arguments;
    bool success;

    template<typename...Args>
    OptionsTest(Args... args) : arguments(std::vector<std::string>{args...}) { 
        success = request.TryParseArgs(arguments, errors); 
    }
};

struct ExampleRequest : public OptionCollection {
    OPTION(bool, SomeBool, Required::True, "A required bool option");
    OPTION(float, SomeFloat, Required::False, "An optional float option");
};

bool TestParsingArguments() {
    auto test = OptionsTest<ExampleRequest>("SomeBool=true", "SomeFloat=3.14");
    if (!test.success) return false;
    if (test.request.SomeBool != true) return false;
    if (test.request.SomeFloat != 3.14f) return false;

    return true;
}

std::vector<std::string> GetValidChars() {
    return { "a", "b", "c" };
}

struct RestrictedExample : public OptionCollection {
    LIMITED_OPTION(char, LimitedChar, Required::True, "Expects a specific value", GetValidChars);
};

bool TestLimitedOption() {
    auto test = OptionsTest<RestrictedExample>("LimitedChar=d");
    if (test.success) return false;
    if (test.errors.str() != R"(value "d" is not in ["a","b","c"])") return false;

    auto test2 = OptionsTest<RestrictedExample>("LimitedChar=b");
    if (!test2.success) return false;
    if (test2.request.LimitedChar != 'b') return false;

    return true;
}

template<float Min, float Max>
bool RangeRestricted(const float& val, std::ostream& outErrors) {
    if (val < Min || val > Max) {
        outErrors << "Value " << val << " is outside the range [" << Min << ".." << Max << "]";
        return false;
    }
    return true;
}

template<float Max>
bool MaxValueRestricted(const float& val, std::ostream& outErrors) {
    if (val > Max) {
        return false;
    }
    return true;
}
bool FromZeroToOne(const float& val, std::ostream& outErrors) {
    if (val < 0.0f || val > 1.0f) {
        outErrors << "Value " << val << " is outside the range [0..1]";
        return false;
    }
    return true;
}

#define COMMA ,
struct ValidatedExample : public OptionCollection {
    //VALIDATED_OPTION(float, ExampleFloat, Required::True, "A range restricted float", FromZeroToOne);
    VALIDATED_OPTION(float, ExampleFloat, Required::True, "A float that must be between 0 and 1", RangeRestricted<0.0 COMMA 1.0>);
};

bool TestValidatedOption() {
    auto test = OptionsTest<ValidatedExample>("exampleFloat=0.5");
    if (!test.success) return false;
    if (test.request.ExampleFloat != 0.5f) return false;

    auto test2 = OptionsTest<ValidatedExample>("exampleFloat=1.5");
    if (test2.success) return false;
    if (test2.errors.str() != "Value 1.5 is outside the range [0..1]") return false;

    return true;
}

bool TestToParsableArgs() {
    ExampleRequest response;
    response.SomeBool = false;
    response.SomeFloat = 3.14f;

    auto parsableArgs = response.ToParsableArgs();
    if (parsableArgs.find("somebool") == parsableArgs.npos) return false;
    if (parsableArgs.find("somefloat=3.14") == parsableArgs.npos) return false;

    ExampleRequest r2;
    parsableArgs = r2.ToParsableArgs();
    if (parsableArgs.find("somebool") == parsableArgs.npos) return false;
    if (parsableArgs.find("somefloat") != parsableArgs.npos) return false;

    return true;
}
bool TestOptions() {
    if (!TestParsingArguments()) return false;
    if (!TestLimitedOption()) return false;
    if (!TestValidatedOption()) return false;
    if (!TestToParsableArgs()) return false;

    return true;
}
