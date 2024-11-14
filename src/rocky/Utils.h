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
#include <rocky/weejobs.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>


class TiXmlDocument;

namespace ROCKY_NAMESPACE
{
    namespace util
    {
        const std::string EMPTY_STRING = {};

        //! Gets the value of an environment variable (safely)
        extern ROCKY_EXPORT std::string getEnvVar(const char* name);

        //! Whther and environment variable is set at all
        extern ROCKY_EXPORT bool isEnvVarSet(const char* name);

        //! Replacement for sprintf
        //! https://stackoverflow.com/a/26221725/4218920
        template<typename ... Args>
        std::string format( const std::string& format, Args... args) {
            int size_s = std::snprintf( nullptr, 0, format.c_str(), args... ) + 1; // Extra space for '\0'
            if( size_s <= 0 ) { throw std::runtime_error( "Error during formatting." ); }
            auto size = static_cast<size_t>( size_s );
            std::unique_ptr<char[]> buf( new char[ size ] );
            std::snprintf( buf.get(), size, format.c_str(), args ... );
            return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
        }

        //! Replaces all the instances of "pattern" with "replacement" in "in_out"
        extern ROCKY_EXPORT std::string& replace_in_place(
            std::string& in_out,
            const std::string& pattern,
            const std::string& replacement);

        //! Replaces all the instances of "pattern" with "replacement" in "in_out" (case-insensitive)
        extern ROCKY_EXPORT std::string& replace_in_place_case_insensitive(
            std::string& in_out,
            const std::string& pattern,
            const std::string& replacement);

        //! Trims whitespace from the ends of a string.
        extern ROCKY_EXPORT std::string trim(const std::string& in);

        //! Trims whitespace from the ends of a string; in-place modification on the string to reduce string copies.
        extern ROCKY_EXPORT void trim_in_place(std::string& str);

        //! True is "ref" starts with "pattern"
        extern ROCKY_EXPORT bool startsWith(
            const std::string& ref,
            const std::string& pattern,
            bool               caseSensitive = true,
            const std::locale& locale = std::locale());

        //! True is "ref" ends with "pattern"
        extern ROCKY_EXPORT bool endsWith(
            const std::string& ref,
            const std::string& pattern,
            bool               caseSensitive = true,
            const std::locale& locale = std::locale());

        //! Case-insensitive compare
        extern ROCKY_EXPORT bool ciEquals(
            const std::string& lhs,
            const std::string& rhs,
            const std::locale& local = std::locale());


        /** Returns a lower-case version of the input string. */
        extern ROCKY_EXPORT std::string toLower(const std::string& input);

        /** Makes a valid filename out of a string */
        extern ROCKY_EXPORT std::string toLegalFileName(const std::string& input, bool allowSubdir = false, const char* replacementChar = NULL);

        /** Generates a hashed integer for a string (poor man's MD5) */
        extern ROCKY_EXPORT unsigned hashString(const std::string& input);

        //! Full pathname of the currently running executable
        extern ROCKY_EXPORT std::string getExecutableLocation();

        //------------------------------------------------------------------------
        // conversion templates

        // converts a string to primitive using serialization
        template<typename T> inline T
            as(const std::string& str, const T& default_value)
        {
            T temp = default_value;
            std::istringstream strin(str);
            if (!strin.eof()) strin >> temp;
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
            as<bool>(const std::string& str, const bool& default_value)
        {
            std::string temp = toLower(str);
            return
                temp == "true" || temp == "yes" || temp == "on" ? true :
                temp == "false" || temp == "no" || temp == "off" ? false :
                default_value;
        }

        // template specialization for string
        template<> inline std::string
            as<std::string>(const std::string& str, const std::string& default_value)
        {
            return str;
        }

        // snips a substring and parses it.
        template<typename T> inline bool
            as(const std::string& in, unsigned start, unsigned len, T default_value)
        {
            std::string buf;
            std::copy(in.begin() + start, in.begin() + start + len, std::back_inserter(buf));
            return as<T>(buf, default_value);
        }

        /**
         * Assembles and returns an inline string using a stream-like << operator.
         * Example:
         *     std::string str = make_string() << "Hello, world " << variable;
         */
        struct make_string
        {
            operator std::string() const
            {
                std::string result;
                result = buf.str();
                return result;
            }

            template<typename T>
            make_string& operator << (const T& val) { buf << val; return (*this); }

            make_string& operator << (const make_string& val) { buf << (std::string)val; return (*this); }

        protected:
            std::stringstream buf;
        };

        template<> inline
            make_string& make_string::operator << <bool>(const bool& val) { buf << (val ? "true" : "false"); return (*this); }

        /**
         * Splits a string up into a vector of strings based on a set of
         * delimiters, quotes, and rules.
         */
        class ROCKY_EXPORT StringTokenizer
        {
        public:
            StringTokenizer(
                const std::string& delims = " \t\r\n",
                const std::string& quotes = "'\"");

            StringTokenizer(
                const std::string& input,
                std::vector<std::string>& output,
                const std::string& delims = " \t\r\n",
                const std::string& quotes = "'\"",
                bool keepEmpties = true,
                bool trimTokens = true);

            StringTokenizer(
                const std::string& input,
                std::unordered_map<std::string, std::string>& output,
                const std::string& delims = " \t\r\n",
                const std::string& keypairseparators = "=",
                const std::string& quotes = "'\"",
                bool keepEmpties = true,
                bool trimTokens = true);


            void tokenize(const std::string& input, std::vector<std::string>& output) const;

            bool& keepEmpties() { return _allowEmpties; }

            bool& trimTokens() { return _trimTokens; }

            void addDelim(char delim, bool keepAsToken = false);

            void addDelims(const std::string& delims, bool keepAsTokens = false);

            void addQuote(char delim, bool keepInToken = false);

            void addQuotes(const std::string& delims, bool keepInTokens = false);

        private:
            using TokenMap = std::unordered_map<char, bool>;
            TokenMap _delims;
            TokenMap _quotes;
            bool     _allowEmpties;
            bool     _trimTokens;
        };

        //! Writes a string to a text file on disk.
        extern ROCKY_EXPORT bool writeToFile(const std::string& data, const std::string& filename);

        //! Reads a disk file into a string.
        extern ROCKY_EXPORT Result<std::string> readFromFile(const std::string& filename);

        //! Sets the name of the current thread
        extern ROCKY_EXPORT void setThreadName(const char* name);


        template<typename T>
        class ring_buffer
        {
        public:
            ring_buffer(int size) : _size(size), _buffer(size) {}

            bool push(const T& obj) {
                if (full()) return false;
                _buffer[_writeIndex] = obj;
                _writeIndex.exchange((_writeIndex + 1) % _size);
                _condition.notify_one();
            }

            bool emplace(T&& obj) {
                if (full()) return false;
                _buffer[_writeIndex] = std::move(obj);
                _writeIndex.exchange((_writeIndex + 1) % _size);
                _condition.notify_one();
                return true;
            }

            bool pop(T& obj) {
                if (_readIndex == _writeIndex) return false;
                obj = std::move(_buffer[_readIndex]);
                _readIndex.exchange((_readIndex + 1) % _size);
                return true;
            }

            template<typename DURATION_T>
            bool wait(DURATION_T timeout) {
                std::unique_lock<std::mutex> L(_mutex);
                return _condition.wait_for(L, timeout, [this]() { return !empty(); });
            }

            bool empty() const {
                return _readIndex == _writeIndex;
            }

            bool full() const {
                return (_writeIndex + 1) % _size == _readIndex;
            }
        private:
            std::atomic_int _readIndex = { 0 };
            std::atomic_int _writeIndex = { 0 };
            int _size;
            std::vector<T> _buffer;
            mutable std::mutex _mutex;
            mutable std::condition_variable_any _condition;
        };

        /**
        * A std::map-like map that uses a vector.
        * This benchmarks much faster than std::map or std::unordered_map for small sets.
        */
        template<typename KEY, typename DATA, typename LESS = std::less<KEY>>
        struct vector_map
        {
            struct ENTRY {
                KEY first;
                DATA second;
            };

            using value_type = DATA;
            using container_t = std::vector<ENTRY>;
            using iterator = typename container_t::iterator;
            using const_iterator = typename container_t::const_iterator;

            container_t _container;

            inline bool keys_equal(const KEY& a, const KEY& b) const {
                return LESS()(a, b) == false && LESS()(b, a) == false;
            }

            inline DATA& operator[](const KEY& key) {
                for (unsigned i = 0; i < _container.size(); ++i) {
                    if (keys_equal(_container[i].first, key)) {
                        return _container[i].second;
                    }
                }
                _container.resize(_container.size() + 1);
                _container.back().first = key;
                return _container.back().second;
            }

            inline DATA& emplace(const KEY& key, const DATA& data) {
                auto& entry = operator[](key);
                entry = data;
                return entry;
            }

            inline DATA& emplace(const KEY& key, DATA&& data) {
                auto& entry = operator[](key);
                entry = std::move(data);
                return entry;
            }

            inline const_iterator begin() const { return _container.begin(); }
            inline const_iterator end()   const { return _container.end(); }
            inline iterator begin() { return _container.begin(); }
            inline iterator end() { return _container.end(); }

            inline iterator find(const KEY& key) {
                for (unsigned i = 0; i < _container.size(); ++i) {
                    if (keys_equal(_container[i].first, key)) {
                        return _container.begin() + i;
                    }
                }
                return _container.end();
            }

            inline const_iterator find(const KEY& key) const {
                for (unsigned i = 0; i < _container.size(); ++i) {
                    if (keys_equal(_container[i].first, key)) {
                        return _container.begin() + i;
                    }
                }
                return _container.end();
            }

            inline bool empty() const { return _container.empty(); }

            inline void clear() { _container.clear(); }

            inline void erase(const KEY& key) {
                for (unsigned i = 0; i < _container.size(); ++i) {
                    if (keys_equal(_container[i].first, key)) {
                        if (i + 1 < _container.size()) {
                            _container[i] = _container[_container.size() - 1];
                        }
                        _container.resize(_container.size() - 1);
                        break;
                    }
                }
            }

            inline int indexOf(const KEY& key) const {
                for (unsigned i = 0; i < _container.size(); ++i) {
                    if (keys_equal(_container[i].first, key)) {
                        return i;
                    }
                }
                return -1;
            }

            inline int size() const { return _container.size(); }

            template<typename InputIterator>
            void insert(InputIterator a, InputIterator b) {
                for (InputIterator i = a; i != b; ++i) (*this)[i->first] = i->second;
            }
        };

        template<typename T>
        class locked_value
        {
        public:
            void set(const T& obj) {
                std::scoped_lock L(_mutex);
                _obj = obj;
            }
            void set(T&& obj) {
                std::scoped_lock L(_mutex);
                _obj = std::move(obj);
            }
            bool get_and_clear(T& obj) {
                std::scoped_lock L(_mutex);
                if (_obj.has_value()) {
                    obj = _obj.value();
                    _obj.reset();
                    return true;
                }
                return false;
            }
            bool has_value() const {
                return _obj.has_value();
            }
        private:
            std::optional<T> _obj;
            mutable std::mutex _mutex;
        };

        /**
        * Utility to manages loops that run in the background.
        */
        class BackgroundServices
        {
        public:
            using Function = std::function<void(jobs::cancelable&)>;

            std::mutex mutex;
            std::vector<jobs::future<bool>> futures;
            jobs::detail::semaphore semaphore;

            void start(const std::string& name, Function function)
            {
                ROCKY_SOFT_ASSERT_AND_RETURN(function, void());
                ROCKY_SOFT_ASSERT_AND_RETURN(!name.empty(), void());

                std::lock_guard lock(mutex);

                // wrap with a delegate function so we can control the semaphore
                auto delegate = [this, function, name](jobs::cancelable& cancelable) -> bool
                    {
                        ++semaphore;
                        function(cancelable);
                        --semaphore;
                        return true;
                    };

                jobs::context context{ name, jobs::get_pool(name, 1) };
                futures.emplace_back(jobs::dispatch(delegate, context));
            }

            void quit()
            {
                std::lock_guard lock(mutex);

                // tell all tasks to cancel
                for (auto& f : futures)
                    f.abandon();

                // block until all the background tasks exit.
                semaphore.join();

                futures.clear();
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

    }
}
