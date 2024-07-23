/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/IOTypes.h>
#include <unordered_map>
#include <set>

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

        //! Global application instance status - returns an error
        //! if the instance does not exist
        static const Status& status();

    public: // Object factory functions

        //! Object creation function that lets you create objects based on their name.
        //! Typical use is for deserializing polymorphic objects from JSON, like
        //! map layers.
        using ObjectFactory = std::function<
            shared_ptr<Object>(
                const std::string& JSON)>;

        //! Create an object based on a name and a JSON-serialized configuration
        template<class T>
        static shared_ptr<T> createObject(const std::string& name, const JSON& conf) {
            return std::dynamic_pointer_cast<T>(createObjectImpl(name, conf));
        }

        //! Global object factory map
        //! Use the ROCKY_ADD_OBJECT_FACTORY macro for bootstrap-time registration
        static std::unordered_map<std::string, ObjectFactory>& objectFactories();

        //! Informational
        static std::set<std::string>& about();

    private:
        struct Implementation
        {
            CachePolicy cachePolicy;
            IOOptions ioOptions;
        };
        shared_ptr<Implementation> _impl;
        static Status _global_status;
        static shared_ptr<Object> createObjectImpl(const std::string& name, const JSON& conf);
    };


    // inlines ...

    CachePolicy& Instance::cachePolicy() {
        return _impl->cachePolicy;
    }
    IOOptions& Instance::ioOptions() {
        return _impl->ioOptions;
    }

    // macro to install an object factory at startup time from a .cpp file.
    #define ROCKY_ADD_OBJECT_FACTORY(NAME, FUNC) \
        struct __ROCKY_OBJECTFACTORY_##NAME##_INSTALLER { \
            __ROCKY_OBJECTFACTORY_##NAME##_INSTALLER () { \
                ROCKY_NAMESPACE::Instance::objectFactories()[util::toLower(#NAME)] = FUNC; \
        } }; \
        __ROCKY_OBJECTFACTORY_##NAME##_INSTALLER __rocky_objectFactory_##NAME ;
}
