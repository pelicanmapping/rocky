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
#include <rocky/optional.h>
#include <string>
#include <memory>
#include <iostream>
#include <cstdlib>

// match the weejobs export to the library export
#ifndef WEEJOBS_EXPORT
#define WEEJOBS_EXPORT ROCKY_EXPORT
#endif

namespace ROCKY_NAMESPACE
{
    template<class T> using shared_ptr = std::shared_ptr<T>;
    template<class T> using shared_constptr = std::shared_ptr<const T>;
    template<class T> using weak_ptr = std::weak_ptr<T>;
    template<class T> using weak_constptr = std::weak_ptr<const T>;

    //! application-wide unique ID.
    using UID = std::int32_t;

    //! revision type
    using Revision = std::int32_t;

    //! json serialization type
    using JSON = std::string;

    //! Pretty-print a json string.
    extern ROCKY_EXPORT std::string json_pretty(const JSON&);

    //! Generate an application-wide unique identifier.
    extern ROCKY_EXPORT UID createUID();

    //! Base class for anything using the "Inherit" pattern
    //! @private
    class ROCKY_EXPORT Object
    {
    protected:
        Object() = default;
        std::string _name;
    public:
        void setName(const std::string& value) { _name = value; }
        const std::string& name() const { return _name; }
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
        using ptr = shared_ptr<ME>;
        using constptr = shared_ptr<const ME>;
        using weakptr = weak_ptr<ME>;
        using constweakptr = weak_ptr<const ME>;
        template<typename... Args>
        static shared_ptr<ME> create(Args&&... args) {
            return std::make_shared<ME>(std::forward<Args>(args)...);
        }
        template<typename T>
        static shared_ptr<ME> cast(shared_ptr<T>& rhs) {
            return std::dynamic_pointer_cast<ME>(rhs);
        }
        template<typename T>
        static shared_ptr<const ME> cast(shared_ptr<const T>& rhs) {
            return std::dynamic_pointer_cast<const ME>(rhs);
        }
        template<typename T>
        static shared_ptr<ME> cast(const shared_ptr<T>& rhs) {
            return std::dynamic_pointer_cast<ME>(rhs);
        }
        template<typename T>
        static const shared_ptr<const ME> cast(const shared_ptr<const T>& rhs) {
            return std::dynamic_pointer_cast<const ME>(rhs);
        }
    };
}

#define ROCKY_ABOUT(NAME, VER) namespace { struct __about_##NAME { __about_##NAME() { rocky::Instance::about().insert(std::string(#NAME) + " " + VER); } }; __about_##NAME about_##NAME; }

#define ROCKY_DEPRECATED(A, B) rocky::Log::warn() << #A << " is deprecated; please use " << #B << std::endl

#if defined(_MSC_VER)
#define ROCKY_FILE (std::strrchr(__FILE__, '\\') ? std::strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define ROCKY_FILE (std::strrchr(__FILE__, '/') ? std::strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

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
