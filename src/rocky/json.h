/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "Common.h"
#include "Status.h"

//#if !defined(ROCKY_LIBRARY) && !defined(ROCKY_EXPOSE_JSON_FUNCTIONS)
//#error json.h is an internal header file; do not include it directly :)
//#endif

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace nlohmann
{
    // Support the optional<> construct
    // https://github.com/nlohmann/json#how-do-i-convert-third-party-types
    template <typename T>
    struct adl_serializer<rocky::optional<T>>
    {
        static void to_json(json& j, const rocky::optional<T>& opt)
        {
            if (opt.has_value())
                j = opt.value();
            else
                j = nullptr;
        }

        static void from_json(const json& j, rocky::optional<T>& opt)
        {
            if (!j.is_null())
                opt = j.get<T>();
        }
    };
}

namespace ROCKY_NAMESPACE
{
    // NOTE: try/catch is disabled here so we can make sure all our
    // json code is safe. If it throws exceptions that means we are
    // doing something wrong
#if 1
#define JSON_TRY try
#define JSON_CATCH catch(...)
#else
#define JSON_TRY if(true)
#define JSON_CATCH else
#endif

    //! extent json with a status so we can check for parsing errors
    struct json_parse_result : public json
    {
        Status status;
        json_parse_result() : json(), status(StatusOK) {}
        json_parse_result(const json& j) : json(j), status(StatusOK) {}
        json_parse_result(const Status& s) : json(), status(s) {}
        json_parse_result(const json& j, const Status& s) : json(j), status(s) {}
    };


    inline json_parse_result parse_json(const std::string& input) {
        if (!input.empty()) {
            JSON_TRY {
                return json::parse(input);
            }
            JSON_CATCH {
                return Status("JSON parsing error");
            }
        }
        return json::object();
    }

    template<class T>
    inline void set(json& obj, const char* name, const T& var) {
        if (obj != nullptr && obj.is_object()) {
            JSON_TRY {
                json j = var;
                if (!j.is_null())
                    obj[name] = j;
            }
            JSON_CATCH{ }
        }
    }

    template<class T>
    inline void set(json& obj, const T& var) {
        if (obj != nullptr && obj.is_object()) {
            JSON_TRY {
                obj = var;
            }
            JSON_CATCH{ }
        }
    }

    template<class T>
    inline bool get_to(const json& obj, const char* name, T& var)
    {
        if (name != nullptr && obj.is_object() && !obj.is_null())
        {
            JSON_TRY
            {
                auto i = obj.find(name);
                if (i != obj.end())
                {
                    i->get_to(var);
                    return true;
                }
            }
            JSON_CATCH{ }
        }
        return false;
    }

    template<class T>
    inline void get_to(const json& obj, T& var) {
        if (obj.is_object() && !obj.is_null()) {
            JSON_TRY {
                obj.get_to(var);
            }
            JSON_CATCH{}
        }
    }

    inline std::string get_string(const json& obj) {
        std::string temp;
        if (obj.is_string()) {
            JSON_TRY{
                obj.get_to(temp);
            }
            JSON_CATCH { }
        }
        return temp;
    }

    inline std::string to_string(const json& obj) {
        return obj.dump(-1, 32, false, nlohmann::json::error_handler_t::replace);
    }
            


#define ROCKY_DEFINE_JSON_SERIALIZERS(TYPE) \
    class TYPE ; \
    extern ROCKY_EXPORT void to_json(json& j, const TYPE& obj); \
    extern ROCKY_EXPORT void from_json(const json& j, TYPE& obj)

    ROCKY_DEFINE_JSON_SERIALIZERS(CachePolicy);
    ROCKY_DEFINE_JSON_SERIALIZERS(Color);
    ROCKY_DEFINE_JSON_SERIALIZERS(Angle);
    ROCKY_DEFINE_JSON_SERIALIZERS(Distance);
    ROCKY_DEFINE_JSON_SERIALIZERS(Duration);
    ROCKY_DEFINE_JSON_SERIALIZERS(Speed);
    ROCKY_DEFINE_JSON_SERIALIZERS(ScreenSize);
    ROCKY_DEFINE_JSON_SERIALIZERS(Profile);
    ROCKY_DEFINE_JSON_SERIALIZERS(ProxySettings);
    ROCKY_DEFINE_JSON_SERIALIZERS(DateTime);
    ROCKY_DEFINE_JSON_SERIALIZERS(SRS);
    ROCKY_DEFINE_JSON_SERIALIZERS(GeoExtent);
    ROCKY_DEFINE_JSON_SERIALIZERS(GeoPoint);
    ROCKY_DEFINE_JSON_SERIALIZERS(URI);
    ROCKY_DEFINE_JSON_SERIALIZERS(Viewpoint);
}

