/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Utils.h"
#include <rocky/Context.h>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef WIN32
#   include <Windows.h>
#elif defined(__APPLE__) || defined(__LINUX__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__ANDROID__)
#   include <unistd.h>
#   include <sys/syscall.h>
#   include <pthread.h>
#endif

#ifdef ROCKY_HAS_ZLIB
#include <zlib.h>
ROCKY_ABOUT(zlib, ZLIB_VERSION)
#endif

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

std::vector<std::string>
StringTokenizer::operator()(std::string_view input, bool* error) const
{
    if (error)
        *error = false;

    std::vector<std::string> output;

    std::stringstream buf;
    bool inside_quote = false;
    char quote_opener = '\0';
    char quote_closer = '\0';
    bool keep_quote_char = false;
    std::size_t quote_opener_offset = 0;

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];
        auto q = _quotes.find(c);

        if (inside_quote)
        {
            if (c == quote_closer)
            {
                inside_quote = false;
                if (keep_quote_char)
                    buf << c;
            }
            else
            {
                buf << c;
            }
        }
        else
        {
            if (q != _quotes.end())
            {
                // start a new quoted region
                inside_quote = true;
                quote_opener = c;
                quote_closer = q->second.first;
                keep_quote_char = q->second.second;
                quote_opener_offset = i;

                if (keep_quote_char)
                    buf << c;
            }
            else
            {
                bool is_delimiter = false;
                auto input_remaining = input.size() - i;
                for (auto& d : _delims)
                {
                    auto delim_size = d.first.size();
                    if (delim_size <= input_remaining && strncmp(&input[i], d.first.c_str(), delim_size) == 0)
                    {
                        is_delimiter = true;

                        // end the current token, clean it up, and push it
                        auto token = buf.str();
                        if (_trimTokens)
                            trimInPlace(token);

                        if (_allowEmpties || !token.empty())
                            output.push_back(token);

                        if (d.second == true) // keep the delimiter itself as a token?
                            output.push_back(d.first);

                        buf.str("");

                        // advance over the delimiter
                        i += d.first.size() - 1;
                        break;
                    }
                }

                if (!is_delimiter)
                {
                    buf << c;
                }
            }
        }
    }

    if (inside_quote)
    {
        Log()->warn("[Tokenizer] unterminated quote in string ({} at offset {}) : {}",
            quote_opener, quote_opener_offset, input);

        if (error)
            *error = true;
    }

    std::string bufstr = buf.str();
    if (_trimTokens)
        trimInPlace(bufstr);
    if (!bufstr.empty())
        output.push_back(bufstr);

    return output;
}

//--------------------------------------------------------------------------



/** Replaces all the instances of "sub" with "other" in "s". */
std::string&
rocky::util::replaceInPlace(std::string& s, std::string_view sub, std::string_view other)
{
    if (sub.empty()) return s;
    size_t b = 0;
    for (; ; )
    {
        b = s.find(sub, b);
        if (b == s.npos) break;
        s.replace(b, sub.size(), other);
        b += other.size();
    }
    return s;
}

/**
* Trims whitespace from the ends of a string.
* by Rodrigo C F Dias
* http://www.codeproject.com/KB/stl/stdstringtrim.aspx
*/
std::string&
rocky::util::trimInPlace(std::string& str)
{
    int start, end;
    for (start = 0; start < (int)str.length() && std::isspace(str[start]); ++start);
    for (end = (int)str.length() - 1; end > start && std::isspace(str[end]); --end);
    if (end >= start) str = str.substr(start, end - start + 1);
    return str;
}

std::string
rocky::util::trim(std::string_view in)
{
    std::string out(in);
    return trimInPlace(out);
}

bool
rocky::util::ciEquals(std::string_view lhs, std::string_view rhs)
{
    if ( lhs.length() != rhs.length() )
        return false;

    for( unsigned i=0; i<lhs.length(); ++i )
    {
        if (toLower(lhs[i]) != toLower(rhs[i]))
            return false;
    }

    return true;
}

#if defined(WIN32) && !defined(__CYGWIN__)
#  define STRICMP ::stricmp
#else
#  define STRICMP ::strcasecmp
#endif

bool
rocky::util::startsWith(std::string_view ref, std::string_view pattern, bool caseSensitive)
{
    if ( pattern.length() > ref.length() )
        return false;

    if ( caseSensitive )
    {
        for( unsigned i=0; i<pattern.length(); ++i )
        {
            if ( ref[i] != pattern[i] )
                return false;
        }
    }
    else
    {
        for( unsigned i=0; i<pattern.length(); ++i )
        {
            if (toLower(ref[i]) != toLower(pattern[i]))
                return false;
        }
    }
    return true;
}

bool
rocky::util::endsWith(std::string_view ref, std::string_view pattern, bool caseSensitive)
{
    if ( pattern.length() > ref.length() )
        return false;

    auto offset = ref.size()-pattern.length();
    if ( caseSensitive )
    {
        for( unsigned i=0; i < (unsigned)pattern.length(); ++i )
        {
            if ( ref[i+offset] != pattern[i] )
                return false;
        }
    }
    else
    {
        for( auto i=0; i < pattern.length(); ++i )
        {
            if (toLower(ref[i + offset]) != toLower(pattern[i]))
                return false;
        }
    }
    return true;
}

std::string
rocky::util::getExecutableLocation()
{
#if defined(_WIN32)

    char buf[4096];
    if (::GetModuleFileName(0L, buf, 4096))
    {
        auto path = std::filesystem::path(std::string(buf));
        return path.remove_filename().generic_string();
    }

#elif defined(__linux__)

    return std::filesystem::canonical("/proc/self/exe").remove_filename().generic_string();

#endif

    return {};
}

namespace
{
    inline std::optional<std::string> _getEnvVar(std::string_view name) {
#if defined(_WIN32)
        char* buffer = nullptr;
        size_t size = 0;

        if (_dupenv_s(&buffer, &size, name.data()) == 0 && buffer) {
            std::string value(buffer);
            ::free(buffer);  // must free
            return value;
        }
#else
        if (const char* v = std::getenv(name.data()))
            return std::string(v);
#endif
        return std::nullopt;
    }
}

std::optional<std::string>
rocky::util::getEnvVar(std::string_view name)
{
    auto result = _getEnvVar(name);
    if (!result.has_value())
    {
        result = _getEnvVar("ROCKY_" + std::string(name));
    }
    return result;
}

bool
rocky::util::isEnvVarSet(const char* name)
{
    return getEnvVar(name).has_value();
}


void
rocky::util::setThreadName(const char* name)
{
#if (defined _WIN32 && defined _WIN32_WINNT_WIN10 && defined _WIN32_WINNT && _WIN32_WINNT >= _WIN32_WINNT_WIN10) || (defined __CYGWIN__)
    wchar_t buf[256];
    size_t converted = 0;
    auto err = mbstowcs_s(&converted, buf, 256, name, _TRUNCATE);
    if (err != 0)
        return;

    // Look up the address of the SetThreadDescription function rather than using it directly.
    typedef ::HRESULT(WINAPI* SetThreadDescription)(::HANDLE hThread, ::PCWSTR lpThreadDescription);
    auto set_thread_description_func = reinterpret_cast<SetThreadDescription>(::GetProcAddress(::GetModuleHandle("Kernel32.dll"), "SetThreadDescription"));
    if (set_thread_description_func)
    {
        set_thread_description_func(::GetCurrentThread(), buf);
    }

#elif defined _GNU_SOURCE && !defined __EMSCRIPTEN__ && !defined __CYGWIN__
    const auto sz = strlen(name);
    if (sz <= 15)
    {
        pthread_setname_np(pthread_self(), name);
    }
    else
    {
        char buf[16];
        memcpy(buf, name, 15);
        buf[15] = '\0';
        pthread_setname_np(pthread_self(), buf);
    }
#endif
}

BackgroundServices::Promise
BackgroundServices::start(const std::string& name, Function function)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(function, {});
    ROCKY_SOFT_ASSERT_AND_RETURN(!name.empty(), {});

    std::lock_guard lock(mutex);

    // wrap with a delegate function so we can control the semaphore
    auto delegate = [this, function, name](jobs::cancelable& cancelable) -> bool
        {
            ++semaphore;
            function(cancelable);
            --semaphore;
            return true;
        };

    jobs::context context{ std::string(name), jobs::get_pool(name, 1) };
    futures.emplace_back(jobs::dispatch(delegate, context));

    return futures.back();
}

void
BackgroundServices::quit()
{
    std::lock_guard lock(mutex);

    // tell all tasks to cancel
    for (auto& f : futures)
        f.abandon();

    // block until all the background tasks exit.
    semaphore.join();

    futures.clear();
}

#ifdef ROCKY_HAS_ZLIB

// adapted from
// https://github.com/openscenegraph/OpenSceneGraph/blob/master/src/osgDB/Compressors.cpp
#undef CHUNK
#define CHUNK 32768

bool
ZLibCompressor::compress(const std::string& src, std::ostream& fout) const
{
    int ret, flush = Z_FINISH;
    unsigned have;
    z_stream strm;
    unsigned char out[CHUNK];

    int level = 6;
    int stategy = Z_DEFAULT_STRATEGY;

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit2(&strm, level, Z_DEFLATED,
        15 + 16, // +16 to use gzip encoding
        8, // default
        stategy);
    if (ret != Z_OK) return false;

    strm.avail_in = (uInt)src.size();
    strm.next_in = (Bytef*)(&(*src.begin()));

    /* run deflate() on input until output buffer not full, finish
       compression if all of source has been read in */
    do
    {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = deflate(&strm, flush);    /* no bad return value */

        if (ret == Z_STREAM_ERROR)
        {
            return false;
        }

        have = CHUNK - strm.avail_out;
        if (have > 0)
            fout.write((const char*)out, have);

        if (fout.fail())
        {
            (void)deflateEnd(&strm);
            return false;
        }
    } while (strm.avail_out == 0);

    /* clean up and return */
    (void)deflateEnd(&strm);

    return true;
}

bool
ZLibCompressor::decompress(std::istream& fin, std::string& target) const
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm,
        15 + 32); // autodetected zlib or gzip header

    if (ret != Z_OK)
    {
        return ret != 0;
    }

    /* decompress until deflate stream ends or end of file */
    do
    {
        fin.read((char*)in, CHUNK);
        strm.avail_in = (uInt)fin.gcount();
        if (strm.avail_in == 0) break;

        /* run inflate() on input until output buffer not full */
        strm.next_in = in;
        do
        {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);

            switch (ret)
            {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return false;
            }
            have = CHUNK - strm.avail_out;
            target.append((char*)out, have);
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? true : false;
}

#endif // ROCKY_HAS_ZLIB
