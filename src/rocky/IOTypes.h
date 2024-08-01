/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/DateTime.h>
#include <rocky/Log.h>
#include <rocky/Status.h>
#include <rocky/Units.h>
#include <rocky/Threading.h>
#include <rocky/LRUCache.h>
#include <optional>

/**
 * A collection of types used by the various I/O systems.
 */
namespace ROCKY_NAMESPACE
{
    class IOOptions;
    class Image;
    class Layer;

    //! Base class for a cache
    class Cache : public Inherit<Object, Cache>
    {
    };

    //! Service for reading an image from a URI
    using ReadImageURIService = std::function<
        Result<shared_ptr<Image>>(const std::string& location, const IOOptions&)>;

    //! Service fro reading an image from a stream
    using ReadImageStreamService = std::function<
        Result<shared_ptr<Image>>(std::istream& stream, std::string contentType, const IOOptions& io)>;

    //! Service for writing an image to a stream
    using WriteImageStreamService = std::function<
        Status(shared_ptr<Image> image, std::ostream& stream, std::string contentType, const IOOptions& io)>;

    //! Service for caching data
    using CacheImpl = void*; // todo.
    using CacheService = std::function<shared_ptr<CacheImpl>()>;

    //! Service for accessing other data
    class DataInterface {
    public:
        virtual shared_ptr<Layer> findLayerByName(const std::string& name) const = 0;
    };
    using DataService = std::function<DataInterface&()>;

    struct Content {
        std::string contentType;
        std::string data;
        std::chrono::system_clock::time_point timestamp;
    };

    using ContentCache = rocky::util::LRUCache<std::string, Result<Content>>;

    class ROCKY_EXPORT Services
    {
    public:
        Services();
        ReadImageURIService readImageFromURI;
        ReadImageStreamService readImageFromStream;
        WriteImageStreamService writeImageToStream;
        CacheService cache = nullptr;
        shared_ptr<ContentCache> contentCache;
    };

    // User options passed along with an IO context.
    class ROCKY_EXPORT IOOptions : public Cancelable
    {
    public:
        IOOptions() = default;
        IOOptions(const IOOptions& rhs);
        IOOptions(Cancelable& p);
        IOOptions(const IOOptions& rhs, Cancelable& p);
        IOOptions(const std::string& referrer);
        IOOptions(const IOOptions& rhs, const std::string& referrer);

        //! Was the current operation canceled?
        inline bool canceled() const override;

        //! Access to pluggable services
        Services services;

        //! Maximum number of attempts to make a network connection
        unsigned maxNetworkAttempts = 4u;

        //! Referring location for an operation using these options
        std::optional<std::string> referrer;

        //! Gate for seriaizing duplicate URI requests (shared)
        mutable std::shared_ptr<util::Gate<std::string>> uriGate;

        //! Custom options for reading/writing data
        //inline std::string property(const std::string& name) const;
        //inline std::string& property(const std::string& name);

    public:
        IOOptions& operator = (const IOOptions& rhs);

    private:
        Cancelable* _cancelable = nullptr;
        std::unordered_map<std::string, std::string> _properties;
    };


    class ROCKY_EXPORT CachePolicy
    {
    public:
        enum class Usage
        {
            READ_WRITE = 0,  // read/write to the cache if one exists.
            CACHE_ONLY = 1,  // treat the cache as the ONLY source of data.
            READ_ONLY = 2,  // read from the cache, but don't write new data to it.
            NO_CACHE = 3   // neither read from or write to the cache
        };

        optional<Usage> usage = Usage::READ_WRITE;
        optional<Duration> maxAge = Duration(DBL_MAX, Units::SECONDS);
        optional<DateTime> minTime = DateTime(0);

        //! default cache policy (READ_WRITE)
        static CachePolicy DEFAULT;

        //! policy indicating to never use a cache
        static CachePolicy NO_CACHE;

        //! policy indicating to only use a cache
        static CachePolicy CACHE_ONLY;

    public:
        //! constructs an invalid CachePolicy
        CachePolicy();

        //! constructs a CachePolicy from a usage
        CachePolicy(const Usage&);

        //! Merges any set properties in another CP into this one, override existing values.
        void mergeAndOverride(const CachePolicy& rhs);
        void mergeAndOverride(const optional<CachePolicy>& rhs);

        //! Gets the oldest timestamp for which to accept a cache record
        DateTime getMinAcceptTime() const;

        //! Whether the given timestamp is considered to be expired based on this CachePolicy
        bool isExpired(TimeStamp lastModified) const;

    public: // convenience functions.

        bool isCacheEnabled() const {
            return isCacheReadable() || isCacheWriteable();
        }

        bool isCacheDisabled() const {
            return !isCacheEnabled();
        }

        bool isCacheReadable() const {
            return
                usage.value() == Usage::READ_WRITE ||
                usage.value() == Usage::CACHE_ONLY ||
                usage.value() == Usage::READ_ONLY;
        }

        bool isCacheWriteable() const {
            return usage.value() == Usage::READ_WRITE;
        }

        bool isCacheOnly() const {
            return usage.value() == Usage::CACHE_ONLY;
        }

        bool operator == (const CachePolicy& rhs) const;

        bool operator != (const CachePolicy& rhs) const {
            return !operator==(rhs);
        }

        CachePolicy& operator = (const CachePolicy& rhs);

        // returns a readable string describing usage
        std::string usageString() const;
    };

    /**
    * Proxy server configuration.
    */
    class ROCKY_EXPORT ProxySettings
    {
    public:
        std::string hostname;
        int port = -1;
        std::string username;
        std::string password;
    };

    /**
     * Convenience metadata tags
     */
    struct ROCKY_EXPORT IOMetadata
    {
        static const std::string CONTENT_TYPE;
    };

    /**
     * Return value from a read* method
     */
    template<typename T>
    struct IOResult : public Result<T>
    {
        /** Read result codes. */
        enum IOCode
        {
            RESULT_OK,
            RESULT_CANCELED,
            RESULT_NOT_FOUND,
            RESULT_EXPIRED,
            RESULT_SERVER_ERROR,
            RESULT_TIMEOUT,
            RESULT_NO_READER,
            RESULT_READER_ERROR,
            RESULT_UNKNOWN_ERROR,
            RESULT_NOT_IMPLEMENTED,
            RESULT_NOT_MODIFIED
        };

        unsigned ioCode = RESULT_OK;
        TimeStamp lastModifiedTime = 0;
        Duration duration;
        bool fromCache = false;
        JSON metadata;

        IOResult(const T& result) :
            Result<T>(result) { }

        /** Construct a result with an error message */
        IOResult(const Status& s) :
            Result<T>(s),
            ioCode(s.ok() ? RESULT_OK : RESULT_NOT_FOUND) { }

        template<typename RHS>
        static IOResult<T> propagate(const IOResult<RHS>& rhs) {
            IOResult<T> lhs(rhs.status);
            lhs.ioCode = rhs.ioCode;
            lhs.metadata = rhs.metadata;
            lhs.fromCache = rhs.fromCache;
            lhs.lastModifiedTime = rhs.lastModifiedTime;
            lhs.duration = rhs.duration;
            return lhs;
        }

        /** Gets a string describing the read result */
        static std::string getResultCodeString(unsigned code)
        {
            return
                code == RESULT_OK              ? "OK" :
                code == RESULT_CANCELED        ? "Read canceled" :
                code == RESULT_NOT_FOUND       ? "Target not found" :
                code == RESULT_SERVER_ERROR    ? "Server reported error" :
                code == RESULT_TIMEOUT         ? "Read timed out" :
                code == RESULT_NO_READER       ? "No suitable ReaderWriter found" :
                code == RESULT_READER_ERROR    ? "ReaderWriter error" :
                code == RESULT_NOT_IMPLEMENTED ? "Not implemented" :
                code == RESULT_NOT_MODIFIED    ? "Not modified" :
                                                 "Unknown error";
        }

        std::string getResultCodeString() const
        {
            return getResultCodeString(ioCode);
        }
    };

#if 0
    std::string IOOptions::property(const std::string& name) const {
        auto i = _properties.find(name);
        return i != _properties.end() ? i->second : "";
    }

    std::string& IOOptions::property(const std::string& name) {
        return _properties[name];
    }
#endif

    bool IOOptions::canceled() const {
        return _cancelable ? _cancelable->canceled() : false;
    }
}
