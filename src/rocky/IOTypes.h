/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Result.h>
#include <rocky/Units.h>
#include <rocky/Threading.h>
#include <rocky/Cache.h>
#include <optional>
#include <string>
#include <cstdint>

/**
 * A collection of types used by the various I/O systems.
 */
namespace ROCKY_NAMESPACE
{
    class IOOptions;
    class Image;
    class Layer;
    class ContextImpl;
    class GeoExtent;

    //! Service for reading an image from a URI
    using ReadImageURIService = std::function<
        Result<std::shared_ptr<Image>>(const std::string& location, const IOOptions&)>;

    //! Service for reading an image from a stream
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

    //! A cache that stores Content objects by URI.
    using ContentCache = rocky::util::LRUCache<std::string, Result<Content>>;

    /**
    * Collection of service available to rocky classes that perform IO operations.
    */
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
        std::shared_ptr<util::ResidentCache<std::string, Image, GeoExtent>> residentImageCache;

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


    // inlines
    inline bool IOOptions::canceled() const {
        return _cancelable ? _cancelable->canceled() : false;
    }
}
