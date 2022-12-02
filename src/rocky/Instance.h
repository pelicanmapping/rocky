/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/IOTypes.h> // replace 
#include <unordered_map>

namespace rocky
{
    class ROCKY_EXPORT Instance
    {
    public:
        // construct a new application instance
        Instance();

        // destructor
        ~Instance();

        optional_prop(CachePolicy, cachePolicy);

        using ContentFactory = std::function<
            shared_ptr<Object>(
                const std::string& contentType,
                const std::string& content)>;

        //! Add a factory that creates an object from a stream of data,
        //! (e.g., an image based on a mime-type)
        void addFactory(
            const std::string& contentType,
            ContentFactory factory);

        using ConfigFactory = std::function<
            shared_ptr<Object>(
                const Config& conf)>;

        //! Add a factory that creates an object from a Config
        //! (e.g., a Map or Layer)
        void addFactory(
            const std::string& name,
            ConfigFactory factory);

        shared_ptr<Object> create(const Config& conf) const;

    private:
        std::unordered_map<std::string, ConfigFactory> _configFactories;
        std::unordered_map<std::string, ContentFactory> _contentFactories;
    };
}
