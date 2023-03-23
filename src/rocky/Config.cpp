/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Config.h"
#include "URI.h"
#include "Utils.h"

#ifdef TINYXML_FOUND
#include <tinyxml.h>
#endif

#ifdef NLOHMANN_JSON_FOUND
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

using namespace ROCKY_NAMESPACE;

#define LC "[Config] "

namespace
{
#ifdef TINYXML_FOUND

    Config xml_node_to_config(Config* parent, const TiXmlNode* node)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(node != nullptr, Config());

        Config conf;

        if (node->Type() == TiXmlNode::TINYXML_ELEMENT)
        {
            const TiXmlElement* element = node->ToElement();
            conf.key() = util::toLower(element->Value());
            // attributes:
            for (const TiXmlAttribute* attr = element->FirstAttribute(); attr != nullptr; attr = attr->Next())
            {
                Config attr_conf(util::toLower(attr->Name()), attr->Value());
                if (!attr_conf.empty())
                {
                    conf.add(attr_conf);
                }
            }
        }

        else if (node->Type() == TiXmlNode::TINYXML_TEXT)
        {
            const TiXmlText* text = node->ToText();
            if (parent)
                parent->setValue(text->Value());
        }

        for (const TiXmlNode* child = node->FirstChild(); child != nullptr; child = child->NextSibling())
        {
            if (child)
            {
                Config child_conf = xml_node_to_config(&conf, child);
                if (!child_conf.empty())
                {
                    conf.add(child_conf);
                }
            }
        }

        return conf;
    }

    Config xml_to_config(const TiXmlDocument& doc)
    {
        Config conf;
        if (doc.RootElement())
        {
            conf = xml_node_to_config(nullptr, doc.RootElement());
        }
        return conf;
    }
#endif // TINYXML_FOUND

#ifdef NLOHMANN_JSON_FOUND

    //! example
    //! {
    //!   "key" : "layer"
    //!   "children" : [
    //!     { "key" : "name", "value" : "imagery" },
    //!     { "key" : "caching", "value" : "true" }
    //!   ]
    //! }
    //! 
    //! {
    //!   "layer" : {
    //!       "$value" : "...",
    //!       "name" : "imagery",
    //!       "caching" : false,
    //!       "attribution" : "openstreetmap",
    //!       "policy" : {
    //!           "usage" : "READ_WRITE",
    //!           "min_time" : "0"
    //!       }
    //!   }
    //! }
    //! 
    //! {
    //!   "$key" : "layer",
    //!   "$value" : "...",
    //!   "$attrs" : {
    //!       "name" : "imagery",
    //!       "caching" : "true",
    //!       "attribution", "openstreetmap"
    //!   },
    //!   "$children" : [
    //!       "cache_policy" : {
    //!       },
    //!       "
    //!   ]

    json conf_to_json(const Config& conf)
    {
        json object;

        if (conf.isSimple())
        {
            object[conf.key()] = conf.value();
            return object;
        }

        if (!conf.value().empty())
        {
            object["$value"] = conf.value();
        }

        if (conf.children().size() > 0)
        {
            std::map<std::string, std::vector<Config>> sets; // sorted by key
            for (auto& child : conf.children())
            {
                sets[child.key()].push_back(child);
            }

            for (auto iter : sets)
            {
                for (auto& child : conf.children())
                {
                    if (child.isSimple())
                    {
                        object[child.key()] = child.value();
                    }
                    else
                    {
                        object[child.key()] = conf_to_json(child);
                    }
                }
            }
        }

        return object;
    }

    Config json_to_conf(const json& j)
    {
        Config conf;

        if (j.is_object())
        {
            for (auto iter = j.begin(); iter != j.end(); ++iter)
            {
                if (iter.key() == "$value")
                {
                    conf.setValue(iter->get<std::string>());
                }
                else
                {
                    if (iter->is_primitive())
                    {
                        conf.add(iter.key(), iter->get<std::string>());
                    }
                    else if (iter->is_object())
                    {
                        conf.add(iter.key(), json_to_conf(iter.value()));
                    }
                }
            }
        }

        if (conf.value().empty() && conf.children().size() == 1)
        {
            return conf.children().front();
        }
        else
        {
            return conf;
        }
    }

#endif
}


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
#ifdef TINYXML_FOUND

    //Read the entire document into a string
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string xml_str = buffer.str();

    // strip the DOCTYPE...apparently tinyxml doesn't like it
    auto dtd = xml_str.find("<!DOCTYPE");
    if (dtd != xml_str.npos)
    {
        auto dtd_end = xml_str.find_first_of('>', dtd);
        if (dtd_end != xml_str.npos)
            xml_str = xml_str.substr(dtd_end + 1);
    }

    TiXmlDocument doc;
    doc.Parse(xml_str.c_str());

    if (doc.Error())
    {
        std::stringstream buf;
        buf << "XML parsing error at row " << doc.ErrorRow() << " col " << doc.ErrorCol() << std::endl;
        set("error", buf.str());
        return false;
    }

    *this = xml_to_config(doc);
    return true;
#else
    ROCKY_SOFT_ASSERT_AND_RETURN(false, false, "XML support unavailable");
#endif
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



std::string
Config::toJSON(bool pretty) const
{
    json object = { { key(), conf_to_json(*this) } };
    return pretty ? object.dump(4) : object.dump();
}

bool
Config::fromJSON(const std::string& input)
{
    try {
        *this = json_to_conf(json::parse(input));
        return true;
    }
    catch (...) {
        return false;
    }
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
