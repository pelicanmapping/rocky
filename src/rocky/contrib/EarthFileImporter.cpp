/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "EarthFileImporter.h"
#include "../json.h"

#ifdef TINYXML_FOUND
#include <tinyxml.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
#ifdef TINYXML_FOUND
    std::string get_text_value(TiXmlNode* parent)
    {
        if (parent)
        {
            for (auto kid = parent->FirstChild(); kid; kid = kid->NextSibling())
            {
                if (kid && kid->ToText())
                {
                    return kid->ToText()->Value();
                }
            }
        }
        return {};
    }

    // map earth file property names to rocky names for common general properties
    std::string map_property_name(const char* name)
    {
        if (util::ciEquals(name, "url"))
            return "uri";
        else
            return name;
    }

    void collect_children_recursively(TiXmlNode* parent_xml, json& parent_json)
    {
        for (auto child = parent_xml->FirstChild(); child; child = child->NextSibling())
        {
            // first see if this element has a text child, and if so
            // that is our simple value.
            auto text_value = util::trim(get_text_value(child));
            if (!text_value.empty())
            {
                parent_json[map_property_name(child->Value())] = text_value;
            }

            // otherwise create a new json object as the child.
            else if (child->ToElement() && !child->ValueStr().empty())
            {
                json child_json = json::object();
                parent_json[map_property_name(child->Value())] = child_json;
                collect_children_recursively(child, child_json);
            }
        }
    }

    Result<TiXmlDocument> load_include_file(const URI& href, const IOOptions& io)
    {
        auto result = href.read(io);
        if (result.status.ok())
        {
            TiXmlDocument doc;
            doc.Parse(result.value.data.c_str());
            if (doc.Error() || !doc.RootElement())
            {
                return Status(Status::GeneralError, util::make_string()
                    << "Include file - XML parse error at row " << doc.ErrorRow()
                    << " col " << doc.ErrorCol());
            }

            return doc;
        }
        return Status(Status::ResourceUnavailable, "Failed to load include file");
    }

#endif // TINYXML_FOUND
}

EarthFileImporter::EarthFileImporter()
{
    //nop
}

Result<JSON>
EarthFileImporter::read(const std::string& location, const IOOptions& io) const
{
#ifdef TINYXML_FOUND
    
    // try to load the earth file into a string:
    URI uri(location);

    auto result = uri.read(io);
    if (result.status.failed())
    {
        return Status(Status::ResourceUnavailable);
    }

    // try to parse the string into an XML document:
    TiXmlDocument doc;
    doc.Parse(result.value.data.c_str());
    if (doc.Error() || !doc.RootElement())
    {
        return Status(Status::GeneralError, util::make_string()
            << "XML parse error at row " << doc.ErrorRow()
            << " col " << doc.ErrorCol());
    }

    // Confirm a top-level "map" element:
    auto mapxml = doc.RootElement();
    if (!util::ciEquals(mapxml->Value(), "map"))
    {
        return Status(Status::ConfigurationError, "XML missing top-level Map element");
    }

    json map = json::object();

    // keep them here do they don't destruct ;)
    std::vector<TiXmlDocument> included_docs;

    // Collect the layers. Layer tag goes into the "type" property.
    // Earth files let you put properties in either XML attributes OR as child elements,
    // so we have to check for both.
    auto layers_json = json::array();

    for (auto child = mapxml->FirstChild(); child != nullptr; child = child->NextSibling())
    {
        auto child_el = child->ToElement();
        if (child_el)
        {
            // Include files
            if (util::ciEquals(child->ValueStr(), "xi:include"))
            {
                std::string href;
                if (child_el->QueryStringAttribute("href", &href) == TIXML_SUCCESS)
                {
                    auto included_doc = load_include_file(URI(href, location), io);
                    if (included_doc.status.ok())
                    {
                        included_docs.push_back(included_doc.value);
                        child_el = included_docs.back().RootElement();
                    }
                }
            }
        
            if (child_el) // check again since the xi:include may have changed things
            {
                // special treatment of the options object
                if (util::ciEquals(child_el->ValueStr(), "options"))
                {
                    auto options_json = json::object();
                    collect_children_recursively(child_el, options_json);
                    map["options"] = options_json;
                }

                else // it's a layer
                {
                    json layer_json = json::object();
                    layer_json["type"] = child->Value();

                    // Attributes:
                    for (auto attr = child_el->FirstAttribute(); attr; attr = attr->Next())
                    {
                        layer_json[attr->Name()] = attr->ValueStr();
                    }

                    // Children:
                    collect_children_recursively(child_el, layer_json);

                    layers_json.push_back(layer_json);
                }
            }
        }
    }

    map["layers"] = layers_json;
    return to_string(map);

#else
    return Status(Status::ServiceUnavailable, "XML support is not available");
#endif
}
