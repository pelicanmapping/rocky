/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/IOTypes.h>
#include <memory>
#include <functional>
#include <unordered_map>
#include <set>

namespace ROCKY_NAMESPACE
{
    class ROCKY_EXPORT ContextImpl
    {
    public:
        //! Copy constructor
        ContextImpl(const ContextImpl& rhs) = delete;
        ContextImpl(ContextImpl&&) noexcept = delete;

        // destructor
        virtual ~ContextImpl();

        //! Default IO options
        IOOptions io;

    public: // Object factory functions

        //! Object creation function that lets you create objects based on their name.
        //! Typical use is for deserializing polymorphic objects from JSON, like
        //! map layers.
        using ObjectFactory = std::function<
            std::shared_ptr<Object>(const std::string& JSON, const IOOptions& io)>;

        //! Create an object based on a name and a JSON-serialized configuration
        template<class T>
        static std::shared_ptr<T> createObject(const std::string& name, const std::string& JSON, const IOOptions& io) {
            return std::dynamic_pointer_cast<T>(createObjectImpl(name, JSON, io));
        }

        //! Global object factory map
        //! Use the ROCKY_ADD_OBJECT_FACTORY macro for bootstrap-time registration
        static std::unordered_map<std::string, ObjectFactory>& objectFactories();

        //! Informational
        static std::set<std::string>& about();

    protected:
        //! Construct a new application context.
        ContextImpl();

    private:
        //struct Implementation;
        //std::shared_ptr<Implementation> _impl;

        //static Status _global_status;
        static std::shared_ptr<Object> createObjectImpl(const std::string& name, const std::string& JSON, const IOOptions& io);

        friend class ContextFactory;
    };

    using Context = std::shared_ptr<ContextImpl>;

    class ContextFactory
    {
    public:
        template<typename... Args>
        static Context create(Args&&... args) {
            return Context(new ContextImpl(std::forward<Args>(args)...));
        };
    };
}



// macro to install an object factory at startup time from a .cpp file.
#define ROCKY_ADD_OBJECT_FACTORY(NAME, FUNC) \
        struct __ROCKY_OBJECTFACTORY_##NAME##_INSTALLER { \
            __ROCKY_OBJECTFACTORY_##NAME##_INSTALLER () { \
                ROCKY_NAMESPACE::ContextImpl::objectFactories()[ROCKY_NAMESPACE::util::toLower(#NAME)] = FUNC; \
        } }; \
        __ROCKY_OBJECTFACTORY_##NAME##_INSTALLER __rocky_objectFactory_##NAME ;
