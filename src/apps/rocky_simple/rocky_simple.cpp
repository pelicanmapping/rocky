/**
 * rocky c++
 * Copyright 2024-2034 Pelican Mapping
 * MIT License
 */
#include <rocky/vsg/Application.h>

#ifdef ROCKY_HAS_TMS
#include <rocky/TMSImageLayer.h>
#endif

#include <rocky/optional.h>

namespace ROCKY_NAMESPACE
{
    struct option_metadata
    {
        std::string name;
        std::string description;
        std::string type;
        std::string default_value;

        bool operator = (const option_metadata& rhs)
        {
            return name == rhs.name;
        }

        bool operator < (const option_metadata& rhs) const
        {
            return name < rhs.name;
        }
    };

    using class_options_metadata = std::set<option_metadata>;

    struct register_option
    {
        register_option(const char* name, const char* description, const char* type, const char* default_value, class_options_metadata& meta)
        {
            meta.emplace(option_metadata{ name, description, type, default_value });
        }
    };

    template<typename T> using option = rocky::optional<T>;
}

#define OE_OPTION_0_ARGS()
#define OE_OPTION_1_ARGS()
#define OE_OPTION_2_ARGS(TYPE, NAME) \
    private: \
      rocky::option< TYPE > _ ## NAME ; \
      rocky::register_option _register_ ## NAME = { #NAME, "", #TYPE, "", metadata }; \
    public: \
      rocky::option< TYPE >& NAME () { return _ ## NAME; } \
      const rocky::option< TYPE >& NAME () const { return _ ## NAME; }

#define OE_OPTION_3_ARGS(TYPE, NAME, DEFAULT_VALUE) \
    private: \
      rocky::option< TYPE > _ ## NAME {DEFAULT_VALUE}; \
      rocky::register_option _register_ ## NAME = { #NAME, "", #TYPE, #DEFAULT_VALUE, metadata }; \
    public: \
      rocky::option< TYPE >& NAME () { return _ ## NAME; } \
      const rocky::option< TYPE >& NAME () const { return _ ## NAME; }

#define OE_OPTION_4_ARGS(TYPE, NAME, DEFAULT_VALUE, DESCRIPTION) \
    private: \
      rocky::option< TYPE > _ ## NAME {DEFAULT_VALUE}; \
      rocky::register_option _register_ ## NAME = { #NAME, #DESCRIPTION, #TYPE, #DEFAULT_VALUE, metadata }; \
    public: \
      rocky::option< TYPE >& NAME () { return _ ## NAME; } \
      const rocky::option< TYPE >& NAME () const { return _ ## NAME; }

// https://stackoverflow.com/a/28074198
#define OE_OPTION_FUNC_CHOOSER(_f1, _f2, _f3, _f4, _f5, ...) _f5
#define OE_OPTION_FUNC_RECOMPOSER(ARGS) OE_OPTION_FUNC_CHOOSER ARGS
#define OE_OPTION_CHOOSE_FROM_ARG_COUNT(...) OE_OPTION_FUNC_RECOMPOSER((__VA_ARGS__, OE_OPTION_4_ARGS, OE_OPTION_3_ARGS, OE_OPTION_2_ARGS, OE_OPTION_1_ARGS, ))
#define OE_OPTION_NO_ARG_EXPANDER() ,,OE_OPTION_0_ARGS
#define OE_OPTION_MACRO_CHOOSER(...) OE_OPTION_CHOOSE_FROM_ARG_COUNT(OE_OPTION_NO_ARG_EXPANDER __VA_ARGS__ ())
#define OE_OPTION(...) OE_OPTION_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

namespace ROCKY_NAMESPACE
{
    struct HasOptions
    {
        class_options_metadata metadata;
    };

    class TestClass : public virtual HasOptions
    {
    public:
        OE_OPTION(int, age, 123, "User's age")
        OE_OPTION(std::string, greeting, "hello", "A greeting message");
        OE_OPTION(double, rainPerc, 0.0, "Percent chance of rain");
    };
}


int main(int argc, char** argv)
{
    rocky::TestClass test;

    for(auto& obj : test.metadata)
    {
        std::cout << "name=" << obj.name << " desc=" << obj.description << " default=" << obj.default_value << " type=" << obj.type  << std::endl;
    }
}
