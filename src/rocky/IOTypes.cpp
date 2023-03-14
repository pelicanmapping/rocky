/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IOTypes.h"
#include "Instance.h"

using namespace ROCKY_NAMESPACE;

const std::string IOMetadata::CONTENT_TYPE = "Content-Type";

ProxySettings::ProxySettings(const Config& conf)
{
    hostname = conf.value<std::string>("host", "");
    port = conf.value<int>("port", 8080);
    username = conf.value<std::string>("username", "");
    password = conf.value<std::string>("password", "");
}

Config
ProxySettings::getConfig() const
{
    Config conf("proxy");
    conf.add("host", hostname);
    conf.add("port", std::to_string(port));
    conf.add("username", username);
    conf.add("password", password);
    return conf;
}



//statics
CachePolicy CachePolicy::DEFAULT;
CachePolicy CachePolicy::NO_CACHE(CachePolicy::Usage::NO_CACHE);
CachePolicy CachePolicy::CACHE_ONLY(CachePolicy::Usage::CACHE_ONLY);

//------------------------------------------------------------------------

CachePolicy::CachePolicy()
{
    //nop
}
CachePolicy::CachePolicy(const Usage& u) :
    usage(u)
{
    //nop
}

CachePolicy::CachePolicy(const Config& conf)
{
    conf.get("usage", "read_write", usage, Usage::READ_WRITE);
    conf.get("usage", "read_only", usage, Usage::READ_ONLY);
    conf.get("usage", "cache_only", usage, Usage::CACHE_ONLY);
    conf.get("usage", "no_cache", usage, Usage::NO_CACHE);
    conf.get("usage", "none", usage, Usage::NO_CACHE);
    conf.get("max_age", maxAge);
    conf.get("min_time", minTime);
}

Config
CachePolicy::getConfig() const
{
    Config conf("cache_policy");
    conf.set("usage", "read_write", usage, Usage::READ_WRITE);
    conf.set("usage", "read_only", usage, Usage::READ_ONLY);
    conf.set("usage", "cache_only", usage, Usage::CACHE_ONLY);
    conf.set("usage", "no_cache", usage, Usage::NO_CACHE);
    conf.set("max_age", maxAge);
    conf.set("min_time", minTime);
    return conf;
}

void
CachePolicy::mergeAndOverride(const CachePolicy& rhs)
{
    if (rhs.usage.has_value())
        usage = rhs.usage.value();

    if (rhs.minTime.has_value())
        minTime = rhs.minTime.value();

    if (rhs.maxAge.has_value())
        maxAge = rhs.maxAge.value();
}

void
CachePolicy::mergeAndOverride(const optional<CachePolicy>& rhs)
{
    if (rhs.has_value())
    {
        mergeAndOverride(rhs.value());
    }
}

DateTime
CachePolicy::getMinAcceptTime() const
{
    return
        minTime.has_value() ? minTime.value() :
        maxAge.has_value() ? DateTime().asTimeStamp() - maxAge.value() :
        0;
}

bool
CachePolicy::isExpired(TimeStamp lastModified) const
{
    return lastModified < getMinAcceptTime().asTimeStamp();
}

bool
CachePolicy::operator == (const CachePolicy& rhs) const
{
    return
        (usage.value() == rhs.usage.value()) &&
        (maxAge.value() == rhs.maxAge.value()) &&
        (minTime.value() == rhs.minTime.value());
}

CachePolicy&
CachePolicy::operator = (const CachePolicy& rhs)
{
    usage = optional<Usage>(rhs.usage);
    maxAge = optional<Duration>(rhs.maxAge);
    minTime = optional<DateTime>(rhs.minTime);

    return *this;
}

std::string
CachePolicy::usageString() const
{
    if (usage == Usage::READ_WRITE) return "read-write";
    if (usage == Usage::READ_ONLY)  return "read-only";
    if (usage == Usage::CACHE_ONLY)  return "cache-only";
    if (usage == Usage::NO_CACHE)    return "no-cache";
    return "unknown";
}


#if 0
bool
ProxySettings::fromOptions( const osgDB::Options* dbOptions, optional<ProxySettings>& out )
{
    if ( dbOptions )
    {
        std::string jsonString = dbOptions->getPluginStringData( "rocky::ProxySettings" );
        if ( !jsonString.empty() )
        {
            Config conf;
            conf.fromJSON( jsonString );
            out = ProxySettings( conf );
            return true;
        }
    }
    return false;
}

void
ProxySettings::apply( osgDB::Options* dbOptions ) const
{
    if ( dbOptions )
    {
        Config conf = getConfig();
        dbOptions->setPluginStringData( "rocky::ProxySettings", conf.toJSON() );
    }
}
#endif


//------------------------------------------------------------------------

#if 0
/**
 * This is an OSG reader/writer template. We're using this to register 
 * readers for "XML" and "JSON" format -- these are just text files, but we
 * have to do this to enable OSG to read them from inside an archive (like
 * a ZIP file). For example, this lets OSG read the tilemap.xml file stores
 * with a TMS repository in a ZIP.
 */

#define STRING_READER_WRITER_SHIM(SUFFIX, EXTENSION, DEF) \
struct rockyStringReaderWriter##SUFFIX : public osgDB::ReaderWriter \
{ \
    rockyStringReaderWriter##SUFFIX () { \
        supportsExtension( EXTENSION, DEF ); \
    } \
    osgDB::ReaderWriter::ReadResult readObject(const std::string& uri, const osgDB::Options* dbOptions) const { \
        std::string ext = osgDB::getLowerCaseFileExtension( uri ); \
        if ( !acceptsExtension(ext) ) return osgDB::ReaderWriter::ReadResult::FILE_NOT_HANDLED; \
        rocky::ReadResult r = URI( uri ).readString( dbOptions ); \
        if ( r.succeeded() ) return r.release<StringObject>(); \
        return osgDB::ReaderWriter::ReadResult::ERROR_IN_READING_FILE; \
    } \
    osgDB::ReaderWriter::ReadResult readObject(std::istream& in, const osgDB::Options* dbOptions ) const { \
        URIContext uriContext( dbOptions ); \
        return new StringObject( Stringify() << in.rdbuf() ); \
    } \
    osgDB::ReaderWriter::WriteResult writeObject( const osg::Object& obj, const std::string& location, const osgDB::Options* dbOptions ) const { \
        std::string ext = osgDB::getLowerCaseFileExtension(location); \
        if ( !acceptsExtension(ext) ) return osgDB::ReaderWriter::WriteResult::FILE_NOT_HANDLED; \
        const StringObject* so = dynamic_cast<const StringObject*>(&obj); \
        if ( !so ) return osgDB::ReaderWriter::WriteResult::FILE_NOT_HANDLED; \
        std::ofstream out(location.c_str()); \
        if ( out.is_open() ) { out << so->getString(); out.close(); return osgDB::ReaderWriter::WriteResult::FILE_SAVED; } \
        return osgDB::ReaderWriter::WriteResult::ERROR_IN_WRITING_FILE; \
    } \
}

STRING_READER_WRITER_SHIM( XML, "xml", "rocky XML shim" );
REGISTER_OSGPLUGIN( xml, rockyStringReaderWriterXML );

STRING_READER_WRITER_SHIM( JSON, "json", "rocky JSON shim" );
REGISTER_OSGPLUGIN( json, rockyStringReaderWriterJSON );
#endif


IOOptions::IOOptions() :
    _cancelable(nullptr)
{
    // nop
}

IOOptions::IOOptions(const IOOptions& rhs)
{
    this->operator=(rhs);
}

IOOptions::IOOptions(Cancelable& c) :
    _cancelable(&c)
{
    //nop
}

IOOptions::IOOptions(const IOOptions& rhs, Cancelable& c) :
    _services(rhs._services),
    _properties(rhs._properties),
    _cancelable(&c)
{
    //nop
}

IOOptions&
IOOptions::operator = (const IOOptions& rhs)
{
    _cancelable = rhs._cancelable;
    _services = rhs._services;
    _properties = rhs._properties;
    return *this;
}

namespace
{
    ReadImageURIService default_read_image_from_uri = [](const std::string&, const IOOptions&) { return Status(Status::ServiceUnavailable); };
    ReadImageStreamService default_read_image_from_stream = [](std::istream&, const std::string&, const IOOptions&) { return Status(Status::ServiceUnavailable); };
    CacheService default_cache = []() { return nullptr; };
}

Services::Services() :
    readImageFromURI(default_read_image_from_uri),
    readImageFromStream(default_read_image_from_stream),
    cache(default_cache)
{
    //nop
}
