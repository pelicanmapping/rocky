/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "URI.h"
#include "Notify.h"
#include <typeinfo>
#include <fstream>
#include <sstream>

#define LC "[URI] "

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;


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
        std::string data;
        std::unordered_map<std::string, std::string> headers;
    };

    IOResult<HTTPResponse> http_get(const HTTPRequest& request)
    {
        ROCKY_TODO("Implement HTTP GET");
        return Status(Status::ServiceUnavailable);
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
    ROCKY_TODO("nyi");
    return target;
    //return rocky::getFullPath( _referrer, target );
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

#if 0
void
URIContext::store(osgDB::Options* options)
{
    if (options)
    {
        if (_referrer.empty() == false)
        {
            options->setDatabasePath(_referrer);
            options->setPluginStringData("rocky::URIContext::referrer", _referrer);
        }
    }
}

URIContext::URIContext( const osgDB::Options* options )
{
    if ( options )
    {
        _referrer = options->getPluginStringData( "rocky::URIContext::referrer" );

        if ( _referrer.empty() && options->getDatabasePathList().size() > 0 )
        {
            const std::string& front = options->getDatabasePathList().front();
            if ( rocky::isArchive(front) )
            {
                _referrer = front + "/";
            }
        }
    }
}
#endif

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
    _context(rhs._context),
    _cacheKey(rhs._cacheKey)
{
    //nop
}

URI::URI(const std::string& location)
{
    _baseURI = location;
    _fullURI = location;
    ctorCacheKey();
}

URI::URI(const std::string& location, const URIContext& context)
{
    _context = context;
    _baseURI = location;
    _fullURI = context.getCanonicalPath(_baseURI);
    ctorCacheKey();
}

URI::URI(const char* location)
{
    _baseURI = std::string(location);
    _fullURI = _baseURI;
    ctorCacheKey();
}

URI
URI::append(const std::string& suffix) const
{
    URI result;
    result._baseURI = _baseURI + suffix;
    result._fullURI = _fullURI + suffix;
    result._context = _context;
    result.ctorCacheKey();
    return result;
}

void
URI::ctorCacheKey()
{
    _cacheKey = util::makeCacheKey(_fullURI, "uri");
}

IOResult<URI::Content>
URI::read(IOControl* io) const
{
    if (containsServerAddress(full()))
    {
        HTTPRequest request{ full() };
        auto r = http_get(request);
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

#if 0
void
URI::ctorCacheKey()
{
    _cacheKey = Cache::makeCacheKey(_fullURI, "uri");
}
#endif

bool
URI::isRemote() const
{
    return containsServerAddress(_fullURI);
}

Config
URI::getConfig() const
{
    Config conf("uri", base());
    conf.set("option_string", _optionString);
    conf.setReferrer(context().referrer());
    conf.setIsLocation(true);

    const Headers& headers = context().getHeaders();
    if (!headers.empty())
    {
        Config headersconf("headers");
        for(Headers::const_iterator i = headers.begin(); i != headers.end(); ++i)
        {
            if (!i->first.empty() && !i->second.empty())
            {
                headersconf.add(Config(i->first, i->second));
            }
        }
        conf.add(headersconf);
    }

    return conf;
}

void
URI::mergeConfig(const Config& conf)
{
    conf.get("option_string", _optionString);

    const ConfigSet headers = conf.child("headers").children();
    for (ConfigSet::const_iterator i = headers.begin(); i != headers.end(); ++i)
    {
        const Config& header = *i;
        if (!header.key().empty() && !header.value().empty())
        {
            _context.addHeader(header.key(), header.value());
        }
    }
}

// Ref.: https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
std::string
URI::urlEncode(const std::string& value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char)c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

#if 0
namespace
{
    struct ReadObject
    {
        bool callbackRequestsCaching( URIReadCallback* cb ) const { return !cb || ((cb->cachingSupport() & URIReadCallback::CACHE_OBJECTS) != 0); }
        ReadResult fromCallback( URIReadCallback* cb, const std::string& uri, const osgDB::Options* opt ) { return cb->readObject(uri, opt); }
        ReadResult fromCache( CacheBin* bin, const std::string& key) { return bin->readObject(key, 0L); }
        ReadResult fromHTTP( const URI& uri, const osgDB::Options* opt, ProgressCallback* p, TimeStamp lastModified )
        {
            HTTPRequest req(uri.full());
            req.getHeaders() = uri.context().getHeaders();
            if (lastModified > 0)
            {
                req.setLastModified(lastModified);
            }
            return HTTPClient::readObject(req, opt, p);
        }
        ReadResult fromFile( const std::string& uri, const osgDB::Options* opt ) {
            osgDB::ReaderWriter::ReadResult osgRR = osgDB::Registry::instance()->readObject(uri, opt);
            if (osgRR.validObject()) return ReadResult(osgRR.takeObject());
            else return ReadResult(osgRR.message());
        }
    };

    struct ReadNode
    {
        bool callbackRequestsCaching( URIReadCallback* cb ) const { return !cb || ((cb->cachingSupport() & URIReadCallback::CACHE_NODES) != 0); }
        ReadResult fromCallback( URIReadCallback* cb, const std::string& uri, const osgDB::Options* opt ) { return cb->readNode(uri, opt); }
        ReadResult fromCache( CacheBin* bin, const std::string& key ) { return bin->readObject(key, 0L); }
        ReadResult fromHTTP(const URI& uri, const osgDB::Options* opt, ProgressCallback* p, TimeStamp lastModified )
        {
            HTTPRequest req(uri.full());
            req.getHeaders() = uri.context().getHeaders();
            if (lastModified > 0)
            {
                req.setLastModified(lastModified);
            }
            return HTTPClient::readNode(req, opt, p);
        }
        ReadResult fromFile( const std::string& uri, const osgDB::Options* opt ) {
            osgDB::ReaderWriter::ReadResult osgRR = osgDB::Registry::instance()->readNode(uri, opt);
            if (osgRR.validNode()) return ReadResult(osgRR.takeNode());
            else return ReadResult(osgRR.message());
        }
    };

    struct ReadImage
    {
        bool callbackRequestsCaching( URIReadCallback* cb ) const {
            return !cb || ((cb->cachingSupport() & URIReadCallback::CACHE_IMAGES) != 0);
        }
        ReadResult fromCallback( URIReadCallback* cb, const std::string& uri, const osgDB::Options* opt ) {
            ReadResult r = cb->readImage(uri, opt);
            if ( r.getImage() ) r.getImage()->setFileName(uri);
            return r;
        }
        ReadResult fromCache( CacheBin* bin, const std::string& key) {
            ReadResult r = bin->readImage(key, 0L);
            if ( r.getImage() ) r.getImage()->setFileName( key );
            return r;
        }
        ReadResult fromHTTP(const URI& uri, const osgDB::Options* opt, ProgressCallback* p, TimeStamp lastModified ) {
            HTTPRequest req(uri.full());
            req.getHeaders() = uri.context().getHeaders();

            if (lastModified > 0)
            {
                req.setLastModified(lastModified);
            }
            ReadResult r = HTTPClient::readImage(req, opt, p);
            if ( r.getImage() ) r.getImage()->setFileName( uri.full() );
            return r;
        }
        ReadResult fromFile( const std::string& uri, const osgDB::Options* opt ) {
            // Call readImageImplementation instead of readImage to bypass any readfile callbacks installed in the registry.
            osgDB::ReaderWriter::ReadResult osgRR = osgDB::Registry::instance()->readImageImplementation(uri, opt);
            if (osgRR.validImage()) {
                osgRR.getImage()->setFileName(uri);
                return ReadResult(osgRR.takeImage());
            }
            else return ReadResult(osgRR.message());
        }
    };

    struct ReadString
    {
        bool callbackRequestsCaching( URIReadCallback* cb ) const {
            return !cb || ((cb->cachingSupport() & URIReadCallback::CACHE_STRINGS) != 0);
        }
        ReadResult fromCallback( URIReadCallback* cb, const std::string& uri, const osgDB::Options* opt ) {
            return cb->readString(uri, opt);
        }
        ReadResult fromCache( CacheBin* bin, const std::string& key) {
            return bin->readString(key, 0L);
        }
        ReadResult fromHTTP(const URI& uri, const osgDB::Options* opt, ProgressCallback* p, TimeStamp lastModified )
        {
            HTTPRequest req(uri.full());
            req.getHeaders() = uri.context().getHeaders();
            if (lastModified > 0)
            {
                req.setLastModified(lastModified);
            }
            return HTTPClient::readString(req, opt, p);
        }
        ReadResult fromFile( const std::string& uri, const osgDB::Options* opt ) {
            return readStringFile(uri, opt);
        }
    };

    //--------------------------------------------------------------------
    // MASTER read template function. I templatized this so we wouldn't
    // have 4 95%-identical code paths to maintain...

    template<typename READ_FUNCTOR>
    ReadResult doRead(
        const URI&            inputURI,
        const osgDB::Options* dbOptions,
        ProgressCallback*     progress)
    {
        //osg::Timer_t startTime = osg::Timer::instance()->tick();

        unsigned long handle = NetworkMonitor::begin(inputURI.full(), "pending", "URI");
        ReadResult result;

        if (rocky::Registry::instance()->isBlacklisted(inputURI.full()))
        {
            NetworkMonitor::end(handle, "Blacklisted");
            return result;
        }

        if ( !inputURI.empty() )
        {
            // establish our IO options:
            osg::ref_ptr<osgDB::Options> localOptions = dbOptions ? Registry::cloneOrCreateOptions(dbOptions) : Registry::cloneOrCreateOptions(Registry::instance()->getDefaultOptions());

            // if we have an option string, incorporate it.
            if ( inputURI.optionString().has_value() )
            {
                localOptions->setOptionString(
                    inputURI.optionString().get() + " " + localOptions->getOptionString());
            }

            // Store a new URI context within the local options so that subloaders know the location they are loading from
            // This is necessary for things like gltf files that can store external binary files that are relative to the gltf file.
            URIContext(inputURI.full()).store(localOptions.get());

            READ_FUNCTOR reader;

            URI uri = inputURI;

            bool gotResultFromCallback = false;

            // check if there's an alias map, and if so, attempt to resolve the alias:
            URIAliasMap* aliasMap = URIAliasMap::from( localOptions.get() );
            if ( aliasMap )
            {
                uri = aliasMap->resolve(inputURI.full(), inputURI.context());
            }

            // check if there's a URI cache in the options.
            URIResultCache* memCache = URIResultCache::from( localOptions.get() );
            if ( memCache )
            {
                URIResultCache::Record rec;
                if ( memCache->get(uri, rec) )
                {
                    result = rec.value();
                }
            }

            if ( result.empty() )
            {
                // see if there's a read callback installed.
                URIReadCallback* cb = Registry::instance()->getURIReadCallback();

                // for a local URI, bypass all the caching logic
                if ( !uri.isRemote() )
                {
                    // try to use the callback if it's set. Callback ignores the caching policy.
                    if ( cb )
                    {
                        // if this returns "not implemented" we fill fall back
                        result = reader.fromCallback( cb, uri.full(), localOptions.get() );

                        if ( result.code() != ReadResult::RESULT_NOT_IMPLEMENTED )
                        {
                            // "not implemented" is the only excuse to fall back.
                            gotResultFromCallback = true;
                        }
                    }

                    if ( !gotResultFromCallback )
                    {
                        // no callback, just read from a local file.
                        result = reader.fromFile( uri.full(), localOptions.get() );
                    }
                }

                // remote URI, consider caching:
                else
                {
                    bool callbackCachingOK = !cb || reader.callbackRequestsCaching(cb);

                    optional<CachePolicy> cp;
                    osg::ref_ptr<CacheBin> bin;

                    CacheSettings* cacheSettings = CacheSettings::get(localOptions.get());
                    if (cacheSettings)
                    {
                        cp = cacheSettings->cachePolicy();
                        if (cp->isCacheEnabled() && callbackCachingOK)
                        {
                            bin = cacheSettings->getCacheBin();
                        }
                    }

                    bool expired = false;
                    // first try to go to the cache if there is one:
                    if ( bin && cp->isCacheReadable() )
                    {
                        result = reader.fromCache( bin.get(), uri.cacheKey() );
                        if ( result.succeeded() )
                        {
                            expired = cp->isExpired(result.lastModifiedTime());
                            result.setIsFromCache(true);
                        }
                    }

                    // If it's not cached, or it is cached but is expired then try to hit the server.
                    if ( result.empty() || expired )
                    {
                        // Need to do this to support nested PLODs and Proxynodes.
                        osg::ref_ptr<osgDB::Options> remoteOptions =
                            Registry::instance()->cloneOrCreateOptions( localOptions.get() );
                        remoteOptions->getDatabasePathList().push_front( osgDB::getFilePath(uri.full()) );

                        // Store the existing object from the cache if there is one.
                        osg::ref_ptr< osg::Object > object = result.getObject();

                        // try to use the callback if it's set. Callback ignores the caching policy.
                        if ( cb )
                        {
                            result = reader.fromCallback( cb, uri.full(), remoteOptions.get() );

                            if ( result.code() != ReadResult::RESULT_NOT_IMPLEMENTED )
                            {
                                // "not implemented" is the only excuse for falling back
                                gotResultFromCallback = true;
                            }
                        }

                        if ( !gotResultFromCallback )
                        {
                            // still no data, go to the source:
                            if ( (result.empty() || expired) && cp->usage() != CachePolicy::USAGE_CACHE_ONLY )
                            {
                                ReadResult remoteResult = reader.fromHTTP( uri, remoteOptions.get(), progress, result.lastModifiedTime() );
                                if (remoteResult.code() == ReadResult::RESULT_NOT_MODIFIED)
                                {
                                    ROCKY_DEBUG << LC << uri.full() << " not modified, using cached result" << std::endl;
                                    // Touch the cached item to update it's last modified timestamp so it doesn't expire again immediately.
                                    if (bin)
                                        bin->touch( uri.cacheKey() );
                                }
                                else
                                {
                                    ROCKY_DEBUG << LC << "Got remote result for " << uri.full() << std::endl;
                                    result = remoteResult;
                                }
                            }

                            // Check for cancellation before a cache write
                            if (progress && progress->isCanceled())
                            {
                                NetworkMonitor::end(handle, "Canceled");
                                return 0L;
                            }

                            // write the result to the cache if possible:
                            if ( result.succeeded() && !result.isFromCache() && bin && cp->isCacheWriteable() )
                            {
                                ROCKY_DEBUG << LC << "Writing " << uri.cacheKey() << " to cache" << std::endl;
                                bin->write( uri.cacheKey(), result.getObject(), result.metadata(), remoteOptions.get() );
                            }
                        }
                    }
                }

                // Check for cancelation before a potential cache write
                if (progress && progress->isCanceled())
                {
                    NetworkMonitor::end(handle, "Canceled");
                    return 0L;
                }

                if (result.getObject() && !gotResultFromCallback)
                {
                    result.getObject()->setName( uri.base() );

                    if ( memCache )
                    {
                        memCache->insert( uri, result );
                    }
                }

                // If the request failed with an unrecoverable error,
                // blacklist so we don't waste time on it again
                if (result.failed())
                {
                    rocky::Registry::instance()->blacklist(inputURI.full());
                }
            }

            ROCKY_TEST << LC
                << uri.base() << ": "
                << (result.succeeded() ? "OK" : "FAILED")
                //<< "; policy=" << cp->usageString()
                << (result.isFromCache() && result.succeeded() ? "; (from cache)" : "")
                << std::endl;
        }

        // post-process if there's a post-URI callback.
        URIPostReadCallback* post = URIPostReadCallback::from(dbOptions);
        if ( post )
        {
            (*post)(result);
        }

        std::stringstream buf;
        buf << result.getResultCodeString();
        if (result.isFromCache() && result.succeeded())
        {
            buf << " (from cache)";
        }
        NetworkMonitor::end(handle, buf.str());

        return result;
    }
}
#endif
