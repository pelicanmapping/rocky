/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Config.h"
#include "Notify.h"
#include "URI.h"
#include "StringUtils.h"
#include <tinyxml.h>
#include <nlohmann/json.hpp>

using namespace rocky;

#define LC "[Config] "

void
Config::setReferrer(const std::string& referrer)
{
    if ( referrer.empty() )
        return;

    std::string result;

    if (URI(referrer).isRemote())
    {
        result = referrer;
    }
    else
    {
        util::Path path(referrer);

        ROCKY_SOFT_ASSERT_AND_RETURN(
            path.is_absolute(), void(),
            "ILLEGAL: call to setReferrer with relative path");

        result = path.string();
    }

    // Don't overwrite an existing referrer:
    if (_referrer.empty())
    {
        _referrer = result;
    }

    for (auto& child : _children)
    {
        child.setReferrer(result);
    }
}

bool
Config::fromXML(std::istream& in)
{
    auto result = util::parseXML(in, referrer());
    if (result.status.failed())
    {
        set("error", result.status.message);
        return false;
    }

    const TiXmlDocument& doc = result.value;

    ROCKY_TODO("Parse the XML into a Config");

    return true;
}

const Config&
Config::child(const std::string& childName) const
{
    for (ConfigSet::const_iterator i = _children.begin(); i != _children.end(); i++) {
        if (i->key() == childName)
            return *i;
    }
    static Config s_emptyConf;
    return s_emptyConf;
}

const Config*
Config::child_ptr(const std::string& childName) const
{
    for (ConfigSet::const_iterator i = _children.begin(); i != _children.end(); i++) {
        if (i->key() == childName)
            return &(*i);
    }
    return 0L;
}

Config*
Config::mutable_child(const std::string& childName)
{
    for (ConfigSet::iterator i = _children.begin(); i != _children.end(); i++) {
        if (i->key() == childName)
            return &(*i);
    }

    return 0L;
}

void
Config::merge(const Config& rhs)
{
    // remove any matching keys first; this will allow the addition of multi-key values
    for (ConfigSet::const_iterator c = rhs._children.begin(); c != rhs._children.end(); ++c)
        remove(c->key());

    // add in the new values.
    for (ConfigSet::const_iterator c = rhs._children.begin(); c != rhs._children.end(); ++c)
        add(*c);
}

const Config*
Config::find(const std::string& key, bool checkMe) const
{
    if (checkMe && key == this->key())
        return this;

    for (ConfigSet::const_iterator c = _children.begin(); c != _children.end(); ++c)
        if (key == c->key())
            return &(*c);

    for (ConfigSet::const_iterator c = _children.begin(); c != _children.end(); ++c)
    {
        const Config* r = c->find(key, false);
        if (r) return r;
    }

    return 0L;
}

Config*
Config::find(const std::string& key, bool checkMe)
{
    if (checkMe && key == this->key())
        return this;

    for (ConfigSet::iterator c = _children.begin(); c != _children.end(); ++c)
        if (key == c->key())
            return &(*c);

    for (ConfigSet::iterator c = _children.begin(); c != _children.end(); ++c)
    {
        Config* r = c->find(key, false);
        if (r) return r;
    }

    return 0L;
}

/****************************************************************/
ConfigOptions::~ConfigOptions()
{
}

Config
ConfigOptions::getConfig() const
{
    // iniialize with the raw original conf. subclass getConfig's can 
    // override the values there.
    Config conf = _conf;
    conf.setReferrer(referrer());
    return conf;
}

/****************************************************************/
DriverConfigOptions::~DriverConfigOptions()
{
}

#if 0
namespace
{
    // Converts a Config to JSON. The "nicer" flag formats the data in a more 
    // readable way than nicer=false. Nicer=true attempts to create JSON "objects",
    // whereas nicer=false makes "$key" and "$children" members.
    Json::Value conf2json(const Config& conf, bool nicer, int depth)
    {
        Json::Value value( Json::objectValue );

        if ( conf.isSimple() )
        {
            value[ conf.key() ] = conf.value();
        }
        else
        {
            if ( !nicer )
            {
                if ( !conf.key().empty() )
                {
                    value["$key"] = conf.key();
                }
            }

            if ( !conf.value().empty() )
            {
                value["$value"] = conf.value();
            }

            if ( conf.children().size() > 0 )
            {
                if ( nicer )
                {
                    std::map< std::string, std::vector<Config> > sets; // sorted

                    // sort into bins by name:
                    for( ConfigSet::const_iterator c = conf.children().begin(); c != conf.children().end(); ++c )
                    {
                        sets[c->key()].push_back( *c );
                    }

                    for( std::map<std::string,std::vector<Config> >::iterator i = sets.begin(); i != sets.end(); ++i )
                    {
                        if ( i->second.size() == 1 )
                        {
                            Config& c = i->second[0];
                            if ( c.isSimple() )
                            {
                                if (c.isNumber())
                                    value[i->first] = c.valueAs<double>(0.0);
                                else
                                    value[i->first] = c.value();
                            }
                            else
                            {
                                Json::Value child = conf2json(c, nicer, depth+1);
                                if (child.isObject())
                                {
                                    value[i->first] = child;
                                }   
                            }
                        }
                        else
                        {
                            std::string array_key = Stringify() << i->first << "__array__";
                            Json::Value array_value( Json::arrayValue );
                            for( std::vector<Config>::iterator j = i->second.begin(); j != i->second.end(); ++j )
                            {
                                array_value.append( conf2json(*j, nicer, depth+1) );
                            }
                            value[array_key] = array_value;
                            //value = array_value;
                        }
                    }
                }
                else
                {
                    Json::Value children( Json::arrayValue );
                    unsigned i = 0;

                    for( ConfigSet::const_iterator c = conf.children().begin(); c != conf.children().end(); ++c )
                    {
                        if ( c->isSimple() )
                            value[c->key()] = c->value();
                        else
                            children[i++] = conf2json(*c, nicer, depth+1);
                    }

                    if ( !children.empty() )
                    {
                        value["$children"] = children;
                    }
                }
            }

            // At the root, embed the Config in a single JSON object.
            if ( depth == 0 )
            {
                Json::Value root;
                root[conf.key()] = value;
                value = root;
            }
        }

        return value;
    }

    void json2conf(const Json::Value& json, Config& conf, int depth)
    {
        if ( json.type() == Json::objectValue )
        {
            Json::Value::Members members = json.getMemberNames();

            for( Json::Value::Members::const_iterator i = members.begin(); i != members.end(); ++i )
            {
                const Json::Value& value = json[*i];

                if ( value.isObject() )
                {
                    if (depth == 0 && members.size() == 1)
                    {
                        conf.key() = *i;
                        json2conf(value, conf, depth+1);
                    }
                    else
                    {
                        Config element( *i );
                        json2conf( value, element, depth+1 );
                        conf.add( element );
                    }
                }
                else if ( value.isArray() )
                {
                    if ( endsWith(*i, "__array__") )
                    {
                        std::string key = i->substr(0, i->length()-9);
                        for( Json::Value::const_iterator j = value.begin(); j != value.end(); ++j )
                        {
                            Config child;
                            json2conf( *j, child, depth+1 );
                            conf.add( key, child );
                        }
                    }
                    else if ( endsWith(*i, "_$set") ) // backwards compatibility
                    {
                        std::string key = i->substr(0, i->length()-5);
                        for( Json::Value::const_iterator j = value.begin(); j != value.end(); ++j )
                        {
                            Config child;
                            json2conf( *j, child, depth+1 );
                            conf.add( key, child );
                        }
                    }
                    else
                    {
                        Config element( *i );
                        json2conf( value, element, depth+1 );
                        conf.add( element );
                    }
                }
                else if ( (*i) == "$key" )
                {
                    conf.key() = value.asString();
                }
                else if ( (*i) == "$value" )
                {
                    conf.setValue(value.asString());
                }
                else if ( (*i) == "$children" && value.isArray() )
                {
                    json2conf( value, conf, depth+1 );
                }
                else
                {
                    if( value.isBool())
                        conf.add(*i, value.asBool());
                    else if( value.isDouble())
                        conf.add(*i, value.asDouble());
                    else if (value.isInt())
                        conf.add(*i, value.asInt());
                    else if (value.isUInt())
                        conf.add(*i, value.asUInt());
                    else
                        conf.add(*i, value.asString());
                }
            }
        }
        else if ( json.type() == Json::arrayValue )
        {          
            for( Json::Value::const_iterator j = json.begin(); j != json.end(); ++j )
            {
                Config child;
                json2conf( *j, child, depth+1 );
                if ( !child.empty() )
                    conf.add( child );
            }
        }
        else if ( json.type() != Json::nullValue )
        {
            conf.setValue(json.asString());
        }
    }
}
#endif

std::string
Config::toJSON(bool pretty) const
{
    ROCKY_TODO("Write config-to-json using nlohmann OR replace config with json");
    return "";
    //Json::Value root = conf2json( *this, true, 0 );
    //if ( pretty )
    //    return Json::StyledWriter().write( root );
    //else
    //    return Json::FastWriter().write( root );
}

bool
Config::fromJSON( const std::string& input )
{
    ROCKY_TODO("Write json-to-config using nlohmann OR replace config with json");
    return false;
#if 0
    Json::Reader reader;
    Json::Value root( Json::objectValue );
    if ( reader.parse( input, root ) )
    {
        json2conf( root, *this, 0 );
        return true;
    }
    else
    {
        ROCKY_WARN 
            << "JSON decoding error: "
            << reader.getFormatedErrorMessages() 
            << std::endl;
    }
    return false;
#endif
}

Config
Config::readJSON(const std::string& json)
{
    Config conf;
    conf.fromJSON(json);
    return conf;
}

Config
Config::operator - ( const Config& rhs ) const
{
    Config result( *this );

    for( ConfigSet::const_iterator i = rhs.children().begin(); i != rhs.children().end(); ++i )
    {
        result.remove( i->key() );
    }

    return result;
}
