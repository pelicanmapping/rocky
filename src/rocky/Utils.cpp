/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Utils.h"
#include "sha1.h"
#include "Instance.h"
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef WIN32
#include <Windows.h>
#endif

#ifdef ROCKY_HAS_ZLIB
#include <zlib.h>
ROCKY_ABOUT(zlib, ZLIB_VERSION)
#endif

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

StringTokenizer::StringTokenizer(
    const std::string& delims,
    const std::string& quotes) :

    _allowEmpties(true),
    _trimTokens(true)
{
    addDelims(delims);
    addQuotes(quotes);
}

StringTokenizer::StringTokenizer(
    const std::string& input,
    std::vector<std::string>& output,
    const std::string& delims,
    const std::string& quotes,
    bool               allowEmpties,
    bool               trimTokens) :

    _allowEmpties(allowEmpties),
    _trimTokens(trimTokens)
{
    addDelims(delims);
    addQuotes(quotes);
    tokenize(input, output);
}

StringTokenizer::StringTokenizer(
    const std::string& input,
    std::unordered_map<std::string, std::string>& output,
    const std::string& delims,
    const std::string& seps,
    const std::string& quotes,
    bool               allowEmpties,
    bool               trimTokens) :

    _allowEmpties(allowEmpties),
    _trimTokens(trimTokens)
{
    addDelims(delims);
    addQuotes(quotes);
    std::vector<std::string> pairs;
    tokenize(input, pairs);

    for (auto& pair : pairs)
    {
        _delims.clear();
        addDelims(seps);
        std::vector<std::string> keyvalue;
        tokenize(pair, keyvalue);
        if (keyvalue.size() == 2)
            output[keyvalue[0]] = keyvalue[1];
    }
}

void
StringTokenizer::addDelim( char delim, bool keep )
{
    _delims[delim] = keep;
}

void
StringTokenizer::addDelims( const std::string& delims, bool keep )
{
    for( unsigned i=0; i<delims.size(); ++i )
        addDelim( delims[i], keep );
}

void
StringTokenizer::addQuote( char quote, bool keep )
{
    _quotes[quote] = keep;
}

void
StringTokenizer::addQuotes( const std::string& quotes, bool keep )
{
    for( unsigned i=0; i<quotes.size(); ++i )
        addQuote( quotes[i], keep );
}

void
StringTokenizer::tokenize( const std::string& input, std::vector<std::string>& output ) const
{
    output.clear();

    std::stringstream buf;
    bool quoted = false;
    char lastQuoteChar = '\0';

    for( std::string::const_iterator i = input.begin(); i != input.end(); ++i )
    {
        char c = *i;

        TokenMap::const_iterator q = _quotes.find( c );

        if ( quoted )
        {
            if( q != _quotes.end() && lastQuoteChar == c)
            {
                quoted = false;
                lastQuoteChar = '\0';
                if ( q->second )
                    buf << c;
            }
            else
            {
                buf << c;
            }
        }
        else
        {
            if ( q != _quotes.end() )
            {
                quoted = true;
                lastQuoteChar = c;
                if ( q->second )
                    buf << c;
            }
            else
            {
                TokenMap::const_iterator d = _delims.find( c );
                if ( d == _delims.end() )
                {
                    buf << c;
                }
                else
                {
                    std::string token = buf.str();
                    if ( _trimTokens )
                        trim_in_place( token );

                    if ( _allowEmpties || !token.empty() )
                        output.push_back( token );

                    if ( d->second == true )
                    {
                        output.push_back( std::string(1, c) );
                    }

                    buf.str("");
                }
            }
        }
    }

    std::string bufstr = buf.str();
    if ( _trimTokens )
        trim_in_place( bufstr );
    if ( !bufstr.empty() )
        output.push_back( bufstr );
}

//--------------------------------------------------------------------------

std::string
rocky::util::toLegalFileName(const std::string& input, bool allowSubdirs, const char* replacementChar)
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
rocky::util::hashString( const std::string& input )
{
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    unsigned int len = input.length();
    const char* data = input.c_str();
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
    const std::string& sub,
    const std::string& other)
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
    const std::string& pattern,
    const std::string& replacement)
{
    if (pattern.empty()) return s;

    std::string upperSource = s;
    std::transform(upperSource.begin(), upperSource.end(), upperSource.begin(), (int(*)(int))std::toupper);

    std::string upperPattern = pattern;
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
rocky::util::trim( const std::string& in )
{
    std::string str = in;
    trim_in_place( str );
    return str;
}

/** Returns a lower-case version of the input string. */
std::string
rocky::util::toLower( const std::string& input )
{
    std::string output = input;
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
rocky::util::ciEquals(const std::string& lhs, const std::string& rhs, const std::locale& loc )
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
rocky::util::startsWith( const std::string& ref, const std::string& pattern, bool caseSensitive, const std::locale& loc )
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
rocky::util::endsWith( const std::string& ref, const std::string& pattern, bool caseSensitive, const std::locale& loc )
{
    if ( pattern.length() > ref.length() )
        return false;

    unsigned offset = ref.size()-pattern.length();
    if ( caseSensitive )
    {
        for( unsigned i=0; i < pattern.length(); ++i )
        {
            if ( ref[i+offset] != pattern[i] )
                return false;
        }
    }
    else
    {
        for( unsigned i=0; i < pattern.length(); ++i )
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
rocky::util::writeToFile(const std::string& data, const std::string& filename)
{
    std::ofstream out(filename);
    if (out.fail())
        return false;
    out << data;
    out.close();
    return true;
}

bool
rocky::util::readFromFile(std::string& data, const std::string& filename)
{
    std::ifstream in(filename, std::ios_base::binary);
    if (in.fail())
        return false;
    std::stringstream buf;
    buf << in.rdbuf();
    data = buf.str();
    in.close();
    return true;
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

    strm.avail_in = src.size();
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
        strm.avail_in = fin.gcount();
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
