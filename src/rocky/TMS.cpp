/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TMS.h"

#include "tinyxml/tinyxml.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::TMS;
using namespace ROCKY_NAMESPACE::util;

namespace
{
    bool intersects(const double& minXa, const double& minYa, const double& maxXa, const double& maxYa,
        const double& minXb, const double& minYb, const double& maxXb, const double& maxYb)
    {
        return std::max(minXa, minXb) <= std::min(maxXa, maxXb) &&
            std::max(minYa, minYb) <= std::min(maxYa, maxYb);
    }

    std::string getHorizSRSString(const SRS& srs)
    {
        if (srs.horizontallyEquivalentTo(SRS::SPHERICAL_MERCATOR))
        {
            return "epsg:3785";
        }
        else if (srs.isGeodetic())
        {
            return "epsg:4326";
        }
        else
        {
            return srs.definition();
        }
    }

    std::string getChildTextValue(const TiXmlElement* node)
    {
        auto text = dynamic_cast<const TiXmlText*>(node->FirstChild());
        return text ? text->Value() : "";
    }

    const TiXmlElement* find(const std::string& tag, const TiXmlElement* node)
    {
        if (!node)
            return nullptr;

        if (util::toLower(node->ValueStr()) == util::toLower(tag))
            return node;

        for (const TiXmlNode* childnode = node->FirstChild();
            childnode != nullptr;
            childnode = childnode->NextSibling())
        {
            auto match = find(tag, childnode->ToElement());
            if (match)
                return match;
        }

        return nullptr;
    }

    Result<TileMap> parseTileMapFromXML(const std::string& xml)
    {
        TileMap tilemap;

        TiXmlDocument doc;
        doc.Parse(xml.c_str());
        if (doc.Error())
        {
            return Failure(Failure::GeneralError, "XML parse error at row " + std::to_string(doc.ErrorRow())
                + " col " + std::to_string(doc.ErrorCol()));
        }

        auto tilemapxml = find("tilemap", doc.RootElement());
        if (!tilemapxml)
            return Failure(Failure::ConfigurationError, "XML missing TileMap element");

        tilemapxml->QueryStringAttribute("version", &tilemap.version);
        tilemapxml->QueryStringAttribute("tilemapservice", &tilemap.tileMapService);

        for (const TiXmlNode* childnode = tilemapxml->FirstChild(); 
            childnode != nullptr; 
            childnode = childnode->NextSibling())
        {
            auto childxml = childnode->ToElement();
            if (childxml)
            {
                std::string name = util::toLower(childxml->Value());
                if (name == "abstract")
                {
                    tilemap.abstract = getChildTextValue(childxml);
                }
                else if (name == "title")
                {
                    tilemap.title = getChildTextValue(childxml);
                }
                else if (name == "srs")
                {
                    tilemap.srsString = getChildTextValue(childxml);
                }
                else if (name == "boundingbox")
                {
                    childxml->QueryDoubleAttribute("minx", &tilemap.minX);
                    childxml->QueryDoubleAttribute("miny", &tilemap.minY);
                    childxml->QueryDoubleAttribute("maxx", &tilemap.maxX);
                    childxml->QueryDoubleAttribute("maxy", &tilemap.maxY);
                }
                else if (name == "origin")
                {
                    childxml->QueryDoubleAttribute("x", &tilemap.originX);
                    childxml->QueryDoubleAttribute("y", &tilemap.originY);
                }
                else if (name == "tileformat")
                {
                    childxml->QueryUnsignedAttribute("width", &tilemap.format.width);
                    childxml->QueryUnsignedAttribute("height", &tilemap.format.height);
                    childxml->QueryStringAttribute("mime-type", &tilemap.format.mimeType);
                    childxml->QueryStringAttribute("extension", &tilemap.format.extension);
                }
                else if (name == "tilesets")
                {
                    std::string temp;
                    childxml->QueryStringAttribute("profile", &temp);
                    tilemap.profileType =
                        temp == "global-geodetic" ? ProfileType::GEODETIC :
                        temp == "global-mercator" ? ProfileType::MERCATOR :
                        temp == "local" ? ProfileType::LOCAL :
                        ProfileType::UNKNOWN;

                    for (const TiXmlNode* tilesetnode = childxml->FirstChild();
                        tilesetnode != nullptr;
                        tilesetnode = tilesetnode->NextSibling())
                    {
                        auto tilesetxml = tilesetnode->ToElement();
                        if (tilesetxml)
                        {
                            TileSet tileset;
                            tilesetxml->QueryStringAttribute("href", &tileset.href);
                            tilesetxml->QueryDoubleAttribute("units-per-pixel", &tileset.unitsPerPixel);
                            tilesetxml->QueryUnsignedAttribute("order", &tileset.order);
                            tilemap.tileSets.push_back(std::move(tileset));
                        }
                    }
                }
                else if (name == "dataextents")
                {
                    Profile profile = tilemap.createProfile();

                    for (const TiXmlNode* denode = childxml->FirstChild();
                        denode != nullptr;
                        denode = denode->NextSibling())
                    {
                        auto dexml = denode->ToElement();
                        if (dexml)
                        {
                            double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
                            unsigned maxLevel = 0;
                            std::string description;

                            dexml->QueryDoubleAttribute("minx", &minX);
                            dexml->QueryDoubleAttribute("miny", &minY);
                            dexml->QueryDoubleAttribute("maxx", &maxX);
                            dexml->QueryDoubleAttribute("maxy", &maxY);
                            dexml->QueryUnsignedAttribute("maxlevel", &maxLevel);
                            dexml->QueryStringAttribute("description", &description);

                            // Note: TMS DataExtents are a Pelican extension and are ALWAYS in WGS84.
                            auto e = GeoExtent(SRS::WGS84, minX, minY, maxX, maxY).transform(profile.srs());

                            if (e.valid())
                            {
                                if (maxLevel > 0u)
                                {
                                    if (description.empty())
                                        tilemap.dataExtents.push_back(DataExtent(e, 0, maxLevel));
                                    else
                                        tilemap.dataExtents.push_back(DataExtent(e, 0, maxLevel, description));
                                }
                                else
                                {
                                    if (description.empty())
                                        tilemap.dataExtents.push_back(DataExtent(e, 0));
                                    else
                                        tilemap.dataExtents.push_back(DataExtent(e, 0, description));
                                }
                            }
                        }
                    }
                }
            }
        }

        // Now, clean up any messes.
         
        // Try to compute the profile based on the SRS if there was no PROFILE tag given
        if (tilemap.profileType == ProfileType::UNKNOWN && !tilemap.srsString.empty())
        {
            SRS srs(tilemap.srsString);

            tilemap.profileType =
                srs.isGeodetic() ? ProfileType::GEODETIC :
                srs.horizontallyEquivalentTo(SRS::SPHERICAL_MERCATOR) ? ProfileType::MERCATOR :
                srs.isProjected() ? ProfileType::LOCAL :
                ProfileType::UNKNOWN;
        }

        tilemap.computeMinMaxLevel();
        tilemap.computeNumTiles();
        tilemap.generateTileSets(20u);

        return tilemap;
    }
}


bool
TileMap::valid() const
{
    return profileType != ProfileType::UNKNOWN;
}

void TileMap::computeMinMaxLevel()
{
    minLevel = INT_MAX;
    maxLevel = 0;
    for (auto& tileSet : tileSets)
    {
        if (tileSet.order < minLevel) minLevel = tileSet.order;
        if (tileSet.order > maxLevel) maxLevel = tileSet.order;
    }
}

void TileMap::computeNumTiles()
{
    numTilesWide = -1;
    numTilesHigh = -1;

    if (tileSets.size() > 0)
    {
        unsigned int level = tileSets[0].order;
        double res = tileSets[0].unitsPerPixel;

        numTilesWide = (int)((maxX - minX) / (res * format.width));
        numTilesHigh = (int)((maxY - minY) / (res * format.width));

        //In case the first level specified isn't level 0, compute the number of tiles at level 0
        for (unsigned int i = 0; i < level; i++)
        {
            numTilesWide /= 2;
            numTilesHigh /= 2;
        }
    }
}

Profile
TileMap::createProfile() const
{
    static const Profile GLOBAL_GEODETIC("global-geodetic");
    static const Profile SPHERICAL_MERCATOR("spherical-mercator");

    Profile profile;
    const double eps = 1e-6;


    std::string def = srsString;
    if (!vsrsString.empty())
    {
        if (vsrsString == "egm96")
            def += "+5773";
    }
    SRS new_srs(def);

    if (profileType == ProfileType::GEODETIC)
    {
        profile = GLOBAL_GEODETIC;
    }
    else if (profileType == ProfileType::MERCATOR)
    {
        profile = SPHERICAL_MERCATOR;
    }
    else if (new_srs.horizontallyEquivalentTo(SRS::SPHERICAL_MERCATOR))
    {
        //HACK:  Some TMS sources, most notably TileCache, use a global mercator extent that is very slightly different than
        //       the automatically computed mercator bounds which can cause rendering issues due to the some texture coordinates
        //       crossing the dateline.  If the incoming bounds are nearly the same as our definion of global mercator, just use our definition.
        if (numTilesWide == 1 && numTilesHigh == 1 &&
            glm::epsilonEqual(SPHERICAL_MERCATOR.extent().xmin(), minX, eps) &&
            glm::epsilonEqual(SPHERICAL_MERCATOR.extent().ymin(), minY, eps) &&
            glm::epsilonEqual(SPHERICAL_MERCATOR.extent().xmax(), maxX, eps) &&
            glm::epsilonEqual(SPHERICAL_MERCATOR.extent().ymax(), maxY, eps))
        {
            profile = SPHERICAL_MERCATOR;
        }
    }

    else if (
        new_srs.isGeodetic() &&
        glm::epsilonEqual(minX, -180., eps) &&
        glm::epsilonEqual(maxX, 180., eps) &&
        glm::epsilonEqual(minY, -90., eps) &&
        glm::epsilonEqual(maxY, 90., eps))
    {
        profile = GLOBAL_GEODETIC;
    }

    if (!profile.valid())
    {
        // everything else is a "LOCAL" profile.
        profile = Profile(
            new_srs,
            Box(minX, minY, maxX, maxY),
            std::max(numTilesWide, 1u),
            std::max(numTilesHigh, 1u));
    }

    return profile;
}


std::string
TileMap::getURI(const TileKey& tilekey, bool invertY) const
{
    if (!intersectsKey(tilekey))
    {
        //OE_NOTICE << LC << "No key intersection for tile key " << tilekey.str() << std::endl;
        return {};
    }

    unsigned zoom = tilekey.level;
    unsigned x = tilekey.x;

    auto [numCols, numRows] = tilekey.profile.numTiles(tilekey.level);
    unsigned y = numRows - tilekey.y - 1;
    unsigned y_inverted = tilekey.y;

    //Some TMS like services swap the Y coordinate so 0,0 is the upper left rather than the lower left.  The normal TMS
    //specification has 0,0 at the bottom left, so inverting Y will make 0,0 in the upper left.
    //http://code.google.com/apis/maps/documentation/overlays.html#Google_Maps_Coordinates
    if (invertY)
    {
        std::swap(y, y_inverted);
    }

    std::string working = filename;

    // are we doing variable substitution?
    bool sub = working.find('{') != working.npos;

    // Select the correct TileSet
    if (tileSets.size() > 0)
    {
        for (auto& tileSet : tileSets)
        {
            if (tileSet.order == zoom)
            {
                if (!tileSet.href.empty())
                {
                    return tileSet.href + "/" + std::to_string(x) + "/" + std::to_string(y) + "." + format.extension;
                }
                else
                {
                    std::stringstream ss;
                    std::string path = std::filesystem::path(working).remove_filename().string();
                    if (sub)
                    {
                        auto temp = working;
                        util::replaceInPlace(temp, "${x}", std::to_string(x));
                        util::replaceInPlace(temp, "${y}", std::to_string(y));
                        util::replaceInPlace(temp, "${-y}", std::to_string(y_inverted));
                        util::replaceInPlace(temp, "${z}", std::to_string(zoom));
                        util::replaceInPlace(temp, "{x}", std::to_string(x));
                        util::replaceInPlace(temp, "{y}", std::to_string(y));
                        util::replaceInPlace(temp, "{-y}", std::to_string(y_inverted));
                        util::replaceInPlace(temp, "{z}", std::to_string(zoom));
                        return temp;
                    }
                    else
                    {
                        return path +
                            std::to_string(zoom) + '/' +
                            std::to_string(x) + '/' +
                            std::to_string(y) + "." +
                            format.extension;

                        return path;
                    }
                }
            }
        }
    }

    if (sub)
    {
        auto temp = working;
        util::replaceInPlace(temp, "${x}", std::to_string(x));
        util::replaceInPlace(temp, "${y}", std::to_string(y));
        util::replaceInPlace(temp, "${-y}", std::to_string(y_inverted));
        util::replaceInPlace(temp, "${z}", std::to_string(zoom));
        util::replaceInPlace(temp, "{x}", std::to_string(x));
        util::replaceInPlace(temp, "{y}", std::to_string(y));
        util::replaceInPlace(temp, "{-y}", std::to_string(y_inverted));
        util::replaceInPlace(temp, "{z}", std::to_string(zoom));
        return temp;
    }
   
    else
    {
        // Just go with it. No way of knowing the max level.
        return std::filesystem::path(working).remove_filename().string() +
            std::to_string(zoom) + '/' +
            std::to_string(x) + '/' +
            std::to_string(y) + "." +
            format.extension;
    }
}

bool
TileMap::intersectsKey(const TileKey& tilekey) const
{
    Box b = tilekey.extent().bounds();

    bool inter = intersects(
        minX, minY, maxX, maxY,
        b.xmin, b.ymin, b.xmax, b.ymax);

    if (!inter && tilekey.profile.srs().horizontallyEquivalentTo(SRS::SPHERICAL_MERCATOR))
    {
        glm::dvec3 keyMin(b.xmin, b.ymin, b.zmin);
        glm::dvec3 keyMax(b.xmax, b.ymax, b.zmax);

        auto xform = tilekey.profile.srs().to(tilekey.profile.srs().geodeticSRS());
        xform(keyMin, keyMin);
        xform(keyMax, keyMax);

        inter = intersects(
            minX, minY, maxX, maxY,
            b.xmin, b.ymin, b.xmax, b.ymax);
    }

    return inter;
}

void
TileMap::generateTileSets(unsigned numLevels)
{
    Profile p = createProfile();
    tileSets.clear();

    double width = (maxX - minX);

    for (unsigned int i = 0; i < numLevels; ++i)
    {
        auto [numCols, numRows] = p.numTiles(i);
        double res = (width / (double)numCols) / (double)format.width;

        TileSet ts{
            "",
            res,
            i
        };
        tileSets.emplace_back(TileSet{ "", res, i });
    }
}


TileMap::TileMap(
    const std::string& url,
    const Profile& profile,
    const DataExtentList& in_dataExtents,
    const std::string& formatString,
    int tile_width,
    int tile_height)
{
    const GeoExtent& ex = profile.extent();

    if (profile.valid())
    {
        profileType =
            profile.srs().isGeodetic() ? ProfileType::GEODETIC :
            profile.srs().horizontallyEquivalentTo(SRS::SPHERICAL_MERCATOR) ? ProfileType::MERCATOR :
            ProfileType::LOCAL;
    }

    minX = ex.xmin();
    minY = ex.ymin();
    maxX = ex.xmax();
    maxY = ex.ymax();

    originX = ex.xmin();
    originY = ex.ymin();

    filename = url;

    // Set up a rotating element in the template
    auto rotateStart = filename.find('[');
    auto rotateEnd = filename.find(']');
    if (rotateStart != std::string::npos && rotateEnd != std::string::npos && rotateEnd - rotateStart > 1)
    {
        rotateString = filename.substr(rotateStart, rotateEnd - rotateStart + 1);
    }

    srsString = getHorizSRSString(profile.srs());

    format.width = tile_width;
    format.height = tile_height;

    auto [x, y] = profile.numTiles(0);
    numTilesWide = x;
    numTilesHigh = y;

    // format can be a mime-type or an extension:
    std::string::size_type p = formatString.find('/');
    if (p == std::string::npos)
    {
        format.extension = formatString;
        format.mimeType = ""; // TODO
        //tileMap->_format.setMimeType( Registry::instance()->getMimeTypeForExtension(format) );
    }
    else
    {
        format.mimeType = formatString;
        format.extension = ""; // TODO;
        //tileMap->_format.setExtension( Registry::instance()->getExtensionForMimeType(format) );
    }

    //Add the data extents
    for (auto& temp : in_dataExtents)
        dataExtents.push_back(temp);

    // If we have some data extents specified then make a nicer bounds than the
    if (!dataExtents.empty())
    {
        // Get the union of all the extents
        GeoExtent e(dataExtents.front());
        for (unsigned int i = 1; i < dataExtents.size(); i++)
        {
            e.expandToInclude(dataExtents[i]);
        }

        // Convert the bounds to the output profile
        GeoExtent bounds = e.transform(profile.srs());
        minX = bounds.xmin();
        minY = bounds.ymin();
        maxX = bounds.xmax();
        maxY = bounds.ymax();
    }

    generateTileSets(20u);
    computeMinMaxLevel();
}


//----------------------------------------------------------------------------

Result<TileMap>
ROCKY_NAMESPACE::TMS::readTileMap(const URI& location, const IOOptions& io)
{
    auto r = location.read(io);

    if (r.failed())
        return r.error();

    auto tilemap = parseTileMapFromXML(r->content.data);

    if (tilemap.ok())
    {
        tilemap.value().filename = location.full();
        
        // remote locations should have a trailing slash
        if (location.isRemote() && tilemap.value().filename.back() != '/')
        {
            tilemap.value().filename += '/';
        }
    }

    return tilemap;
}



void
TMS::Driver::close()
{
    tileMap = {};
}

Result<>
TMS::Driver::open(const URI& uri, Profile& profile, const std::string& format, DataExtentList& dataExtents, const IOOptions& io)
{
    // URI is mandatory.
    if (uri.empty())
    {
        return Failure(Failure::ConfigurationError, "TMS driver requires a valid \"uri\" property");
    }

    // If the user supplied a profile, this means we are NOT querying a TMS manifest
    // and instead this is likely a normal XYZ data source. For these we want to
    // invert the Y axis by default.
    if (profile.valid())
    {
        DataExtentList dataExtents_dummy; // empty.

        tileMap = TileMap(uri.full(), profile, dataExtents_dummy, format, 256, 256);

        // Non-TMS "XYZ" data sources usually have an inverted Y component:
        tileMap.invertYaxis = true;
    }

    else
    {
        // Attempt to read the tile map parameters from a TMS TileMap manifest:
        auto tileMapRead = readTileMap(uri, io);

        if (tileMapRead.failed())
            return tileMapRead.error();

        tileMap = tileMapRead.value();

        Profile profileFromTileMap = tileMap.createProfile();
        if (profileFromTileMap.valid())
        {
            profile = profileFromTileMap;
        }

        tileMapExtent = GeoExtent(profile.extent());
    }

    // Make sure we've established a profile by this point:
    if (!profile.valid())
    {
        return Failure("Failed to establish a profile for " + uri.full());
    }

    // TileMap and profile are valid at this point. Build the tile sets.
    // Automatically set the min and max level of the TileMap
    if (!tileMap.tileSets.empty())
    {
        if (!tileMap.dataExtents.empty())
        {
            for (auto& de : tileMap.dataExtents)
            {
                dataExtents.push_back(de);
            }
        }
    }

    if (dataExtents.empty() && profile.valid())
    {
        dataExtents.push_back(DataExtent(profile.extent(), 0, tileMap.maxLevel));
    }

    return ResultVoidOK;
}

Result<std::shared_ptr<Image>>
TMS::Driver::read(const TileKey& key, bool invertY, bool isMapboxRGB, const URI::Context& context, const IOOptions& io) const
{
    std::shared_ptr<Image> image;
    URI imageURI;

    // create the URI from the tile map?
    if (tileMap.valid() && key.level <= tileMap.maxLevel)
    {
        bool y_inverted = tileMap.invertYaxis;
        if (invertY) y_inverted = !y_inverted;

        imageURI = URI(tileMap.getURI(key, y_inverted), context);

        if (!imageURI.empty() && isMapboxRGB)
        {
            if (imageURI.full().find('?') == std::string::npos)
                imageURI = URI(imageURI.full() + "?mapbox=true", context);
            else
                imageURI = URI(imageURI.full() + "&mapbox=true", context);
        }

        auto fetch = imageURI.read(io);
        if (fetch.failed())
        {
            return fetch.error();
        }

        std::istringstream stream(fetch->content.data);
        auto image_rr = io.services().readImageFromStream(stream, fetch->content.type, io);

        if (image_rr.failed())
        {
            return image_rr.error();
        }

        image = image_rr.value();

        if (!image)
        {
            if (imageURI.empty() || !tileMap.intersectsKey(key))
            {
                // We couldn't read the image from the URL or the cache, so check to see
                // whether the given key is less than the max level of the tilemap
                // and create a transparent image.
                if (key.level <= tileMap.maxLevel)
                {
                    ROCKY_TODO("");
                    image = Image::create(Image::R8G8B8A8_UNORM, 1, 1);
                }
            }
        }
    }

    if (image)
        return image;
    else
        return Failure_ResourceUnavailable;
}
