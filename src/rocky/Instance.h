/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/IOTypes.h>
#include <unordered_map>

namespace ROCKY_NAMESPACE
{
    class ROCKY_EXPORT Instance
    {
    public:
        //! Construct a new application instance.
        Instance();

        //! Copy constructor
        Instance(const Instance& rhs);

        // destructor
        virtual ~Instance();

        //! Default IO options
        inline IOOptions& ioOptions();

        //! Global caching policy
        inline CachePolicy& cachePolicy();

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

        //! Global application instance status - returns an error
        //! if the instance does not exist
        static const Status& status();

    private:
        struct Implementation
        {
            std::unordered_map<std::string, ConfigFactory> configFactories;
            std::unordered_map<std::string, ContentFactory> contentFactories;
            CachePolicy cachePolicy;
            IOOptions ioOptions;
        };
        shared_ptr<Implementation> _impl;
        static Status _global_status;
    };


    // inlines ...

    CachePolicy& Instance::cachePolicy() {
        return _impl->cachePolicy;
    }
    IOOptions& Instance::ioOptions() {
        return _impl->ioOptions;
    }
}
