/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "URI.h"
#include "Utils.h"
#include "Instance.h"
#include "Version.h"
#include "json.h"

#include <typeinfo>
#include <fstream>
#include <sstream>
#include <cstdlib>

#ifdef ROCKY_HAS_HTTPLIB
#ifdef ROCKY_HAS_OPENSSL
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>
ROCKY_ABOUT(cpp_httplib, CPPHTTPLIB_VERSION)
#ifdef OPENSSL_VERSION_STR
ROCKY_ABOUT(openssl, OPENSSL_VERSION_STR)
#endif
#endif

#define LC "[URI] "

using namespace ROCKY_NAMESPACE;
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
    static bool httpDebug = !ROCKY_NAMESPACE::util::getEnvVar("ROCKY_HTTP_DEBUG").empty();

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

    IOResult<HTTPResponse> http_get(const HTTPRequest& request, const IOOptions& io)
    {
#ifndef ROCKY_HAS_HTTPLIB
        return Status(Status::ServiceUnavailable);
#else        
        httplib::Headers headers;

        for (auto& h : request.headers)
        {
            headers.insert(std::make_pair(h.name, h.value));
        }

        // insert a default User-Agent unless the user passed one in.
        if (headers.count("User-Agent") == 0)
        {
            headers.insert(std::make_pair("User-Agent", "rocky/" ROCKY_VERSION_STRING));
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

            unsigned max_attempts = std::max(1u, io.maxNetworkAttempts);

            unsigned tooManyRequestsCount = 0;
            std::random_device device;
            std::default_random_engine engine(device());
            std::uniform_real_distribution distribution;

            for(;;)
            {
                if (io.canceled())
                    return StatusOK;
                
                auto t0 = std::chrono::steady_clock::now();
                auto res = client.Get(path, params, headers);
                auto t1 = std::chrono::steady_clock::now();

                if (res)
                {
                    if (httpDebug)
                    {
                        auto dur_ms = 1e-6 * (double)(t1 - t0).count();
                        auto cti = res->headers.find("Content-Type");
                        auto ct = cti != res->headers.end() ? cti->second : "unknown";
                        Log()->info(LC "({} {}ms {}b {}) HTTP GET {}", res->status, (int)dur_ms, res->body.size(), ct, request.url);
                    }

                    if (res->status == 404) // NOT FOUND (permanent)
                    {
                        return Status(Status::ResourceUnavailable, httplib::status_message(res->status));
                    }
                    else if (res->status == 429) // TOO MANY REQUESTS (rate limiting)
                    {
                        if (--max_attempts > 0)
                        {
                            // random delay should avoid many requests failing at once, then waiting and retrying all at the same time and failing again
                            auto delay = 1000ms * std::pow(2, tooManyRequestsCount++ + distribution(engine));
                            Log()->debug(LC + std::string(httplib::status_message(res->status)) + " with " + proto_host_port + "; retrying with delay of " + std::to_string(delay.count()) + "ms... ");
                            if (!io.canceled())
                                std::this_thread::sleep_for(delay);
                            continue;
                        }
                        else
                        {
                            Log()->info(LC + std::string("Retries exhausted with ") + proto_host_port + path);
                            return Status(Status::ResourceUnavailable, httplib::status_message(res->status));
                        }
                    }
                    else if (res->status != 200)
                    {
                        return Status(Status::GeneralError, httplib::status_message(res->status));
                    }

                    response.status = res->status;

                    for (auto& h : res->headers)
                        response.headers[h.first] = h.second;

                    response.data = std::move(res->body);

                    break;
                }

                else
                {
                    if (httpDebug)
                    {
                        Log()->info(LC "(---) HTTP GET {:.2} ({})", request.url, httplib::to_string(res.error()));
                    }

                    // retry on a missing connection
                    if (res.error() == httplib::Error::Connection && (--max_attempts > 0))
                    {
                        Log()->info(LC + httplib::to_string(res.error()) + " with " + proto_host_port + "; retrying..");
                        std::this_thread::sleep_for(1s);
                        continue;
                    }

                    return Status(Status::ServiceUnavailable, httplib::to_string(res.error()));
                }
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
    _r0(rhs._r0),
    _r1(rhs._r1),
    _context(rhs._context)
{
    //nop
}

URI::URI(const std::string& location)
{
    _baseURI = location;
    _fullURI = location;

    findRotation();
}

URI::URI(const std::string& location, const URIContext& context)
{
    set(location, context);
}

void
URI::set(const std::string& location, const URIContext& context)
{
    std::string location_to_use = location;

    if (util::startsWith(location, "file://", false))
        _baseURI = location.substr(7);
    else
        _baseURI = location;

    _context = context;
    _fullURI = _baseURI;

    bool absolute_location =
        std::filesystem::path(_baseURI).is_absolute() ||
        util::toLower(_baseURI).substr(0, 7) == "http://" ||
        util::toLower(_baseURI).substr(0, 8) == "https://";

    // resolve a relative path using the referrer
    if (!absolute_location && !context.referrer.empty())
    {
        std::string referrer = context.referrer;

        // strip the network protocol if there is one
        std::string protocol;
        if (URI(referrer).isRemote())
        {
            auto pos = referrer.find_first_of('/');
            if (pos != std::string::npos)
            {
                protocol = referrer.substr(0, pos + 1);
                referrer = referrer.substr(pos + 1);
            }
        }

        std::filesystem::path p(referrer, std::filesystem::path::generic_format);
        p = p.remove_filename() / _baseURI;
        p = weakly_canonical(p);
        _fullURI = p.generic_string();

        // re-prepend the network protocol if necessary
        if (!protocol.empty())
        {
            _fullURI = protocol + _fullURI;
        }
    }

    findRotation();
}

void
URI::findRotation()
{
    if (isRemote())
    {
        auto i0 = _fullURI.find_first_of('[');
        if (i0 != std::string::npos)
        {
            auto i1 = _fullURI.find_first_of(']', i0);
            if (i1 != std::string::npos)
            {
                _r0 = i0;
                _r1 = i1;
            }
        }
    }
}

IOResult<Content>
URI::read(const IOOptions& io) const
{
    // protect against multiple threads trying to read the same URI at the same time
    util::ScopedGate<std::string> gate(io.uriGate, full());

    if (io.services.contentCache)
    {
        auto cached = io.services.contentCache->get(full());
        if (cached.status.ok())
        {
            if (httpDebug)
            {
                Log()->debug(LC "Cache hit, ratio = "
                    + std::to_string(100.0f * (float)io.services.contentCache->hits / (float)io.services.contentCache->gets)
                    + "% (" + full() + ")");
            }

            IOResult<Content> result(cached.value);
            result.fromCache = true;
            return result;
        }
    }

    Content content;

    if (std::filesystem::exists(full()))
    {
        auto contentType = inferContentTypeFromFileExtension(full());

        ROCKY_TODO("worry about text or binary open mode?");

        std::ifstream in(full().c_str(), std::ios_base::in);
        std::stringstream buf;
        buf << in.rdbuf() << std::flush;
        content.data = buf.str();
        content.contentType = contentType;
        in.close();
    }

    else if (isRemote())
    {
        HTTPRequest request{ full() };

        for(auto& header : _context.headers)
        {
            request.headers.push_back({ header.first, header.second });
        }

        // resolve a rotation:
        static int rotator = 0;
        if (_r0 != std::string::npos && _r1 != std::string::npos)
        {
            util::replace_in_place(
                request.url,
                request.url.substr(_r0, _r1 - _r0 + 1),
                request.url.substr(_r0 + 1 + (rotator++ % (_r1 - _r0 - 1)), 1));
        }

        // make the actual request:
        auto r = http_get(request, io);
        if (r.status.failed())
        {
            return IOResult<Content>::propagate(r);
        }

        std::string contentType;

        auto i = r.value.headers.find("Content-Type");
        if (i != r.value.headers.end())
            contentType = i->second;

        if (contentType.empty())
        {
            auto p = request.url.find_first_of('?');
            auto url_path = p != std::string::npos ? request.url.substr(0, p) : request.url;
            contentType = inferContentTypeFromFileExtension(url_path);
        }

        if (contentType.empty())
        {
            contentType = inferContentTypeFromData(r.value.data);
        }

        content = {
            contentType,
            r.value.data
        };
    }
    else
    {
        return Status(Status::ResourceUnavailable, full());
    }

    if (io.services.contentCache)
    {
        io.services.contentCache->put(full(), Result<Content>(content));
    }

    return content;
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

void
URI::setReferrer(const std::string& value)
{
    _context.referrer = value;
    set(_baseURI, _context);
}

#include "json.h"
namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const URI& obj)
    {
        if (obj.context().referrer.empty() && obj.context().headers.empty())
        {
            j = obj.base();
        }
        else
        {
            j = json::object();
            set(j, "href", obj.base());

            if (obj.context().headers.empty() == false) {
                auto headers = json::array();
                for (auto& h : obj.context().headers) {
                    headers.push_back({ h.first, h.second });
                }
                j["headers"] = headers;
            }
        }
    }

    void from_json(const json& j, URI& obj)
    {
        if (j.is_string())
        {
            obj = URI(get_string(j));
        }
        else
        {
            std::string base, referrer;
            get_to(j, "href", base);
            get_to(j, "referrer", referrer);
            URIContext context{ referrer };
            if (j.contains("headers")) {
                auto headers = j.at("headers");
                if (headers.is_array()) {
                    // correct??
                    for (auto i = headers.begin(); i != headers.end(); ++i)
                        context.headers[i.key()] = i.value();
                }
            }
            obj = URI(base, context);
        }
    }

    void to_json(json& j, const Hyperlink& obj)
    {
        j = json::object();
        set(j, "href", obj.href);
        set(j, "text", obj.text);    
    }

    void from_json(const json& j, Hyperlink& obj)
    {
        get_to(j, "href", obj.href);
        get_to(j, "text", obj.text);
    }
}

// specializations from json.h

bool ROCKY_NAMESPACE::get_to(const json& obj, const char* name, URI& var, const IOOptions& io)
{
    bool ok = get_to(obj, name, var);
    if (ok && io.referrer.has_value())
        var.setReferrer(io.referrer.value());
    return ok;
}
bool ROCKY_NAMESPACE::get_to(const json& obj, const char* name, rocky::optional<URI>& var, const IOOptions& io)
{
    bool ok = get_to(obj, name, var);
    if (ok && io.referrer.has_value())
        var->setReferrer(io.referrer.value());
    return ok;
}
