/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Config.h>
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
    class ROCKY_EXPORT URIContext
    {
    public:
        /** Creates an empty context. */
        URIContext();

        /** Creates a context that specifies a referring object. */
        URIContext(const std::string& referrer);

        /** Copy constructor. */
        URIContext(const URIContext& rhs);

        /** dtor */
        virtual ~URIContext() { }

        /** Serializes this context to an Options structure. This is useful when passing context information
            to an osgDB::ReaderWriter that takes a stream as input -- the stream is anonymous, therefore this
            is the preferred way to communicate the context information. */
        void store(IOOptions* options);

        /** Creates a context from the serialized version in an Options structure (see above) */
        URIContext(const IOOptions* options);

        /** Returns the referring object. */
        const std::string& referrer() const { return _referrer; }

        /** True if the context is empty */
        bool empty() const { return _referrer.empty(); }

        /** Parents the input context with the current object, placing the subContext's information
            under it. Used to re-parent relative locations with a higher-level referrer object. */
        URIContext add(const URIContext& subContext) const;

        /** Returns a new context with the sub path appended to the current referrer path. */
        URIContext add(const std::string& subPath) const;

        /** creates a string suitable for passing to an implementation */
        std::string getCanonicalPath(const std::string& target) const;

        //! Add a header name/value pair to use when requesting a remote URL
        void addHeader(const std::string& name, const std::string& value);

        //! Headers in this URIContext
        const Headers& getHeaders() const;

        //! Headers in this URIContext
        Headers& getHeaders();


    private:
        friend class URI;
        std::string _referrer;
        Headers _headers;
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

        //! Content resulting from a URI request.
        struct Content
        {
            Stream data;
            std::string contentType;
        };

    public:
        //! Constructs an empty (and invalid) URI.
        URI();

        //! Constructs a new URI from a location (typically an absolute url)
        URI(const std::string& location);

        //! Constructs a new URI from a location and an existing context.
        URI(const std::string& location, const URIContext& context);

        //! Constructs a new URI from a string.
        URI(const char* location);

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

        /** Returns a copy of this URI with the suffix appended */
        URI append(const std::string& suffix) const;

        /** String used for keying the cache */
        const std::string& cacheKey() const { return !_cacheKey.empty() ? _cacheKey : _fullURI; }

        /** osgDB::Options option string (plugin options) */
        optional<std::string>& optionString() { return _optionString; }
        const optional<std::string>& optionString() const { return _optionString; }

        /** Sets a cache key. By default the cache key is the full URI, but you can override that. */
        void setCacheKey(const std::string& key) { _cacheKey = key; }

        //! Reads the URI and then tries to create a new object
        //! from the data buffer.
        //template<typename T>
        //IOResult<T> read(IOControl* io) const;

        //! Reads the URI into a data buffer
        IOResult<URI::Content> read(const IOOptions& io) const;

    public:

        bool operator < (const URI& rhs) const { return _fullURI < rhs._fullURI; }

        bool operator == (const URI& rhs) const { return _fullURI.compare(rhs._fullURI) == 0; }

        bool operator != (const URI& rhs) const { return _fullURI.compare(rhs._fullURI) != 0; }

    public:
        URI(const URI& rhs);

    public: // config methods

        Config getConfig() const;

        void mergeConfig(const Config& conf);

    public: // Static convenience methods

        /** Encodes text to URL safe test. Escapes special charaters */
        inline static std::string urlEncode(const std::string &value);

    protected:
        std::string _baseURI;
        std::string _fullURI;
        std::string _cacheKey;
        URIContext _context;
        optional<std::string> _optionString;
        //IOResult<std::string> read_to_string() const;
        void ctorCacheKey();
    };


    // Config specializations for URI:

    template<> inline
        void Config::set<URI>(const std::string& key, const optional<URI>& opt) {
        if (opt.has_value()) {
            remove(key);
            set(key, opt->getConfig());
        }
    }

    template<> inline
        bool Config::get<URI>(const std::string& key, optional<URI>& output) const {
        if (hasChild(key)) {
            const Config& uriconf = child(key);
            if (!uriconf.value().empty()) {
                output = URI(uriconf.value(), uriconf.referrer());
                output->mergeConfig(uriconf);
                return true;
            }
            else return false;
        }
        else return false;
    }
}
