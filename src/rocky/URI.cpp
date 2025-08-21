/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#define NOMINMAX

#include "URI.h"
#include "Utils.h"
#include "Context.h"
#include "Version.h"
#include "json.h"

#include <fstream>
#include <sstream>
#include <random>

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

#ifdef ROCKY_HAS_CURL
    #include <curl/curl.h>
    ROCKY_ABOUT(curl, LIBCURL_VERSION)
#endif

#define LC "[URI] "

using namespace ROCKY_NAMESPACE;
using namespace std::chrono_literals;

bool URI::supportsHTTPS()
{
    //todo: query CURL for the same info?
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    return true;
#else
    return false;
#endif
}

// Adapted from https://oroboro.com/image-format-magic-bytes
std::string
URI::inferContentType(const std::string& buffer)
{
    if (buffer.length() < 16)
        return {};

    if (!strncmp(buffer.c_str(), "<?xml", 5))
        return "text/xml";

    if (!strncmp(buffer.c_str(), "<html", 5))
        return "text/html";

    // .jpg:  FF D8 FF
    // .png:  89 50 4E 47 0D 0A 1A 0A
    // .gif:  GIF87a
    //        GIF89a
    // .tiff: 49 49 2A 00
    //        4D 4D 00 2A
    // .bmp:  BM
    // .webp: RIFF ???? WEBP
    // .ico   00 00 01 00
    //        00 00 02 00 ( cursor files )
    const char* data = buffer.c_str();
    switch (buffer[0])
    {
    case '\xFF':
        return (!strncmp((const char*)data, "\xFF\xD8\xFF", 3)) ? "image/jpg" : "";

    case '\x89':
        return (!strncmp((const char*)data,
            "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8)) ? "image/png" : "";

    case 'G':
        return (!strncmp((const char*)data, "GIF87a", 6) ||
            !strncmp((const char*)data, "GIF89a", 6)) ? "image/gif" : "";

    case 'I':
        return (!strncmp((const char*)data, "\x49\x49\x2A\x00", 4)) ? "image/tif" : "";

    case 'M':
        return (!strncmp((const char*)data, "\x4D\x4D\x00\x2A", 4)) ? "image/tif" : "";

    case 'B':
        return ((data[1] == 'M')) ? "image/bmp" : "";

    case 'R':
        return (!strncmp((const char*)data, "RIFF", 4)) ? "image/webp" : "";
    }


    return { };
}

namespace
{
    static bool httpDebug = !ROCKY_NAMESPACE::util::getEnvVar("HTTP_DEBUG").empty();

    std::string inferContentTypeFromFileExtension(const std::string& filename)
    {
        ROCKY_TODO("Map the extension to a known mime type");
        auto dot = filename.find_last_of('.');
        if (dot != std::string::npos)
            return filename.substr(dot + 1);
        else
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
        std::vector<KeyValuePair> headers;
    };

    std::string findHeader(const std::vector<KeyValuePair>& headers, const std::string& name)
    {
        for (auto& h : headers)
        {
            if (util::ciEquals(h.name, name))
                return h.value;
        }
        return {};
    }

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

#ifdef ROCKY_HAS_CURL

    struct stream_object
    {
        void write(const char* ptr, size_t realsize)
        {
            stream.write(ptr, realsize);
        }

        void writeHeader(const char* ptr, size_t realsize)
        {
            std::string header(ptr);
            std::size_t colon = header.find_first_of(':');
            if (colon != std::string::npos && colon > 0 && colon < header.length() - 1)
            {
                headers.emplace_back(KeyValuePair{
                    util::trim(header.substr(0, colon)),
                    util::trim(header.substr(colon + 1)) });
            }
        }

        std::stringstream stream;
        std::vector<KeyValuePair> headers;
    };

    static size_t stream_object_write_function(void* ptr, size_t size, size_t nmemb, void* data)
    {
        size_t realsize = size * nmemb;
        stream_object* sp = (stream_object*)data;
        sp->write((const char*)ptr, realsize);
        return realsize;
    }

    static size_t stream_object_header_function(void* ptr, size_t size, size_t nmemb, void* data)
    {
        size_t realsize = size * nmemb;
        stream_object* sp = (stream_object*)data;
        sp->writeHeader((const char*)ptr, realsize);
        return realsize;
    }

    struct CURLHandle
    {
        CURL* handle = nullptr;
        CURLHandle() {
            handle = curl_easy_init();
        }
        ~CURLHandle() {
            if (handle)
                curl_easy_cleanup(handle);
        }
    };

    Result<HTTPResponse> http_get_curl(const HTTPRequest& request, const IOOptions& io)
    {
        // use thread-local clients for connection reuse.
        static thread_local struct Basket {
            CURL* handle = nullptr;
            ~Basket() { if (handle) curl_easy_cleanup(handle); }
        } basket;

        HTTPResponse response;

        CURL* handle = basket.handle;
        if (!handle)
        {
            handle = curl_easy_init();

            curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, stream_object_write_function);
            curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, stream_object_header_function);
            curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, (void*)1);
            curl_easy_setopt(handle, CURLOPT_MAXREDIRS, (void*)5);
            curl_easy_setopt(handle, CURLOPT_FILETIME, true);
            curl_easy_setopt(handle, CURLOPT_USERAGENT, "rocky/" ROCKY_VERSION_STRING);

            // Enable automatic CURL decompression of known types.
            // An empty string will automatically add all supported encoding types that are built into CURL.
            // Note that you must have CURL built against zlib to support gzip or deflate encoding.
            curl_easy_setopt(handle, CURLOPT_ENCODING, "");

            // Disable peer certificate verification to allow us to access  https servers
            // where the peer certificate cannot be verified.
            curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, (void*)0);

            basket.handle = handle;
        }

        //todo: authentication
        //todo: proxy server

        // request headers:
        struct curl_slist* headers = nullptr;
        for (auto& h : request.headers)
        {
            std::string header = h.name + ": " + h.value;
            headers = curl_slist_append(headers, header.c_str());
        }
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str());

        stream_object so;
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*)&so);
        curl_easy_setopt(handle, CURLOPT_HEADERDATA, (void*)&so);

        char errorBuf[CURL_ERROR_SIZE];
        errorBuf[0] = 0;
        curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, (void*)errorBuf);

        CURLcode result = CURLE_OK;
        std::random_device device;
        std::default_random_engine engine(device());
        std::uniform_real_distribution distribution;

        auto t0 = std::chrono::steady_clock::now();

        auto max_attempts = std::max(1u, io.maxNetworkAttempts);
        unsigned attempts = 0;
        while(attempts++ < max_attempts)
        {
            if (attempts > 1)
            {
                auto delay = 1000ms * std::pow(2, attempts + distribution(engine));
                if (!io.canceled())
                    std::this_thread::sleep_for(delay);
            }

            result = curl_easy_perform(handle);

            if (result == CURLE_COULDNT_CONNECT || result == CURLE_OPERATION_TIMEDOUT)
            {
                continue;
            }

            curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status);

            if (response.status == 429) // TOO MANY REQUESTS (rate limiting)
            {
                continue;
            }

            break;
        }

        //curl_easy_cleanup(handle);

        if (result == CURLE_OK)
        {
            response.data = so.stream.str();
            response.headers = so.headers;

            auto t1 = std::chrono::steady_clock::now();
            if (httpDebug)
            {
                auto dur_ms = 1e-6 * (double)(t1 - t0).count();
                auto cti = findHeader(response.headers, "Content-Type");
                auto ct = cti.empty() ? "unknown" : cti;
                Log()->info(LC "({} {:3d}ms {:6}b {}) HTTP GET {}", response.status, (int)dur_ms, response.data.size(), ct, request.url);
            }

            if (response.status != 200)
            {
                if (response.status == 404) // NOT FOUND (permanent)
                {
                    return Failure(Failure::ResourceUnavailable, request.url);
                }
                else
                {
                    return Failure(Failure::ResourceUnavailable, std::to_string(response.status));
                }
            }
        }
        else
        {
            return Failure(Failure::ServiceUnavailable, errorBuf);
        }

        return response;
    }
#endif

#ifdef ROCKY_HAS_HTTPLIB
    Result<HTTPResponse> http_get_httplib(const HTTPRequest& request, const IOOptions& io)
    {
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
            return Failure_ConfigurationError;

        httplib::Params params;
        httplib::detail::parse_query_text(query_text, params);

        HTTPResponse response;

        try
        {
            // use thread-local clients so we can do keep-alive for high performance
            static thread_local httplib::Client client("");
            static thread_local std::string last_proto_host_port;

            if (proto_host_port != last_proto_host_port)
            {
                client = httplib::Client(proto_host_port);
                last_proto_host_port = proto_host_port;
            }

            // follow redirects
            client.set_follow_location(true);

            // disable cert verification
            client.enable_server_certificate_verification(false);
            //client.enable_server_hostname_verification(false);

            // ask the server to keep the connection alive
            client.set_keep_alive(true);

            unsigned max_attempts = std::max(1u, io.maxNetworkAttempts);

            unsigned tooManyRequestsCount = 0;
            std::random_device device;
            std::default_random_engine engine(device());
            std::uniform_real_distribution distribution;

            for(;;)
            {
                if (io.canceled())
                    return Failure_OperationCanceled;
                
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
                        auto cacahestatusi = res->headers.find("Cf-Cache-Status");
                        auto cachestatus = cacahestatusi != res->headers.end() ? cacahestatusi->second : "";
                        if (cachestatus.empty()) {
                            auto xcachei = res->headers.find("X-Cache");
                            cachestatus = xcachei != res->headers.end() ? xcachei->second : "";
                        }
                        Log()->info(LC "({} {:3d}ms {:6d}b {}) HTTP GET {} ({})", res->status, (int)dur_ms, res->body.size(), ct, request.url, cachestatus);
                    }

                    if (res->status == 404) // NOT FOUND (permanent)
                    {
                        return Failure(Failure::ResourceUnavailable, httplib::status_message(res->status));
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
                            return Failure(Failure::ResourceUnavailable, httplib::status_message(res->status));
                        }
                    }
                    else if (res->status != 200)
                    {
                        return Failure(Failure::GeneralError, httplib::status_message(res->status));
                    }

                    response.status = res->status;

                    for (auto& h : res->headers)
                        response.headers.emplace_back(KeyValuePair{ h.first, h.second });

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

                    return Failure(Failure::ServiceUnavailable, httplib::to_string(res.error()));
                }
            }

        }
        catch (std::exception& ex)
        {
            return Failure(Failure::GeneralError, ex.what());
        }

        return std::move(response);
    }
#endif

    Result<HTTPResponse> http_get(const HTTPRequest& request, const IOOptions& io)
    {
#if defined(ROCKY_HAS_HTTPLIB)
        return http_get_httplib(request, io);
#elif defined(ROCKY_HAS_CURL)
        return http_get_curl(request, io);
#else
        return Failure(Failure::ServiceUnavailable, "HTTP not supported without curl or httplib");
#endif
    }
}

//------------------------------------------------------------------------

URI::Stream::Stream(std::shared_ptr<std::istream> s) :
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

//URI::URI(const std::string& location)
//{
//    _baseURI = location;
//    _fullURI = location;
//
//    findRotation();
//}

URI::URI(const std::string& location, const URI::Context& context)
{
    set(location, context);
}

void
URI::set(std::string_view location, const URI::Context& context)
{
    std::string location_to_use(location);

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


auto URI::read(const IOOptions& io) const -> Result<URIResponse>
{
    // protect against multiple threads trying to read the same URI at the same time
    util::ScopedGate<std::string> gate(io.services().uriGate, full());

    if (io.services().contentCache)
    {
        auto cached = io.services().contentCache->get(full());
        if (cached.has_value() && cached->ok())
        {
            Result<URIResponse> result(cached->value());
            result->fromCache = true;
            return result;
        }
    }

    Content content;

    // check the dead pool, if available.
    if (io.services().deadpool)
    {
        if (auto r = io.services().deadpool->get(full()))
        {
            return r.value();
        }
    }

    if (std::filesystem::exists(full()))
    {
        auto contentType = inferContentTypeFromFileExtension(full());

        ROCKY_TODO("worry about text or binary open mode?");

        std::ifstream in(full().c_str(), std::ios_base::in);
        std::stringstream buf;
        buf << in.rdbuf() << std::flush;
        content.data = buf.str();
        content.type = contentType;
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
            util::replaceInPlace(
                request.url,
                request.url.substr(_r0, _r1 - _r0 + 1),
                request.url.substr(_r0 + 1 + (rotator++ % (_r1 - _r0 - 1)), 1));
        }

        // make the actual request:
        auto r = http_get(request, io);

        if (r.failed())
        {
            // if the error is unrecoverable, deadpool it.
            if (io.services().deadpool && r.error().type == Failure::ResourceUnavailable)
            {
                io.services().deadpool->put(full(), r.error());
            }

            return r.error();
        }

        std::string contentType = findHeader(r.value().headers, "Content-Type");

        if (contentType.empty())
        {
            contentType = inferContentType(r.value().data);
        }

        if (contentType.empty())
        {
            auto p = request.url.find_first_of('?');
            auto url_path = p != std::string::npos ? request.url.substr(0, p) : request.url;
            contentType = inferContentTypeFromFileExtension(url_path);
        }

        content.type = std::move(contentType);
        content.data = std::move(r.value().data);
    }
    else
    {
        return Failure(Failure::ResourceUnavailable, full());
    }

    if (io.services().contentCache)
    {
        io.services().contentCache->put(full(), Result<Content>(content));
    }

    return URIResponse(content);
}

bool
URI::isRemote() const
{
    auto temp = util::trim(util::toLower(_fullURI));
    return
        util::startsWith(temp, "http://") ||
        util::startsWith(temp, "https://");
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
            URI::Context context{ referrer };
            if (j.contains("headers")) {
                auto headers = j.at("headers");
                if (headers.is_array()) {
                    for (auto i = headers.begin(); i != headers.end(); ++i)
                        context.headers.emplace_back(i.key(), i.value());
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
bool ROCKY_NAMESPACE::get_to(const json& obj, const char* name, rocky::option<URI>& var, const IOOptions& io)
{
    bool ok = get_to(obj, name, var);
    if (ok && io.referrer.has_value())
        var->setReferrer(io.referrer.value());
    return ok;
}
