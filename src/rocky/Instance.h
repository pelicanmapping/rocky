/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/IOTypes.h>
#include <rocky/Log.h>
#include <unordered_map>

namespace ROCKY_NAMESPACE
{
    class ROCKY_EXPORT Instance : public Inherit<Object, Instance>
    {
    public:
        //! Use Instance::create to construct
        Instance();

        // destructor
        virtual ~Instance();

        //! Default IO options
        inline IOOptions& ioOptions();

        //! Global caching policy
        inline CachePolicy& cachePolicy();

        //! Logging service
        inline Log& log();

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

        shared_ptr<Object> read(const Config& conf) const;

    private:
        std::unordered_map<std::string, ConfigFactory> _configFactories;
        std::unordered_map<std::string, ContentFactory> _contentFactories;
        CachePolicy _cachePolicy;
        IOOptions _ioOptions;
        Log _log;
    };


    // inlines ...

    CachePolicy& Instance::cachePolicy() {
        return _cachePolicy;
    }
    IOOptions& Instance::ioOptions() {
        return _ioOptions;
    }
    Log& Instance::log() {
        return ioOptions().services().log();
    }
}
