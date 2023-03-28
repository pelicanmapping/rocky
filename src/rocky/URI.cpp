/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "URI.h"
#include <typeinfo>
#include <fstream>
#include <sstream>

#ifdef HTTPLIB_FOUND
#ifdef OPENSSL_FOUND
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>
#endif

#define LC "[URI] "

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;
using namespace std::chrono_literals;

bool URI::supportsHTTPS()
{
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    return true;
#else
    return false;
#endif
}

namespace
{
    bool containsServerAddress(const std::string& input)
    {
        auto temp = util::trim(util::toLower(input));
        return
            util::startsWith(temp, "http://") ||
            util::startsWith(temp, "https://");
    }

    std::string inferContentTypeFromFileExtension(const std::string& filename)
    {
        ROCKY_TODO("Map the extension to a known mime type");
        auto dot = filename.find_last_of('.');
        if (dot != std::string::npos)
            return filename.substr(dot + 1);
        else
            return "";
    }
    
    std::string inferContentTypeFromData(const std::string& data)
    {
        ROCKY_TODO("Read the header bytes to infer the content type");
        return "";
    }

    struct KeyValuePair
    {
        std::string name;
        std::string value;
    };

    struct HTTPRequest
    {
        std::string url;
        std::vector<KeyValuePair> parameters;
        std::vector<KeyValuePair> headers;
    };

    struct HTTPResponse
    {
        int status;
        std::string data;
        std::unordered_map<std::string, std::string> headers;
    };

    bool split_url(
        const std::string& url,
        std::string& proto_host_port,
        std::string& path,
        std::string& query_text)
    {
        auto pos = url.find_first_of("://");
        if (pos == url.npos)
            return false;

        pos = url.find_first_of('/', pos + 3);
        if (pos == url.npos)
        {
            proto_host_port = url;
        }
        else
        {
            proto_host_port = url.substr(0, pos);
            auto query_pos = url.find_first_of('?', pos);
            if (query_pos == url.npos)
            {
                path = url.substr(pos);
            }
            else
            {
                path = url.substr(pos, query_pos - pos);
                if (query_pos < url.length())
                {
                    query_text = url.substr(query_pos + 1);
                }
            }
        }
        return true;
    }

    IOResult<HTTPResponse> http_get(const HTTPRequest& request, unsigned max_attempts)
    {
#ifndef HTTPLIB_FOUND
        ROCKY_TODO("Implement HTTP GET");
        return Status(Status::ServiceUnavailable);
#else        
        httplib::Headers headers;
        for (auto& h : request.headers)
        {
            headers.insert(std::make_pair(h.name, h.value));
        }

        std::string proto_host_port;
        std::string path;
        std::string query_text;

        if (!split_url(request.url, proto_host_port, path, query_text))
            return Status(Status::ConfigurationError);

        httplib::Params params;
        httplib::detail::parse_query_text(query_text, params);

        HTTPResponse response;

        try
        {
            httplib::Client client(proto_host_port);

            // follow redirects
            client.set_follow_location(true);

            // disable cert verification
            client.enable_server_certificate_verification(false);

            max_attempts = std::max(1u, max_attempts);

            for(;;)
            {
                auto r = client.Get(path, params, headers);

                if (r.error() != httplib::Error::Success)
                {
                    // retry on a missing connection
                    if (r.error() == httplib::Error::Connection && (--max_attempts > 0))
                    {
                        Log::info() << LC << httplib::to_string(r.error()) << " with " << proto_host_port << "; retrying.." << std::endl;
                        std::this_thread::sleep_for(1s);
                        continue;
                    }

                    return Status(Status::ServiceUnavailable, httplib::to_string(r.error()));
                }

                if (r->status == 404)
                {
                    return Status(Status::ResourceUnavailable, httplib::detail::status_message(r->status));
                }
                else if (r->status != 200)
                {
                    return Status(Status::GeneralError, httplib::detail::status_message(r->status));
                }

                response.status = r->status;

                for (auto& h : r->headers)
                    response.headers[h.first] = h.second;

                response.data = std::move(r->body);

                break;
            }

        }
        catch (std::exception& ex)
        {
            return Status(Status::GeneralError, ex.what());
        }

        return std::move(response);
#endif
    }
}


//------------------------------------------------------------------------

URIContext::URIContext()
{
}

URIContext::URIContext(const std::string& referrer) :
    _referrer(referrer)
{
}

URIContext::URIContext(const URIContext& rhs) :
    _referrer(rhs._referrer),
    _headers(rhs._headers)
{
}

std::string
URIContext::getCanonicalPath(const std::string& target) const
{
    ROCKY_TODO("nyi - resolve relative path into full absolute path");
    return target;
}

void
URIContext::addHeader(const std::string& name, const std::string& value)
{
    _headers[name] = value;
}

const Headers&
URIContext::getHeaders() const
{
    return _headers;
}

Headers&
URIContext::getHeaders()
{
    return _headers;
}


URIContext
URIContext::add(const URIContext& sub) const
{
    ROCKY_TODO("NYI");
    return *this;
    //return URIContext(rocky::getFullPath(_referrer, sub._referrer));
}

URIContext
URIContext::add(const std::string& sub) const
{
    ROCKY_TODO("NYI");
    return *this;
    //return URIContext(osgDB::concatPaths(_referrer, sub));
}

//------------------------------------------------------------------------

URI::Stream::Stream(shared_ptr<std::istream> s) :
    _in(s)
{
    //nop
}

std::string
URI::Stream::to_string()
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), "");

    std::string s;
    std::ostringstream os;
    os << _in->rdbuf();
    return os.str();
}

URI::URI()
{
    //nop
}

URI::URI(const URI& rhs) :
    _baseURI(rhs._baseURI),
    _fullURI(rhs._fullURI),
    _context(rhs._context)
{
    //nop
}

URI::URI(const std::string& location)
{
    _baseURI = location;
    _fullURI = location;
}

URI::URI(const std::string& location, const URIContext& context)
{
    _context = context;
    _baseURI = location;
    _fullURI = location;
    if (!isRemote())
        _fullURI = context.getCanonicalPath(_baseURI);
}

URI::URI(const char* location)
{
    _baseURI = std::string(location);
    _fullURI = _baseURI;
}

URI
URI::append(const std::string& suffix) const
{
    URI result;
    result._baseURI = _baseURI + suffix;
    result._fullURI = _fullURI + suffix;
    result._context = _context;
    return result;
}

IOResult<URI::Content>
URI::read(const IOOptions& io) const
{
    if (containsServerAddress(full()))
    {
        HTTPRequest request{ full() };
        auto r = http_get(request, io.maxNetworkAttempts);
        if (r.status.failed())
        {
            return IOResult<Content>::propagate(r);
        }

        std::string contentType;

        auto i = r.value.headers.find("Content-Type");
        if (i != r.value.headers.end())
            contentType = i->second;
        else
            contentType = inferContentTypeFromFileExtension(full());

        if (contentType.empty())
            contentType = inferContentTypeFromData(r.value.data);

        return Content
        {
            std::shared_ptr<std::istream>(
                new std::stringstream(r.value.data, std::ios_base::in)),
            contentType
        };
    }
    else
    {
        auto contentType = inferContentTypeFromFileExtension(full());

        ROCKY_TODO("worry about text or binary open mode?");

        return Content
        {
            std::shared_ptr<std::istream>(
                new std::ifstream(full().c_str(), std::ios_base::in)),
            contentType
        };
    }
}

bool
URI::isRemote() const
{
    return containsServerAddress(_fullURI);
}

std::string
URI::urlEncode(const std::string& value)
{
    return httplib::detail::encode_url(value);
}


#include "json.h"
namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const URI& obj) {
        if (obj.context().referrer().empty() && obj.context().getHeaders().empty())
        {
            j = obj.base();
        }
        else
        {
            j = json::object();
            set(j, "href", obj.base());

            if (!obj.context().referrer().empty())
                set(j, "referrer", obj.context().referrer());

            if (obj.context().getHeaders().empty() == false) {
                auto headers = json::array();
                for (auto& h : obj.context().getHeaders()) {
                    headers.push_back({ h.first, h.second });
                }
                j["headers"] = headers;
            }
        }
    }

    void from_json(const json& j, URI& obj) {
        if (j.is_string())
        {
            obj = URI(get_string(j));
        }
        else
        {
            std::string base, referrer;
            get_to(j, "href", base);
            get_to(j, "referrer", referrer);
            URIContext context(referrer);
            if (j.contains("headers")) {
                auto headers = j.at("headers");
                if (headers.is_array()) {
                    // correct??
                    for (auto i = headers.begin(); i != headers.end(); ++i)
                        context.addHeader(i.key(), i.value());
                }
            }
            obj = URI(base, context);
        }
    }
}
