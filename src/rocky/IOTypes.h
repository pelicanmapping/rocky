/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Result.h>
#include <rocky/Units.h>
#include <rocky/Threading.h>
#include <rocky/LRUCache.h>
#include <optional>
#include <string>
#include <cstdint>
#include <unordered_set>

/**
 * A collection of types used by the various I/O systems.
 */
namespace ROCKY_NAMESPACE
{
    class IOOptions;
    class Image;
    class Layer;
    class ContextImpl;

    //! Service for reading an image from a URI
    using ReadImageURIService = std::function<
        Result<std::shared_ptr<Image>>(const std::string& location, const IOOptions&)>;

    //! Service fro reading an image from a stream
    using ReadImageStreamService = std::function<
        Result<std::shared_ptr<Image>>(std::istream& stream, std::string contentType, const IOOptions& io)>;

    //! Service for writing an image to a stream
    using WriteImageStreamService = std::function<
        Result<>(std::shared_ptr<Image> image, std::ostream& stream, std::string contentType, const IOOptions& io)>;

    //! Service for tracking invalid request URIs
    using DealpoolService = util::LRUCache<std::string, Failure>;

    //! Holds a generic content buffer and its type.
    struct Content {
        std::string type;   // i.e., content-type or mime-type
        std::string data;   // actual data buffer
        std::chrono::system_clock::time_point timestamp;
    };

    using ContentCache = rocky::util::LRUCache<std::string, Result<Content>>;

    class ROCKY_EXPORT Services
    {
    public:
        Services();
        Services(const Services&) = default;
        Services& operator=(const Services&) = default;
        Services(Services&&) noexcept = delete;
        Services& operator=(Services&&) noexcept = delete;

        //! Decodes an Image::Ptr from a URI
        ReadImageURIService readImageFromURI;

        //! Decodes an Image::Ptr from a std::istream
        ReadImageStreamService readImageFromStream;

        //! Encodes an Image::Ptr to a std::ostream
        WriteImageStreamService writeImageToStream;

        //! Serializes reads from identical URIs
        util::Gate<std::string> uriGate;

        //! Caches raw context coming from a URI (like a browser cache)
        std::shared_ptr<ContentCache> contentCache;

        //! Provides fast access to Image data that is resident somwehere in memory
        std::shared_ptr<util::ResidentCache<std::string, Image>> residentImageCache;

        //! URI deadpool; URI will use this if available.
        std::shared_ptr<DealpoolService> deadpool;
    };

    /**
     * Options and services passed along to IO operations.
     */
    class ROCKY_EXPORT IOOptions : public Cancelable
    {
    public:
        IOOptions();
        IOOptions(const IOOptions& rhs) = default;
        IOOptions(const IOOptions& rhs, Cancelable& p);
        IOOptions(const IOOptions& rhs, const std::string& referrer);

        IOOptions(IOOptions&&) = delete;
        IOOptions& operator=(IOOptions&&) noexcept = delete;

        //! Copy this options structure and set a new referrer.
        IOOptions from(const std::string& referrer) {
            return IOOptions(*this, referrer);
        }

        //! Was the current operation canceled?
        inline bool canceled() const override;

        //! Maximum number of attempts to make a network connection
        unsigned maxNetworkAttempts = 4u;

        //! Referring location for an operation using these options
        std::optional<std::string> referrer;

        //! Access to shared services
        Services& services() const {
            return *_services;
        }

    public:
        IOOptions& operator = (const IOOptions& rhs) = default;

    private:
        Cancelable* _cancelable = nullptr;
        mutable std::shared_ptr<Services> _services;
    };

    /**
     * Return value from a read* method
     */
    struct IOFailure
    {
        enum Type {
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

        Type type;
        std::string message;

        IOFailure(Type t) : type(t) {}
        IOFailure(Type t, std::string_view m) : type(t), message(m) {}

        std::string string() const {
            return
                type == RESULT_CANCELED ? "Read canceled" :
                type == RESULT_NOT_FOUND ? "Target not found" :
                type == RESULT_EXPIRED ? "Target expired" :
                type == RESULT_SERVER_ERROR ? "Server reported error" :
                type == RESULT_TIMEOUT ? "Read timed out" :
                type == RESULT_NO_READER ? "No suitable ReaderWriter found" :
                type == RESULT_READER_ERROR ? "ReaderWriter error" :
                type == RESULT_NOT_IMPLEMENTED ? "Not implemented" :
                type == RESULT_NOT_MODIFIED ? "Not modified" :
                "Unknown error";
        }
    };
    
    struct IOResponse
    {
        Content content;
        std::int64_t lastModifiedTime = 0;
        Duration duration;
        bool fromCache = false;
        std::string jsonMetadata;

        IOResponse(const Content& in_content) :
            content(in_content) { }

        IOResponse(Content&& in_content) :
            content(std::move(in_content)) { }
    };

    bool IOOptions::canceled() const {
        return _cancelable ? _cancelable->canceled() : false;
    }
}
