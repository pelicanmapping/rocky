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

#include <rocky/optional.h>
#include <string>
#include <memory>

namespace rocky
{
    template<class T> using shared_ptr = std::shared_ptr<T>;
    template<class T> using shared_constptr = std::shared_ptr<const T>;
    template<class T> using weak_ptr = std::weak_ptr<T>;
    template<class T> using weak_constptr = std::weak_ptr<const T>;

    // application-wide unique ID.
    using UID = std::int32_t;

    // revision type
    using Revision = std::int32_t;

    //! Generate an application-wide unique identifier.
    extern ROCKY_EXPORT UID createUID();

    //! Base class for anything using the "Inherit" pattern
    class ROCKY_EXPORT Object
    {
    protected:
        Object() { }
        optional<std::string> _name;
    public:
        void setName(const std::string& value) { _name = value; }
        const optional<std::string>& name() const { return _name; }
        virtual ~Object() { }
    };

    //! Base class for objects implementing the create() pattern
    //! that should only be held with a shared_ptr.
    template<typename PARENT, typename ME>
    class Inherit : public PARENT
    {
    protected:
        //template<typename... Args>
        //Inherit(Args&... args) : PARENT(args...) { }
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


//! public getter/setter
#define public_prop(TYPE, NAME) \
    private: \
        TYPE _ ## NAME ; \
    public: \
        TYPE & NAME () { return _ ## NAME; } \
        TYPE & NAME ## _mutable() { return _ ## NAME; } \
        const TYPE & NAME () const { return _ ## NAME; }

    //! property macro
#define const_prop(TYPE, NAME) \
    private: \
        TYPE _ ## NAME ; \
    protected: \
        TYPE & NAME () { return _ ## NAME; } \
        TYPE & NAME ## _mutable() { return _ ## NAME; } \
    public: \
        const TYPE & NAME () const { return _ ## NAME; }

//! optional property macro
#define optional_prop(TYPE, NAME) \
    private: \
      optional< TYPE > _ ## NAME ; \
    public: \
      optional< TYPE >& NAME () { return _ ## NAME; } \
      const optional< TYPE >& NAME () const { return _ ## NAME; }

    //! optional property macro
#define ROCKY_OPTION(TYPE, NAME) \
    private: \
      optional< TYPE > _ ## NAME ; \
      mutable std::vector<std::function<void(const TYPE&)>> _ ## NAME ## _listeners; \
    public: \
      optional< TYPE >& NAME () { return _ ## NAME; } \
      const optional< TYPE >& NAME () const { return _ ## NAME; } \
      void NAME ## Changed (std::function<void(const TYPE&)> f) const { \
        _ ## NAME ## _listeners.push_back(f); } \
      void set_ ## NAME (const TYPE& value) { \
        _ ## NAME = value; \
        for(auto& notify : _ ## NAME ## _listeners) notify(value); }

//! ref_ptr property macro
#define ROCKY_OPTION_SHAREDPTR(TYPE, NAME) \
    private: \
    std::shared_ptr< TYPE > _ ## NAME ; \
    public: \
    std::shared_ptr< TYPE >& NAME () { return _ ## NAME; } \
    const std::shared_ptr< TYPE >& NAME () const { return _ ## NAME; }

//! vector property macro
#define ROCKY_OPTION_VECTOR(TYPE, NAME) \
    private: \
    std::vector< TYPE > _ ## NAME ; \
    public: \
    std::vector< TYPE >& NAME () { return _ ## NAME; } \
    const std::vector< TYPE >& NAME () const { return _ ## NAME; }

#define ROCKY_OPTION_IMPL(CLASS, TYPE, FUNC, OPTION) \
    void CLASS ::set ## FUNC (const TYPE & value) { options(). set_ ## OPTION (value); }\
    const TYPE & CLASS ::get ## FUNC () const { return options(). OPTION ().get(); }

//! property macro
#define ROCKY_PROPERTY(TYPE, NAME) \
    private: \
    TYPE _ ## NAME ; \
    public: \
    TYPE & NAME () { return _ ## NAME; } \
    TYPE & NAME ## _mutable() { return _ ## NAME; } \
    const TYPE & NAME () const { return _ ## NAME; }

//! const property macro
#define ROCKY_PROPERTY_CONST(TYPE, NAME) \
    private: \
    TYPE _ ## NAME ; \
    protected: \
    TYPE & NAME () { return _ ## NAME; } \
    TYPE & NAME ## _mutable() { return _ ## NAME; } \
    public: \
    const TYPE & NAME () const { return _ ## NAME; }

#define ROCKY_OPTION_LESS(L,R) \
    if (L().has_value() && !R().has_value()) return true; \
    if (R().has_value() && !L().has_value()) return false; \
    if (L().has_value() && R().has_value()) { \
        if (L().get() < R().get()) return true; \
        if (L().get() > R().get()) return false; }
            

#ifdef _MSC_VER
// VS ignores
#pragma warning (disable: 4224)
#pragma warning (disable: 4180)
#endif

// Sringification
#define ROCKY_STRINGIFY_0(x) #x
#define ROCKY_STRINGIFY(x) ROCKY_STRINGIFY_0(x)

