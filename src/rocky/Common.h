/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#if defined(_MSC_VER)
#pragma warning( disable : 4244 )
#pragma warning( disable : 4251 )
#pragma warning( disable : 4267 )
#pragma warning( disable : 4275 )
#pragma warning( disable : 4290 )
#pragma warning( disable : 4786 )
#pragma warning( disable : 4305 )
#pragma warning( disable : 4996 )
#endif

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined( __BCPLUSPLUS__)  || defined( __MWERKS__)
#  if defined( ROCKY_LIBRARY_STATIC )
#    define ROCKY_EXPORT
#  elif defined( ROCKY_LIBRARY )
#    define ROCKY_EXPORT   __declspec(dllexport)
#  else
#    define ROCKY_EXPORT   __declspec(dllimport)
#  endif
#else
#  define ROCKY_EXPORT
#endif  

#define ROCKY_NAMESPACE rocky
#define ROCKY_ENGINE_NAMESPACE rocky::engine

#include <rocky/optional.h>
#include <string>
#include <memory>
#include <iostream>

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
        Object() { }
        std::string _name;
    public:
        void setName(const std::string& value) { _name = value; }
        const std::string& name() const { return _name; }
        virtual ~Object() { }
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
            return std::make_shared<ME>(args...);
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

#ifdef _MSC_VER
// VS ignores
#pragma warning (disable: 4224)
#pragma warning (disable: 4180)
#endif

// Sringification
#define ROCKY_STRINGIFY_0(x) #x
#define ROCKY_STRINGIFY(x) ROCKY_STRINGIFY_0(x)

#define ROCKY_USE_STATIC_INSTANCE_LOG

#ifdef ROCKY_USE_STATIC_INSTANCE_LOG
#define ROCKY_INFO rocky::Log::info()
#define ROCKY_WARN rocky::Log::warn()
#else
#define ROCKY_INFO std::cout << "<rk> "
#define ROCKY_WARN std::cout << "<rk> *** WARNING *** "
#define ROCKY_NULL if (false) std::cout
#endif

#define ROCKY_DEPRECATED(A, B) ROCKY_WARN << #A << " is deprecated; please use " << #B << std::endl

#if defined(_MSC_VER)
#define ROCKY_FILE (std::strrchr(__FILE__, '\\') ? std::strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define ROCKY_FILE (std::strrchr(__FILE__, '/') ? std::strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define ROCKY_SOFT_ASSERT(EXPR, ...) if(!(EXPR)) { std::cerr << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; }
#define ROCKY_SOFT_ASSERT_AND_RETURN(EXPR, RETVAL, ...) if(!(EXPR)) { std::cerr << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; return RETVAL; }
#define ROCKY_IF_SOFT_ASSERT(EXPR, ...) if(!(EXPR)) { std::cerr << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; } else
#define ROCKY_HARD_ASSERT(EXPR, ...) if(!(EXPR)) { std::cerr << "FATAL ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; abort(); }

#define ROCKY_TODO(...) if (false) std::cerr << "TODO (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ")..." << __VA_ARGS__ "" << std::endl