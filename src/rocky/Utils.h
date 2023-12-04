/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Math.h>
#include <rocky/Status.h>
#include <rocky/Log.h>

#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <locale>
#include <iomanip>
#include <map>
#include <unordered_map>
#include <list>
#include <set>
#include <filesystem>
#include <ctype.h>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>

class TiXmlDocument;

namespace ROCKY_NAMESPACE { namespace util
{
    extern ROCKY_EXPORT const std::string EMPTY_STRING;

    using StringVector = std::vector<std::string>;
    using StringSet = std::set<std::string>;
    using StringTable = std::unordered_map<std::string, std::string>;

    /** Replaces all the instances of "pattern" with "replacement" in "in_out" */
    extern ROCKY_EXPORT std::string& replace_in_place(
        std::string&       in_out,
        const std::string& pattern,
        const std::string& replacement );

    /** Replaces all the instances of "pattern" with "replacement" in "in_out" (case-insensitive) */
    extern ROCKY_EXPORT std::string& replace_in_place_case_insensitive(
        std::string&       in_out,
        const std::string& pattern,
        const std::string& replacement );

    //! Trims whitespace from the ends of a string.
    extern ROCKY_EXPORT std::string trim(const std::string& in);

    //! Trims whitespace from the ends of a string; in-place modification on the string to reduce string copies.
    extern ROCKY_EXPORT void trimInPlace(std::string& str);

    //! Removes leading and trailing whitespace, and replaces all other
    //! whitespace with single spaces
    extern ROCKY_EXPORT std::string trimAndCompress(const std::string& in);

    //! True is "ref" starts with "pattern"
    extern ROCKY_EXPORT bool startsWith(
        const std::string& ref,
        const std::string& pattern,
        bool               caseSensitive =true,
        const std::locale& locale        =std::locale() );

    //! True is "ref" ends with "pattern"
    extern ROCKY_EXPORT bool endsWith(
        const std::string& ref,
        const std::string& pattern,
        bool               caseSensitive =true,
        const std::locale& locale        =std::locale() );

    //! Case-insensitive compare
    extern ROCKY_EXPORT bool ciEquals(
        const std::string& lhs,
        const std::string& rhs,
        const std::locale& local = std::locale() );

    //! Case-insensitive STL comparator
    struct ROCKY_EXPORT CIStringComp {
        bool operator()(const std::string& lhs, const std::string& rhs) const;
    };


    extern ROCKY_EXPORT std::string joinStrings( const StringVector& input, char delim );

    /** Returns a lower-case version of the input string. */
    extern ROCKY_EXPORT std::string toLower( const std::string& input );

    /** Makes a valid filename out of a string */
    extern ROCKY_EXPORT std::string toLegalFileName( const std::string& input, bool allowSubdir=false, const char* replacementChar=NULL);

    /** Generates a hashed integer for a string (poor man's MD5) */
    extern ROCKY_EXPORT unsigned hashString( const std::string& input );

    /** Same as hashString but returns a string value. */
    extern ROCKY_EXPORT std::string hashToString(const std::string& input);

    //! Gets the total number of seconds formatted as H:M:S
    extern ROCKY_EXPORT std::string prettyPrintTime( double seconds );

    //! Gets a pretty printed version of the given size in MB.
    extern ROCKY_EXPORT std::string prettyPrintSize( double mb );  
    
    //! Extract the "i-th" token from a delimited string
    extern ROCKY_EXPORT std::string getToken(const std::string& input, unsigned i, const std::string& delims=",");

    extern ROCKY_EXPORT glm::u8vec4 toColor(const std::string& str, const glm::u8vec4& default_value);

    extern ROCKY_EXPORT std::string makeCacheKey(const std::string& key, const std::string& prefix);
    
    //! Full pathname of the currently running executable
    extern ROCKY_EXPORT std::string getExecutableLocation();

    //! Writes a string to a text file on disk.
    extern ROCKY_EXPORT bool writeToFile(const std::string& data, const std::string& filename);
    
    //! Reads a disk file into a string.
    extern ROCKY_EXPORT bool readFromFile(std::string& data, const std::string& filename);

    //------------------------------------------------------------------------
    // conversion templates

    // converts a string to primitive using serialization
    template<typename T> inline T
    as( const std::string& str, const T& default_value )
    {
        T temp = default_value;
        std::istringstream strin( str );
        if ( !strin.eof() ) strin >> temp;
        return temp;
    }

    // template specialization for integers (to handle hex)
#define AS_INT_DEC_OR_HEX(TYPE) \
    template<> inline TYPE \
    as< TYPE >(const std::string& str, const TYPE & dv) { \
        TYPE temp = dv; \
        std::istringstream strin( trim(str) ); \
        if ( !strin.eof() ) { \
            if ( str.length() >= 2 && str[0] == '0' && str[1] == 'x' ) { \
                strin.seekg( 2 ); \
                strin >> std::hex >> temp; \
            } \
            else { \
                strin >> temp; \
            } \
        } \
        return temp; \
    }

    AS_INT_DEC_OR_HEX(int)
    AS_INT_DEC_OR_HEX(unsigned)
    AS_INT_DEC_OR_HEX(short)
    AS_INT_DEC_OR_HEX(unsigned short)
    AS_INT_DEC_OR_HEX(long)
    AS_INT_DEC_OR_HEX(unsigned long)

    // template specialization for a bool
    template<> inline bool
    as<bool>( const std::string& str, const bool& default_value )
    {
        std::string temp = toLower(str);
        return
            temp == "true"  || temp == "yes" || temp == "on" ? true :
            temp == "false" || temp == "no" || temp == "off" ? false :
            default_value;
    }

    // template specialization for string
    template<> inline std::string
    as<std::string>( const std::string& str, const std::string& default_value )
    {
        return str;
    }

    // snips a substring and parses it.
    template<typename T> inline bool
    as(const std::string& in, unsigned start, unsigned len, T default_value)
    {
        std::string buf;
        std::copy( in.begin()+start, in.begin()+start+len, std::back_inserter(buf) );
        return as<T>(buf, default_value);
    }

    // converts a primitive to a string
    template<typename T> inline std::string
    toString(const T& value)
    {
        std::stringstream out;
		//out << std::setprecision(20) << std::fixed << value;
		out << std::setprecision(20) <<  value;
        std::string outStr;
        outStr = out.str();
        return outStr;
    }

    // template speciallization for a bool to print out "true" or "false"
    template<> inline std::string
    toString<bool>(const bool& value)
    {
        return value ? "true" : "false";
    }

    /**
     * Assembles and returns an inline string using a stream-like << operator.
     * Example:
     *     std::string str = Stringify() << "Hello, world " << variable;
     */
    struct Stringify
    {
        operator std::string () const
        {
            std::string result;
            result = buf.str();
            return result;
        }

        template<typename T>
        Stringify& operator << (const T& val) { buf << val; return (*this); }

        Stringify& operator << (const Stringify& val) { buf << (std::string)val; return (*this); }

    protected:
        std::stringstream buf;
    };

    using make_string = Stringify;

    template<> inline
    Stringify& Stringify::operator << <bool>(const bool& val) { buf << (val ? "true" : "false"); return (*this); }

    /**
     * Splits a string up into a vector of strings based on a set of
     * delimiters, quotes, and rules.
     */
    class ROCKY_EXPORT StringTokenizer
    {
    public:
        StringTokenizer( 
            const std::string& delims =" \t\r\n", 
            const std::string& quotes ="'\"" );

        StringTokenizer(
            const std::string& input,
            StringVector& output,
            const std::string& delims =" \t\r\n", 
            const std::string& quotes ="'\"",
            bool keepEmpties =true, 
            bool trimTokens =true);

        StringTokenizer(
            const std::string& input,
            StringTable& output,
            const std::string& delims = " \t\r\n",
            const std::string& keypairseparators = "=",
            const std::string& quotes = "'\"",
            bool keepEmpties = true,
            bool trimTokens = true);


        void tokenize( const std::string& input, StringVector& output ) const;

        bool& keepEmpties() { return _allowEmpties; }

        bool& trimTokens() { return _trimTokens; }

        void addDelim( char delim, bool keepAsToken =false );

        void addDelims( const std::string& delims, bool keepAsTokens =false );

        void addQuote( char delim, bool keepInToken =false );

        void addQuotes( const std::string& delims, bool keepInTokens =false );

    private:
        using TokenMap = std::unordered_map<char,bool>;
        TokenMap _delims;
        TokenMap _quotes;
        bool     _allowEmpties;
        bool     _trimTokens;
    };

    /**
     * Tracks usage data by maintaining a sentry-blocked linked list.
     * Each time a use called "use" the corresponding record moves to
     * the right of the sentry marker. After a cycle you can call
     * collectTrash to process all users that did not call use() in the
     * that cycle, and dispose of them.
     */
    template<typename T>
    class SentryTracker
    {
    public:
        struct ListEntry
        {
            ListEntry(T data, void* token) : _data(data), _token(token) { }
            T _data;
            void* _token;
        };

        using List = std::list<ListEntry>;
        using ListIterator = typename List::iterator;
        using Token = ListIterator;

        SentryTracker()
        {
            reset();
        }

        ~SentryTracker()
        {
            for (auto& e : _list)
            {
                Token* te = static_cast<Token*>(e._token);
                if (te)
                    delete te;
            }
        }

        void reset()
        {
            for (auto& e : _list)
            {
                Token* te = static_cast<Token*>(e._token);
                if (te)
                    delete te;
            }
            _list.clear();
            _list.emplace_front(nullptr, nullptr); // the sentry marker
            _sentryptr = _list.begin();
        }

        List _list;
        ListIterator _sentryptr;

        inline void* use(const T& data, void* token)
        {
            // Find the tracker for this tile and update its timestamp
            if (token)
            {
                Token* ptr = static_cast<Token*>(token);

                // Move the tracker to the front of the list (ahead of the sentry).
                // Once a cull traversal is complete, all visited tiles will be
                // in front of the sentry, leaving all non-visited tiles behind it.
                _list.splice(_list.begin(), _list, *ptr);
                *ptr = _list.begin();
                return ptr;
            }
            else
            {
                // New entry:
                Token* ptr = new Token();
                _list.emplace_front(data, ptr); // ListEntry
                *ptr = _list.begin();
                return ptr;
            }
        }

        inline void flush(
            unsigned maxCount,
            std::function<bool(T& obj)> dispose)
        {
            // After cull, all visited tiles are in front of the sentry, and all
            // non-visited tiles are behind it. Start at the sentry position and
            // iterate over the non-visited tiles, checking them for deletion.
            ListIterator i = _sentryptr;
            ListIterator tmp;
            unsigned count = 0;

            for (++i; i != _list.end() && count < maxCount; ++i)
            {
                ListEntry& le = *i;

                bool disposed = true;

                // user disposal function
                if (dispose != nullptr)
                    disposed = dispose(le._data);

                if (disposed)
                {
                    // back up the iterator so we can safely erase the entry:
                    tmp = i;
                    --i;

                    // delete the token
                    delete static_cast<Token*>(le._token);

                    // remove it from the tracker list:
                    _list.erase(tmp);
                    ++count;
                }
            }

            // reset the sentry.
            _list.splice(_list.begin(), _list, _sentryptr);
            _sentryptr = _list.begin();
        }
    };

    template<class T = std::chrono::steady_clock>
    struct scoped_chrono
    {
        std::string _me;
        std::chrono::time_point<T> _a;
        std::chrono::duration<T>* _d = nullptr;
        scoped_chrono(const std::string& me) : _me(me), _a(T::now()) { }
        scoped_chrono(std::chrono::duration<T>& dur) : _d(&dur), _a(T::now()) { }
        ~scoped_chrono() {
            auto b = T::now();
            auto dur = b - _a;
            if (_d) {
                *_d = dur;
            }
            else {
                auto d = (float)std::chrono::duration_cast<std::chrono::microseconds>(dur).count();
                Log()->info(std::to_string(std::this_thread::get_id()) + " : " + _me + " = " + std::to_string(d) + "us");
            }
        }
    };

    template<class T = std::chrono::steady_clock>
    struct timer
    {
        std::chrono::time_point<T> then;
        timer() : then(T::now()) { }
        double seconds() const {
            return (double)std::chrono::duration_cast<std::chrono::milliseconds>(T::now() - then).count() * 0.001;
        }
        double milliseconds() const {
            return (double)std::chrono::duration_cast<std::chrono::microseconds>(T::now()-then).count() * 0.001;
        }
    };

    /**
    * Virtual interface for a stream compressor
    */
    class StreamCompressor
    {
    public:
        //! Compress data to an output stream.
        //! @param src Data to compress
        //! @param out Stream to which to write compressed data
        //! @return True upon success
        virtual bool compress(const std::string& src, std::ostream& out) const = 0;

        //! Decompress data from a stream.
        //! @param src Data to decompress
        //! @param out Data in which to store decompressed data
        //! @return True upon success
        virtual bool decompress(std::istream& in, std::string& out) const = 0;
    };

#ifdef ROCKY_HAS_ZLIB
    /**
    * Stream compressor that uses INFLATE/DEFLATE compression
    */
    class ROCKY_EXPORT ZLibCompressor : public StreamCompressor
    {
    public:
        //! Compress data to an output stream.
        //! @param src Data to compress
        //! @param out Stream to which to write compressed data
        //! @return True upon success
        bool compress(const std::string& src, std::ostream& out) const override;

        //! Decompress data from a stream.
        //! @param src Data to decompress
        //! @param out Data in which to store decompressed data
        //! @return True upon success
        bool decompress(std::istream& in, std::string& out) const override;
    };
#endif // ROCKY_HAS_ZLIB

    // Adapted from https://www.geeksforgeeks.org/lru-cache-implementation
    template<class K, class V>
    class LRUCache
    {
    private:
        mutable std::mutex mutex;
        int capacity;
        using E = typename std::pair<K, V>;
        typename std::list<E> cache;
        std::unordered_map<K, typename std::list<E>::iterator> map;

    public:
        int hits = 0;
        int gets = 0;

        LRUCache(int capacity = 32) : capacity(capacity) { }

        inline void setCapacity(int value) {
            std::scoped_lock L(mutex);
            cache.clear();
            map.clear();
            hits = 0;
            gets = 0;
            capacity = std::max(0, value);
        }

        inline V get(const K& key) {
            if (capacity == 0) return V();
            std::scoped_lock L(mutex);
            ++gets;
            auto it = map.find(key);
            if (it == map.end())
                return V();
            cache.splice(cache.end(), cache, it->second);
            ++hits;
            return it->second->second;
        }

        inline void put(const K& key, const V& value) {
            if (capacity == 0) return;
            std::scoped_lock L(mutex);
            if (cache.size() == capacity) {
                auto first_key = cache.front().first;
                cache.pop_front();
                map.erase(first_key);
            }
            cache.push_back({ key, value });
            map[key] = --cache.end();
        }
    };
} }
