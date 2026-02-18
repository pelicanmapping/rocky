/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#if defined(_MSC_VER)
#pragma warning(disable:4244) // disable precision loss warnings (e.g., double to float)
#endif

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined( __BCPLUSPLUS__)  || defined( __MWERKS__)
#  if defined( ROCKY_BUILDING_SHARED_LIBRARY )
#    define ROCKY_EXPORT __declspec(dllexport)
#  elif defined( ROCKY_STATIC ) // building OR importing static library
#    define ROCKY_EXPORT
#  else // importing shared library
#    define ROCKY_EXPORT __declspec(dllimport)
#  endif
#else
#  define ROCKY_EXPORT
#endif

#define ROCKY_NAMESPACE rocky

#include <rocky/Version.h>
#include <rocky/Log.h>
#include <rocky/option.h>
#include <string>
#include <memory>
#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <functional>

// match the weejobs export to the library export
#ifndef WEEJOBS_EXPORT
#define WEEJOBS_EXPORT ROCKY_EXPORT
#endif

namespace ROCKY_NAMESPACE
{
    //! application-wide unique ID.
    using UID = std::int32_t;

    //! revision type
    using Revision = std::int32_t;

    //! Pretty-print a json string.
    extern ROCKY_EXPORT std::string json_pretty(std::string_view JSON);

    //! Generate an application-wide unique identifier.
    extern ROCKY_EXPORT UID createUID();

    //! unqualify a C++ class name
    inline constexpr std::string_view unqualify(std::string_view s) {
        auto pos = s.rfind("::");
        return (pos == s.npos) ? s : s.substr(pos + 2);
    }

    //! Base class for anything using the "Inherit" pattern
    //! @private
    class ROCKY_EXPORT Object
    {
    protected:
        Object() = default;

    public:
        std::string name;

        virtual ~Object() { } // makes Object polymorphic
    };

    //! Base class for objects implementing the create() pattern
    //! that should only be held with a shared_ptr.
    //! @private
    template<typename PARENT, typename ME>
    class Inherit : public PARENT
    {
    protected:
        template<typename... Args>
        Inherit(const Args&... args) : PARENT(args...) { }
    public:
        using super = Inherit<PARENT, ME>;
        using ptr = std::shared_ptr<ME>;
        using Ptr = ptr;
        using constptr = std::shared_ptr<const ME>;
        using ConstPtr = constptr;
        using weakptr = std::weak_ptr<ME>;
        using WeakPtr = weakptr;
        using constweakptr = std::weak_ptr<const ME>;
        using ConstWeakPtr = constweakptr;
        virtual std::string_view className() const noexcept {
            return unqualify(typeid(ME).name());
        }
        template<typename... Args>
        static std::shared_ptr<ME> create(Args&&... args) {
            return std::make_shared<ME>(std::forward<Args>(args)...);
        }
        template<typename T>
        static std::shared_ptr<ME> cast(std::shared_ptr<T>& rhs) {
            return std::dynamic_pointer_cast<ME>(rhs);
        }
        template<typename T>
        static std::shared_ptr<const ME> cast(std::shared_ptr<const T>& rhs) {
            return std::dynamic_pointer_cast<const ME>(rhs);
        }
        template<typename T>
        static std::shared_ptr<ME> cast(const std::shared_ptr<T>& rhs) {
            return std::dynamic_pointer_cast<ME>(rhs);
        }
        template<typename T>
        static const std::shared_ptr<const ME> cast(const std::shared_ptr<const T>& rhs) {
            return std::dynamic_pointer_cast<const ME>(rhs);
        }
    };

    /** A comparable function that you can use in a container */
    struct NamedFunction
    {
        void* id = nullptr;
        std::function<void()> func;
        bool operator==(const NamedFunction& other) const { return id == other.id; }
        bool operator<(const NamedFunction& other) const { return id < other.id; }
        NamedFunction(void* in_in, std::function<void()> in_func) : id(in_in), func(in_func) { }
    };
}

#define ROCKY_ABOUT(NAME, VER) namespace { struct __about_##NAME { __about_##NAME() { rocky::ContextImpl::about().insert(std::string(#NAME) + " " + VER); } }; __about_##NAME about_##NAME; }

#define ROCKY_DEPRECATED(A, B) rocky::Log::warn() << #A << " is deprecated; please use " << #B << std::endl

#if defined(_MSC_VER)
#define ROCKY_FILE (std::strrchr(__FILE__, '\\') ? std::strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define ROCKY_FILE (std::strrchr(__FILE__, '/') ? std::strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define ROCKY_QUIET_ASSERT(EXPR, ...) if(!(EXPR)) { std::cerr << ""; }
#define ROCKY_SOFT_ASSERT(EXPR, ...) if(!(EXPR)) { std::cerr << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; }
#define ROCKY_SOFT_ASSERT_AND_RETURN(EXPR, RETVAL, ...) if(!(EXPR)) { std::cerr << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; return RETVAL; }
#define ROCKY_IF_SOFT_ASSERT(EXPR, ...) if(!(EXPR)) { std::cerr << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; } else
#define ROCKY_HARD_ASSERT(EXPR, ...) if(!(EXPR)) { std::cerr << "FATAL ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; abort(); }

#define ROCKY_HARD_ASSERT_STATUS(STATUS) \
    if(STATUS .failed()) { std::cerr \
        << "FATAL ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " \
        << STATUS .message \
        << std::endl; exit(-1); \
    }

#define ROCKY_TODO(...) if (false) std::cerr << "TODO (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ")..." << __VA_ARGS__ "" << std::endl
