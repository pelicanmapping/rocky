/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Utils.h"
#include "sha1.h"
#include "Context.h"
#include <cctype>
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
                            trim_in_place(token);

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

#if 0

    for (auto& c : input)
    {
        ++offset;
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
                quote_opener_offset = offset - 1;

                if (keep_quote_char)
                    buf << c;
            }
            else
            {
                auto d = _delims.find(c);
                if (d == _delims.end())
                {
                    buf << c;
                }
                else
                {
                    // found a delimiter. end the current token.
                    std::string token = buf.str();
                    if (_trimTokens)
                        trim2(token);

                    if (_allowEmpties || !token.empty())
                        output.push_back(token);

                    if (d->second == true) // keep the delimiter itself as a token?
                    {
                        output.push_back(std::string(1, c));
                    }

                    buf.str("");
                }
            }
        }
    }
#endif

    if (inside_quote)
    {
        Log()->warn("[Tokenizer] unterminated quote in string ({} at offset {}) : {}",
            quote_opener, quote_opener_offset, input);

        if (error)
            *error = true;
    }

    std::string bufstr = buf.str();
    if (_trimTokens)
        trim_in_place(bufstr);
    if (!bufstr.empty())
        output.push_back(bufstr);

    return output;
}

//--------------------------------------------------------------------------

std::string
rocky::util::toLegalFileName(std::string_view input, bool allowSubdirs, const char* replacementChar)
{
    // See: http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_282
    // We omit '-' so we can use it for the HEX identifier.
    static const std::string legalWithoutSubdirs("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.");
    static const std::string legalWithDirs      ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./");

    
    std::size_t pos = input.find("://");
    pos = pos == std::string::npos ? 0 : pos+3;

    const std::string& legal = allowSubdirs? legalWithDirs : legalWithoutSubdirs;

    std::stringstream buf;
    for( ; pos < input.size(); ++pos )
    {
        std::string::const_reference c = input[pos];
        if (legal.find(c) != std::string::npos)
        {
            buf << c;
        }
        else
        {
            if (replacementChar)
                buf << (char)(*replacementChar);
            else
                buf << "-" << std::hex << static_cast<unsigned>(c) << "-";
        }
    }

    std::string result;
    result = buf.str();

    return result;
}

/** MurmurHash 2.0 (http://sites.google.com/site/murmurhash/) */
unsigned
rocky::util::hashString(std::string_view input )
{
    const unsigned m = 0x5bd1e995;
    const int r = 24;
    unsigned len = (unsigned)input.length();
    const char* data = input.data();
    unsigned int h = m ^ len; // using "m" as the seed.

    while(len >= 4)
    {
        unsigned int k = *(unsigned int *)data;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        len -= 4;
    }

    switch(len)
    {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
        h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

/** Replaces all the instances of "sub" with "other" in "s". */
std::string&
rocky::util::replace_in_place(
    std::string& s,
    std::string_view sub,
    std::string_view other)
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

std::string&
rocky::util::replace_in_place_case_insensitive(
    std::string& s,
    std::string_view pattern,
    std::string_view replacement)
{
    if (pattern.empty()) return s;

    std::string upperSource(s);
    std::transform(upperSource.begin(), upperSource.end(), upperSource.begin(), (int(*)(int))std::toupper);

    std::string upperPattern(pattern);
    std::transform(upperPattern.begin(), upperPattern.end(), upperPattern.begin(), (int(*)(int))std::toupper);

    for (size_t b = 0; ; )
    {
        b = upperSource.find(upperPattern, b);
        if (b == s.npos) break;
        s.replace(b, pattern.size(), replacement);
        upperSource.replace(b, upperPattern.size(), replacement);
        b += replacement.size();
    }

    return s;
}

/**
* Trims whitespace from the ends of a string.
* by Rodrigo C F Dias
* http://www.codeproject.com/KB/stl/stdstringtrim.aspx
*/
void
rocky::util::trim_in_place( std::string& str )
{
    static const std::string whitespace (" \t\f\v\n\r");
    std::string::size_type pos = str.find_last_not_of( whitespace );
    if(pos != std::string::npos) {
        str.erase(pos + 1);
        pos = str.find_first_not_of( whitespace );
        if(pos != std::string::npos) str.erase(0, pos);
    }
    else str.erase(str.begin(), str.end());
}

/**
* Trims whitespace from the ends of a string, returning a
* copy of the string with whitespace removed.
*/
std::string
rocky::util::trim(std::string_view in)
{
    std::string str(in);
    trim_in_place(str);
    return str;
}

/** Returns a lower-case version of the input string. */
std::string
rocky::util::toLower(std::string_view input )
{
    std::string output(input);
    std::transform( output.begin(), output.end(), output.begin(), ::tolower );
    return output;
}

namespace
{
    template<typename charT>
    struct ci_equal {
        ci_equal( const std::locale& loc ) : _loc(loc) { }
        bool operator()(charT c1, charT c2) {
            return std::toupper(c1,_loc) == std::toupper(c2,_loc);
        }
        const std::locale& _loc;
    };
}

bool
rocky::util::ciEquals(std::string_view lhs, std::string_view rhs, const std::locale& loc )
{
    if ( lhs.length() != rhs.length() )
        return false;

    for( unsigned i=0; i<lhs.length(); ++i )
    {
        if ( std::toupper(lhs[i], loc) != std::toupper(rhs[i], loc) )
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
rocky::util::startsWith(std::string_view ref, std::string_view pattern, bool caseSensitive, const std::locale& loc )
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
            if ( std::toupper(ref[i], loc) != std::toupper(pattern[i],loc) )
                return false;
        }
    }
    return true;
}

bool
rocky::util::endsWith(std::string_view ref, std::string_view pattern, bool caseSensitive, const std::locale& loc )
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
            if ( std::toupper(ref[i+offset], loc) != std::toupper(pattern[i],loc) )
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

bool
rocky::util::writeToFile(std::string_view data, std::string_view filename)
{
    std::ofstream out(filename.data());
    if (out.fail())
        return false;
    out << data;
    out.close();
    return true;
}

Result<std::string>
rocky::util::readFromFile(std::string_view filename)
{
    std::ifstream in(filename.data(), std::ios_base::binary);
    if (in.fail())
        return Status(Status::ResourceUnavailable);
    std::stringstream buf;
    buf << in.rdbuf();
    auto data = buf.str();
    in.close();
    return data;
}

namespace
{
    inline std::string _getEnvVar(const std::string& name)
    {
        const char* value = std::getenv(name.c_str());
        return value ? std::string(value) : std::string();
    }
}

std::string
rocky::util::getEnvVar(const char* name)
{
    auto result = _getEnvVar(name);
    if (result.empty())
    {
        result = _getEnvVar("ROCKY_" + std::string(name));
    }
    return result;
}

bool
rocky::util::isEnvVarSet(const char* name)
{
    return !getEnvVar(name).empty();
}


void
rocky::util::setThreadName(const char* name)
{
#if (defined _WIN32 && defined _WIN32_WINNT_WIN10 && defined _WIN32_WINNT && _WIN32_WINNT >= _WIN32_WINNT_WIN10) || (defined __CYGWIN__)
    wchar_t buf[256];
    mbstowcs(buf, name, 256);

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
