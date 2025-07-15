/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/IOTypes.h>

#include <string>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * Represents the location of a resource, providing the raw (original, possibly
     * relative) and absolute forms.
     */
    class ROCKY_EXPORT URI
    {
    public:

        using Headers = std::vector<std::pair<std::string, std::string>>;

        /**
         * Context for resolving relative URIs.
         *
         * This object provides "context" for a relative URI. In other words, it provides
         * all of the information the system needs to resolve it to an absolute location.
         *
         * The "referrer" is the location of an object that "points" to the object in the
         * corresponding URI. The location conveyed by the URI will be relative to the location of
         * its referrer. For example, a referrer of "http://server/folder/hello.xml" applied
         * to the URI "there.jpg" will resolve to "http://server/folder/there.jpg". NOTE that referrer
         * it not itself a location (like a folder); rather it's the object that referred to the URI
         * being contextualized.
         */
        struct Context
        {
            Context() = default;
            Context(std::string_view v) : referrer(v) {}
            std::string referrer;
            Headers headers;
        };

    public:
        //! Whether HTTPS support is available
        static bool supportsHTTPS();

        //! Holds a stream for reading content data.
        struct ROCKY_EXPORT Stream
        {
        public:
            Stream(std::shared_ptr<std::istream> s = nullptr);

            //! Whether the stream exists
            bool valid() const { return _in != nullptr; }

            //! Access the underlying stream (if valid == true)
            operator std::istream&() { return *_in.get(); }

            //! Read the stream into a string and return it
            std::string to_string();

        private:
            std::shared_ptr<std::istream> _in;
        };

    public:
        //! Constructs an empty (and invalid) URI.
        URI() = default;

        //! Copy construct
        URI(const URI& rhs) = default;

        //! Constructs a new URI from a location and a referring location.
        URI(const std::string& location, const URI::Context& context = {});

        //! Constructs a new URI from a location and a referring location.
        URI(const std::string& location, const std::string& referrer) :
            URI(location, URI::Context{ referrer }) { }

        //! Construct a new URI from a character string
        URI(const char* location) :
            URI(std::string(location)) { }

        /** The base (possibly relative) location string. */
        const std::string& base() const { return _baseURI; }

        /** The fully qualified location string. */
        const std::string& full() const { return _fullURI; }

        /** The dereference operator also returns the fully qualified URI, since it's a common operation. */
        const std::string& operator * () const { return _fullURI; }

        //! Sets a referrer string for relative path URIs
        void setReferrer(const std::string& value);

        /** Context with which this URI was created. */
        const URI::Context& context() const { return _context; }

        /** Whether the URI is empty */
        bool empty() const { return _baseURI.empty(); }

        /** Whether the object of the URI is cacheable. */
        bool isRemote() const;

        //! Reads the URI into a data buffer
        IOResult<Content> read(const IOOptions& io) const;

    public:

        bool operator < (const URI& rhs) const { 
            return _fullURI < rhs._fullURI;
        }

        bool operator == (const URI& rhs) const {
            return _fullURI.compare(rhs._fullURI) == 0;
        }

        bool operator != (const URI& rhs) const {
            return _fullURI.compare(rhs._fullURI) != 0;
        }


    public:

        //! Encodes text to URL safe test. Escapes special charaters
        static std::string urlEncode(const std::string& value);

        //! Try to infer a content-type from a string of bytes.
        static std::string inferContentType(const std::string& value);


    protected:
        std::string _baseURI;
        std::string _fullURI;
        std::string::size_type _r0 = std::string::npos, _r1 = std::string::npos;
        URI::Context _context;

        void set(std::string_view location, const URI::Context& context);
        void findRotation();
    };

    /**
    * A hyperlink is a text string with an associated URI.
    */
    struct Hyperlink
    {
        std::string text;
        URI href;
    };
}
