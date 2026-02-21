/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Result.h>
#include <rocky/Threading.h>
#include <rocky/Cache.h>
#include <optional>
#include <string>
#include <cstdint>
#include <chrono>

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
    using DealpoolService = rocky::detail::LRUCache<std::string, Failure>;

    //! Holds a generic content buffer and its type.
    struct Content {
        std::string type;   // i.e., content-type or mime-type
        std::string data;   // actual data buffer
        std::chrono::system_clock::time_point timestamp;
    };

    //! A cache that stores Content objects by URI.
    using ContentCache = rocky::detail::LRUCache<std::string, Result<Content>>;

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
        detail::Gate<std::string> uriGate;

        //! Caches raw context coming from a URI (like a browser cache)
        std::shared_ptr<ContentCache> contentCache;

        //! Provides fast access to Image data that is resident somwehere in memory
        std::shared_ptr<detail::ResidentCache<std::string, Image, GeoExtent>> residentImageCache;

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
        inline IOOptions(const IOOptions& rhs) = default;
        inline IOOptions(IOOptions&& rhs) { *this = rhs; }
        inline IOOptions& operator=(IOOptions&&) noexcept;

        //! Clone this options structure and set a new referrer.
        inline IOOptions from(const std::string& referrer);

        //! Clone this options structure and include the cancelable token.
        //! Use this to pass options to operations that support cancellation.
        inline IOOptions with(Cancelable& c) const;

        //! Was the current operation canceled?
        inline bool canceled() const override;

        //! Maximum number of attempts to make a network connection
        unsigned maxNetworkAttempts = 4u;

        //! Network connection timeout (in seconds; 0 = infinite)
        std::chrono::seconds networkConnectionTimeout = std::chrono::seconds(5);

        //! Referring location for an operation using these options
        std::optional<std::string> referrer;

        //! Access to shared services
        inline Services& services() const;

    public:
        IOOptions& operator = (const IOOptions& rhs) = default;

    private:
        Cancelable* _cancelable = nullptr;
        mutable std::shared_ptr<Services> _services;
    };


    // inlines
    IOOptions& IOOptions::operator = (IOOptions&& rhs) noexcept
    {
        if (this != &rhs)
        {
            Cancelable::operator=(rhs);
            maxNetworkAttempts = rhs.maxNetworkAttempts;
            referrer = std::move(rhs.referrer);
            _services = rhs._services;
            _cancelable = rhs._cancelable;
            rhs._cancelable = nullptr;
        }
        return *this;
    }

    inline IOOptions IOOptions::from(const std::string& referrer) {
        IOOptions copy(*this);
        copy.referrer = referrer;
        return copy;
    }

    inline IOOptions IOOptions::with(Cancelable& c) const {
        IOOptions copy(*this);
        copy._cancelable = &c;
        return copy;
    }

    inline bool IOOptions::canceled() const {
        return _cancelable ? _cancelable->canceled() : false;
    }

    inline Services& IOOptions::services() const {
        return *_services;
    }
}
