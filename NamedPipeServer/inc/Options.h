#pragma once

#include <string>
#include <vector>
#include <functional>
#include "ArgParse.h"
#include "StringUtils.h"

struct OptionCollection;

enum struct Required {
    True, False
};

using ValidValuesFunc = std::function<std::vector<std::string>()>;

struct OptionWrapper {
    OptionWrapper(std::string name, Required required, std::string helpText, ValidValuesFunc validValuesFunc);

    virtual ~OptionWrapper() = default;
    virtual bool TryParse(std::string_view sv, std::ostream& outErrors) = 0;
    virtual void AppendParsableArg(std::ostream& outStream) const = 0;
    virtual bool HasDefaultValue() const = 0;

    std::string Name{ "Unset" };
    Required IsRequired{ Required::False };
    std::string HelpText{ "Help Text Not Populated" };
    ValidValuesFunc GetValidValuesFunc{nullptr};

    bool Populated{ false };
};

template<typename T>
struct Option : public OptionWrapper {
    using ValueValidatorFunc = std::function<bool(const T&, std::ostream&)>;

    Option(std::string name, Required required, std::string helpText, T& backingField, ValidValuesFunc validValuesFunc, ValueValidatorFunc valueValidator)
        : OptionWrapper(name, required, helpText, validValuesFunc)
        , BackingField(backingField)
        , ValueValidator(valueValidator)
    {
    }

    bool TryParse(std::string_view sv, std::ostream& outErrors) override {
        if (ArgParse::TryParse(sv, BackingField, outErrors)) {
            if (ValueValidator) {
                return ValueValidator(BackingField, outErrors);
            }
            return true;
        }
        return false;
    }

    void AppendParsableArg(std::ostream& outStr) const override {
        outStr << Name << "=" << BackingField;
    }

    bool HasDefaultValue() const override {
        static T defaultValue{};
        return BackingField == defaultValue;
    }

    T& BackingField;
    ValueValidatorFunc ValueValidator{ nullptr };
};

struct OptionCollection {
    bool TryParseArgs(const std::vector<std::string>& args, std::ostream& outErrors);
    std::string Describe();
    std::string ToParsableArgs();

protected:
    virtual bool ValidateParsedArgs(std::ostream& outErrors) { return true; }

private:
    template<typename T>
    friend struct OptionCollectionRegistrar;
    std::vector<std::unique_ptr<OptionWrapper>> Options;

    bool CheckValidValues(OptionWrapper& option, std::string_view value, std::ostream& outErrors);
    bool CheckRequiredValues(std::ostream& outErrors);
};

template<typename T>
struct OptionCollectionRegistrar {
    OptionCollectionRegistrar(OptionCollection& parentCollection, std::string name, Required required, std::string helpText, T& backingField, ValidValuesFunc validValuesFunc, std::function<bool(const T&, std::ostream&)> valueValidator) {
        parentCollection.Options.push_back(std::make_unique<Option<T>>(name, required, helpText, backingField, validValuesFunc, valueValidator));
    }
};

#define _IMPL_OPTION(_type, _name, _required, _helpText, _validValuesFunc, _valueValidator) \
    static_assert(_required == Required::True || _required == Required::False, "Expected Required to be either Required::True or Required::False"); \
    static_assert(std::is_default_constructible_v<_type>, "Provided type must be default constructible"); \
    public: _type _name{}; \
    private: OptionCollectionRegistrar<_type> _name##Option { *this, #_name, _required, _helpText, _name, _validValuesFunc, _valueValidator }; \
    public:

#define OPTION(_type, _name, _required, _helpText) _IMPL_OPTION(_type, _name, _required, _helpText, nullptr, nullptr)
#define LIMITED_OPTION(_type, _name, _required, _helpText, _validValuesFunc) _IMPL_OPTION(_type, _name, _required, _helpText, _validValuesFunc, nullptr)
#define VALIDATED_OPTION(_type, _name, _required, _helpText, _valueValidator) _IMPL_OPTION(_type, _name, _required, _helpText, nullptr, _valueValidator)
