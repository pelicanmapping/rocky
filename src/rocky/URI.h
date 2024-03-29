/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/IOTypes.h>

#include <iostream>
#include <string>

namespace ROCKY_NAMESPACE
{
    class URI;
    class IOControl;

    /**
     * Context for resolving relative URIs.
     *
     * This object provides "context" for a relative URI. In other words, it provides
     * all of the information the system needs to resolve it to an absolute location from
     * which OSG can load data.
     *
     * The "referrer" is the location of an object that "points" to the object in the
     * corresponding URI. The location conveyed by the URI will be relative to the location of
     * its referrer. For example, a referrer of "http://server/folder/hello.xml" applied
     * to the URI "there.jpg" will resolve to "http://server/folder/there.jpg". NOTE that referrer
     * it not itself a location (like a folder); rather it's the object that referred to the URI
     * being contextualized.
     */
    struct URIContext
    {
        std::string referrer;
        Headers headers;
    };


    /**
     * Represents the location of a resource, providing the raw (original, possibly
     * relative) and absolute forms.
     *
     * URI is serializable and may be used in an earth file, like in the following
     * example. Note that in earth files, the URI is actually called "url"; this is
     * simply because of an old convention and we wish to avoid breaking backwards
     * compatibility.
     *
     *   <url>../path/relative/to/earth/file</url>
     *
     * Note also that a relative URI will be relative to the location of the
     * parent resource (usually the earth file itself).
     *
     * You can also specify osgDB plugin options; for example:
     *
     *   <url options_string="JPEG_QUALITY 60">../path/to/image.jpg</url>
     *
     * Of course, options are particular to OSG plugins, so please consult the
     * code for your plugin for more information.
     */
    class ROCKY_EXPORT URI
    {
    public:
        //! Whether HTTPS support is available
        static bool supportsHTTPS();

        //! Holds a stream for reading content data.
        struct ROCKY_EXPORT Stream
        {
        public:
            Stream(shared_ptr<std::istream> s = nullptr);

            //! Whether the stream exists
            bool valid() const { return _in != nullptr; }

            //! Access the underlying stream (if valid == true)
            operator std::istream&() { return *_in.get(); }

            //! Read the stream into a string and return it
            std::string to_string();

        private:
            shared_ptr<std::istream> _in;
        };

    public:
        //! Constructs an empty (and invalid) URI.
        URI();

        URI(const URI& rhs);

        //! Constructs a new URI from a location (typically an absolute url)
        URI(const std::string& location);

        //! Constructs a new URI from a location and an existing context.
        URI(const std::string& location, const URIContext& context);

        //! Constructs a new URI from a location and a referring location.
        URI(const std::string& location, const std::string& referrer) :
            URI(location, URIContext{ referrer }) { }

        URI(const char* location) :
            URI(std::string(location)) { }

        /** The base (possibly relative) location string. */
        const std::string& base() const { return _baseURI; }

        /** The fully qualified location string. */
        const std::string& full() const { return _fullURI; }

        /** The dereference operator also returns the fully qualified URI, since it's a common operation. */
        const std::string& operator * () const { return _fullURI; }

        /** Context with which this URI was created. */
        const URIContext& context() const { return _context; }

        /** Whether the URI is empty */
        bool empty() const { return _baseURI.empty(); }

        /** Whether the object of the URI is cacheable. */
        bool isRemote() const;

        //! Reads the URI into a data buffer
        IOResult<Content> read(const IOOptions& io) const;

    public:

        bool operator < (const URI& rhs) const { return _fullURI < rhs._fullURI; }

        bool operator == (const URI& rhs) const { return _fullURI.compare(rhs._fullURI) == 0; }

        bool operator != (const URI& rhs) const { return _fullURI.compare(rhs._fullURI) != 0; }


    public: // Static convenience methods

        /** Encodes text to URL safe test. Escapes special charaters */
        inline static std::string urlEncode(const std::string &value);

    protected:
        std::string _baseURI;
        std::string _fullURI;
        URIContext _context;
    };
}
