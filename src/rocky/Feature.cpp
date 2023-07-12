/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Feature.h"
#include <gdal.h> // OGR API
#include <ogr_spatialref.h>

using namespace ROCKY_NAMESPACE;

Geometry::Geometry(Type in_type) :
    type(in_type)
{
    //nop
}

Geometry::const_iterator::const_iterator(const Geometry& geom)
{
    _stack.push(&geom);
    fetch();
}

bool
Geometry::const_iterator::hasMore() const
{
    return _next != nullptr;
}

const Geometry&
Geometry::const_iterator::next()
{
    auto n = _next;
    fetch();
    return *n;
}

void
Geometry::const_iterator::fetch()
{
    _next = nullptr;

    if (_stack.size() == 0)
        return;

    const Geometry* current = _stack.top();
    _stack.pop();

    bool is_multi =
        current->type == Geometry::Type::MultiLineString ||
        current->type == Geometry::Type::MultiPoints ||
        current->type == Geometry::Type::MultiPolygon;

    if (is_multi && _traverse_multi)
    {
        for (auto& part : current->parts)
            _stack.push(&part);
        fetch();
    }
    else
    {
        _next = current;
        
        if (current->type == Type::Polygon && _traverse_polygon_holes)
        {
            for (auto& ring : current->parts)
                _stack.push(&ring);
        }
    }
}

bool
Feature::FieldNameComparator::operator()(const std::string& L, const std::string& R) const
{
    for (int i = 0; i < L.length() && i < R.length(); ++i)
    {
        char Lc = std::tolower(L[i]), Rc = std::tolower(R[i]);
        if (Lc < Rc) return true;
        if (Lc > Rc) return false;
    }
    return L.length() < R.length();
}


#ifdef GDAL_FOUND

namespace
{
    void* open_OGR_layer(void* ds, const std::string& layerName)
    {
        // Open a specific layer within the data source, if applicable:
        auto handle = GDALDatasetGetLayerByName(ds, layerName.c_str());
        if (!handle)
        {
            unsigned index = layerName.empty() ? 0 : std::stoi(layerName);
            handle = GDALDatasetGetLayer(ds, index);
        }
        return handle;
    }

    void populate(OGRGeometryH handle, Geometry& out_geom, int numPoints)
    {
        out_geom.points.reserve(numPoints);

        for (int i = 0; i < numPoints; ++i)
        {
            double x = 0, y = 0, z = 0;
            OGR_G_GetPoint(handle, i, &x, &y, &z);
            glm::dvec3 p(x, y, z);
            if (out_geom.points.size() == 0 || p != out_geom.points.back())
                out_geom.points.push_back(p);
        }
    }

    void create_polygon(OGRGeometryH geomHandle, Geometry& out_geom)
    {
        int numParts = OGR_G_GetGeometryCount(geomHandle);
        if (numParts == 0)
        {
            int numPoints = OGR_G_GetPointCount(geomHandle);
            out_geom.type = Geometry::Type::Polygon;
            populate(geomHandle, out_geom, numPoints);

#if 0
            if (rewindPolygons)
            {
                output->open();
                output->rewind(Ring::ORIENTATION_CCW);
            }
#endif
        }
        else if (numParts > 0)
        {
            for (int p = 0; p < numParts; p++)
            {
                OGRGeometryH partRef = OGR_G_GetGeometryRef(geomHandle, p);
                int numPoints = OGR_G_GetPointCount(partRef);

                if (p == 0)
                {
                    out_geom.type = Geometry::Type::Polygon;
                    populate(partRef, out_geom, numPoints);
#if 0
                    if (rewindPolygons)
                    {
                        output->open();
                        output->rewind(Ring::ORIENTATION_CCW);
                    }
#endif
                }
                else
                {
                    out_geom.parts.push_back({});
                    Geometry& hole = out_geom.parts.back();
                    populate(partRef, hole, numPoints);
#if 0
                    if (rewindPolygons)
                    {
                        hole->open();
                        hole->rewind(Ring::ORIENTATION_CW);
                    }
                    output->getHoles().push_back(hole);
#endif
                }
            }
        }
    }

    void create_geometry(OGRGeometryH geomHandle, Geometry& out_geom)
    {
        OGRwkbGeometryType wkbType = OGR_G_GetGeometryType(geomHandle);

        int numPoints, numGeoms;

        switch (wkbType)
        {
        case wkbPolygon:
        case wkbPolygon25D:
#ifdef GDAL_HAS_M_TYPES
        case wkbPolygonM:
        case wkbPolygonZM:
#endif
            create_polygon(geomHandle, out_geom);
            break;

        case wkbLineString:
        case wkbLineString25D:
#ifdef GDAL_HAS_M_TYPES
        case wkbLineStringM:
        case wkbLineStringZM:
#endif
            numPoints = OGR_G_GetPointCount(geomHandle);
            out_geom.type = Geometry::Type::LineString;
            populate(geomHandle, out_geom, numPoints);
            break;

        case wkbLinearRing:
            numPoints = OGR_G_GetPointCount(geomHandle);
            out_geom.type = Geometry::Type::LineString;
            //output = new Ring(numPoints);
            populate(geomHandle, out_geom, numPoints);
            // ringify:
            if (out_geom.points.size() >= 3 && out_geom.points.front() != out_geom.points.back())
                out_geom.points.push_back(out_geom.points.front());
            break;

        case wkbPoint:
        case wkbPoint25D:
#ifdef GDAL_HAS_M_TYPES
        case wkbPointM:
        case wkbPointZM:
#endif
            numPoints = OGR_G_GetPointCount(geomHandle);
            out_geom.type = Geometry::Type::Points;
            //output = new Point(numPoints);
            populate(geomHandle, out_geom, numPoints);
            break;

        case wkbMultiPoint:
        case wkbMultiPoint25D:
#ifdef GDAL_HAS_M_TYPES
        case wkbMultiPointM:
        case wkbMultiPointZM:
#endif
            numGeoms = OGR_G_GetGeometryCount(geomHandle);
            out_geom.type = Geometry::Type::Points;
            for (int n = 0; n < numGeoms; n++)
            {
                OGRGeometryH subGeomRef = OGR_G_GetGeometryRef(geomHandle, n);
                if (subGeomRef)
                {
                    numPoints = OGR_G_GetPointCount(subGeomRef);
                    populate(subGeomRef, out_geom, numPoints);
                }
            }
            break;

#ifdef GDAL_HAS_M_TYPES
        case wkbTINZ:
        case wkbTIN:
        case wkbTINM:
        case wkbTINZM:
            output = createTIN(geomHandle);
            break;
#endif

        case wkbGeometryCollection:
        case wkbGeometryCollection25D:
        case wkbMultiLineString:
        case wkbMultiLineString25D:
        case wkbMultiPolygon:
        case wkbMultiPolygon25D:
#ifdef GDAL_HAS_M_TYPES
        case wkbGeometryCollectionM:
        case wkbGeometryCollectionZM:
        case wkbMultiLineStringM:
        case wkbMultiLineStringZM:
        case wkbMultiPolygonM:
        case wkbMultiPolygonZM:
#endif

            numGeoms = OGR_G_GetGeometryCount(geomHandle);
            for (int n = 0; n < numGeoms; n++)
            {
                OGRGeometryH subGeomRef = OGR_G_GetGeometryRef(geomHandle, n);
                if (subGeomRef)
                {
                    out_geom.parts.push_back(Geometry());
                    Geometry& subgeom = out_geom.parts.back();
                    create_geometry(subGeomRef, subgeom);
                    if (subgeom.points.empty())
                        out_geom.parts.resize(out_geom.parts.size() - 1);
                    //Geometry* geom = createGeometry(subGeomRef, rewindPolygons);
                    //if (geom) multi->getComponents().push_back(geom);
                }
            }

            if (out_geom.parts.size() > 0)
            {
                if (out_geom.parts[0].type == Geometry::Type::Points)
                    out_geom.type = Geometry::Type::MultiPoints;
                else if (out_geom.parts[0].type == Geometry::Type::LineString)
                    out_geom.type = Geometry::Type::MultiLineString;
                else if (out_geom.parts[0].type == Geometry::Type::Polygon)
                    out_geom.type = Geometry::Type::Polygon;
            }
            break;
        }
    }

    void create_feature_from_OGR_handle(void* handle, const SRS& srs, Feature& out_feature)
    {
        Feature::ID fid = OGR_F_GetFID(handle);
        OGRGeometryH geom_handle = OGR_F_GetGeometryRef(handle);

        out_feature.srs = srs;

        if (geom_handle)
        {
            create_geometry(geom_handle, out_feature.geometry);
        }

        int numAttrs = OGR_F_GetFieldCount(handle);

        for (int i = 0; i < numAttrs; ++i)
        {
            OGRFieldDefnH field_handle_ref = OGR_F_GetFieldDefnRef(handle, i);

            // get the field name and convert to lower case:
            const char* field_name = OGR_Fld_GetNameRef(field_handle_ref);
            std::string name = util::toLower(std::string(field_name));

            // get the field type and set the value appropriately
            OGRFieldType field_type = OGR_Fld_GetType(field_handle_ref);
            switch (field_type)
            {
            case OFTInteger:
            {
                if (OGR_F_IsFieldSetAndNotNull(handle, i))
                {
                    auto value = OGR_F_GetFieldAsInteger(handle, i);

                    out_feature.fields.emplace(name, Feature::FieldValueUnion{
                        std::to_string(value),
                        (double)value,
                        value,
                        (value != 0) });
                }
                else
                {
                    //feature->setNull(name, ATTRTYPE_INT);
                }
            }
            break;

            case OFTInteger64:
            {
                if (OGR_F_IsFieldSetAndNotNull(handle, i))
                {
                    auto value = OGR_F_GetFieldAsInteger64(handle, i);

                    out_feature.fields.emplace(name, Feature::FieldValueUnion{
                        std::to_string(value),
                        (double)value,
                        value,
                        (value != 0) });
                }
                else
                {
                    //feature->setNull(name, ATTRTYPE_INT);
                }
            }
            break;

            case OFTReal:
            {
                if (OGR_F_IsFieldSetAndNotNull(handle, i))
                {
                    double value = OGR_F_GetFieldAsDouble(handle, i);

                    out_feature.fields.emplace(name, Feature::FieldValueUnion{
                        std::to_string(value),
                        value,
                        (long long)value,
                        (value != 0.0) });
                }
                else
                {
                    //feature->setNull(name, ATTRTYPE_DOUBLE);
                }
            }
            break;

            default:
            {
                if (OGR_F_IsFieldSetAndNotNull(handle, i))
                {
                    const char* value = OGR_F_GetFieldAsString(handle, i);
                    if (value)
                    {
                        std::string svalue(value);
                        out_feature.fields.emplace(name, Feature::FieldValueUnion{
                            svalue,
                            0.0,
                            0,
                            util::toLower(svalue) == "true" });
                    }
                    else
                    {
                        //feature->setNull(name, ATTRTYPE_STRING);
                    }
                }
            }
            }
        }
    }
}

OGRFeatureSource::~OGRFeatureSource()
{
    close();
}

std::shared_ptr<FeatureSource::iterator>
OGRFeatureSource::iterate(IOOptions& io)
{
    OGRDataSourceH dsHandle = nullptr;
    OGRLayerH layerHandle = nullptr;

    const char* openOptions[2] = {
        "OGR_GPKG_INTEGRITY_CHECK=NO",
        nullptr
    };

    dsHandle = GDALOpenEx(
        _source.c_str(),
        GDAL_OF_VECTOR | GDAL_OF_READONLY,
        nullptr,
        nullptr, //openOptions,
        nullptr);

    // open the handles safely:
    // Each cursor requires its own DS handle so that multi-threaded access will work.
    // The cursor impl will dispose of the new DS handle.
    if (dsHandle)
    {
        layerHandle = open_OGR_layer(dsHandle, layerName);
    }

    if (dsHandle && layerHandle)
    {
        auto i = std::make_shared<iterator>();
        i->_source = this;
        i->_dsHandle = dsHandle;
        i->_layerHandle = layerHandle;
        i->init();

#if 0
        Query newQuery(query);
        if (options().query().isSet())
        {
            newQuery = options().query()->combineWith(query);
        }

        OE_DEBUG << newQuery.getConfig().toJSON(true) << std::endl;
#endif

        return i;
    }
    else
    {
        if (dsHandle)
        {
            OGRReleaseDataSource(dsHandle);
        }
    }
    return { };

}


OGRFeatureSource::iterator::iterator()
{
}

void
OGRFeatureSource::iterator::init()
{
    std::string expr;
    std::string from = OGR_FD_GetName(OGR_L_GetLayerDefn(_layerHandle));
    std::string driverName = OGR_Dr_GetName(OGR_DS_GetDriver(_dsHandle));

    // Quote the layer name if it is a shapefile, so we can handle any weird filenames like those with spaces or hyphens.
    // Or quote any layers containing spaces for PostgreSQL
    if (driverName == "ESRI Shapefile" || driverName == "VRT" ||
        from.find(' ') != std::string::npos)
    {
        std::string delim = "\"";
        from = delim + from + delim;
    }

    std::stringstream buf;
    buf << "SELECT * FROM " << from;
    expr = buf.str();

#if 0
    if (_query.expression().isSet())
    {
        // build the SQL: allow the Query to include either a full SQL statement or
        // just the WHERE clause.
        expr = _query.expression().value();

        // if the expression is just a where clause, expand it into a complete SQL expression.
        std::string temp = osgEarth::toLower(expr);

        if (temp.find("select") != 0)
        {
            std::stringstream buf;
            buf << "SELECT * FROM " << from << " WHERE " << expr;
            std::string bufStr;
            bufStr = buf.str();
            expr = bufStr;
        }
    }
    else
    {
        std::stringstream buf;
        buf << "SELECT * FROM " << from;
        expr = buf.str();
    }

    //Include the order by clause if it's set
    if (_query.orderby().isSet())
    {
        std::string orderby = _query.orderby().value();

        std::string temp = osgEarth::toLower(orderby);

        if (temp.find("order by") != 0)
        {
            std::stringstream buf;
            buf << "ORDER BY " << orderby;
            std::string bufStr;
            bufStr = buf.str();
            orderby = buf.str();
        }
        expr += (" " + orderby);
    }

    // if the tilekey is set, convert it to feature profile coords
    if (_query.tileKey().isSet() && !_query.bounds().isSet() && profile)
    {
        GeoExtent localEx = _query.tileKey()->getExtent().transform(profile->getSRS());
        _query.bounds() = localEx.bounds();
    }

    // if there's a spatial extent in the query, build the spatial filter:
    if (_query.bounds().isSet())
    {
        OGRGeometryH ring = OGR_G_CreateGeometry(wkbLinearRing);
        OGR_G_AddPoint(ring, _query.bounds()->xMin(), _query.bounds()->yMin(), 0);
        OGR_G_AddPoint(ring, _query.bounds()->xMin(), _query.bounds()->yMax(), 0);
        OGR_G_AddPoint(ring, _query.bounds()->xMax(), _query.bounds()->yMax(), 0);
        OGR_G_AddPoint(ring, _query.bounds()->xMax(), _query.bounds()->yMin(), 0);
        OGR_G_AddPoint(ring, _query.bounds()->xMin(), _query.bounds()->yMin(), 0);

        _spatialFilter = OGR_G_CreateGeometry(wkbPolygon);
        OGR_G_AddGeometryDirectly(_spatialFilter, ring);
        // note: "Directly" above means _spatialFilter takes ownership if ring handle
    }
#endif

    _resultSetHandle = GDALDatasetExecuteSQL(_dsHandle, expr.c_str(), _spatialFilterHandle, 0L);

    if (_resultSetHandle)
        OGR_L_ResetReading(_resultSetHandle);

    readChunk();
}

OGRFeatureSource::iterator::~iterator()
{

    if (_nextHandleToQueue)
        OGR_F_Destroy(_nextHandleToQueue);

#if 0
    if (_dsHandle && _resultSetHandle != _layerHandle)
        OGR_DS_ReleaseResultSet(_dsHandle, _resultSetHandle);

    if (_spatialFilter)
        OGR_G_DestroyGeometry(_spatialFilter);

    if (_dsHandle)
        OGRReleaseDataSource(_dsHandle);
#endif
}

void
OGRFeatureSource::iterator::readChunk()
{
    if (!_resultSetHandle)
        return;

    while (_queue.size() < _chunkSize && !_resultSetEndReached)
    {
        OGRFeatureH handle = OGR_L_GetNextFeature(_resultSetHandle);
        if (handle)
        {
            Feature feature;
            const SRS& srs = _source->_featureProfile.extent.srs();
            create_feature_from_OGR_handle(handle, srs, feature);

            if (feature.valid())
            {
                _queue.push(std::move(feature));
            }

            OGR_F_Destroy(handle);
        }
        else
        {
            _resultSetEndReached = true;
        }
    }

#if 0
                    if (_source == nullptr || !_source->isBlacklisted(feature->getFID()))
                    {
                        if (validateGeometry(feature->getGeometry()))
                        {
                            filterList.push_back(feature.release());
                        }
                        else
                        {
                            OE_DEBUG << LC << "Invalid geometry found at feature " << feature->getFID() << std::endl;
                        }
                    }
                    else
                    {
                        OE_DEBUG << LC << "Blacklisted feature " << feature->getFID() << " skipped" << std::endl;
                    }
                }
                else
                {
                    Log::info() << "OGR feature iterator: Skipping invalid feature" << std::endl;
                }

                OGR_F_Destroy(handle);
            }
            else
            {
                _resultSetEndReached = true;
            }
        }
#endif

#if 0
        // preprocess the features using the filter list:
        if (_filters.valid() && !_filters->empty())
        {
            FilterContext cx;
            cx.setProfile(_profile.get());
            if (_query.bounds().isSet())
            {
                cx.extent() = GeoExtent(_profile->getSRS(), _query.bounds().get());
            }
            else
            {
                cx.extent() = _profile->getExtent();
            }

            for (FeatureFilterChain::const_iterator i = _filters->begin(); i != _filters->end(); ++i)
            {
                FeatureFilter* filter = i->get();
                cx = filter->push(filterList, cx);
            }
        }

        for (FeatureList::const_iterator i = filterList.begin(); i != filterList.end(); ++i)
        {
            _queue.push(i->get());
        }
    }

    if (_chunkSize == ~0)
    {
        OGR_L_ResetReading(_resultSetHandle);
    }
#endif
}

bool
OGRFeatureSource::iterator::hasMore() const
{
    return _resultSetHandle && !_queue.empty();
};

const Feature&
OGRFeatureSource::iterator::next()
{
    if (!hasMore())
        return _lastFeatureReturned;

    if (_queue.size() == 1u)
        readChunk();

    // do this in order to hold a reference to the feature we return, so the caller
    // doesn't have to. This lets us avoid requiring the caller to use a ref_ptr when 
    // simply iterating over the cursor, making the cursor move conventient to use.
    _lastFeatureReturned = _queue.front();
    _queue.pop();

    return _lastFeatureReturned;
}




Status
OGRFeatureSource::open()
{
    //Status parent = FeatureSource::openImplementation();
    //if (parent.isError())
    //    return parent;

    // Data source at a URL?
    if (uri.has_value())
    {
        _source = uri->full();

        // ..inside a zip file?
        if (util::endsWith(_source, ".zip", false) || _source.find(".zip/") != std::string::npos)
        {
            _source = util::make_string() << "/vsizip/" << _source;
        }
    }

#if 0
    else if (!_geometry.valid())
    {
        // ..or inline geometry?
        _geometry =
            options().geometryConfig().isSet() ? parseGeometry(*options().geometryConfig()) :
            options().geometryUrl().isSet() ? parseGeometryUrl(*options().geometryUrl(), getReadOptions()) :
            0L;
    }
#endif

    // If nothing was set, we're done
    if (_source.empty()) // && !_geometry.valid())
    {
        return Status(Status::ConfigurationError, "No URL, connection, or inline geometry provided");
    }

    // Try to open the datasource and establish a feature profile.        
    //FeatureProfile featureProfile;

#if 0
    // see if we have a custom profile.
    if (options().profile().isSet() && !_profile.valid())
    {
        _profile = Profile::create(*options().profile());
    }

    if (_geometry.valid())
    {
        // if the user specified explicit geometry, use that and the calculated
        // extent of the geometry to derive a profile.
        GeoExtent ex;
        if (_profile.valid())
        {
            ex = GeoExtent(_profile->getSRS(), _geometry->getBounds());
        }

        if (!ex.valid())
        {
            // default to WGS84 Lat/Long
            ex = GeoExtent(SRS::WGS84, _geometry->getBounds());
        }

        featureProfile = FeatureProfile{ ex };
    }

    else if (!source.empty())
#endif
    {
        // otherwise, assume we're loading from the URL/connection:

        // remember the thread so we don't use the handles illegaly.
        _dsHandleThreadId = std::thread::id();

        // If the user request a particular driver, set that up now:
        std::string driverName;
        if (ogrDriver.has_value())
            driverName = ogrDriver.value();

        //if (driverName.empty())
        //    driverName = "ESRI Shapefile";

        const char* driverList[2] = {
            driverName.c_str(),
            nullptr
        };

        // always opening a vector source:
        int openFlags = GDAL_OF_VECTOR;

        // whether it's read-only or writable:
#if 0
        if (options().openWrite().isSetTo(true))
            openFlags |= GDAL_OF_UPDATE;
        else
#endif
            openFlags |= GDAL_OF_READONLY;

        if (Log::level >= LogLevel::INFO)
            openFlags |= GDAL_OF_VERBOSE_ERROR;

        // this handle may ONLY be used from this thread!
        // https://github.com/OSGeo/gdal/blob/v2.4.1/gdal/gcore/gdaldataset.cpp#L2577
        _dsHandle = GDALOpenEx(
            _source.c_str(),
            openFlags,
            driverName.empty() ? nullptr : driverList,
            nullptr,
            nullptr);

        if (!_dsHandle)
        {
            return Status(Status::ResourceUnavailable, util::make_string() << "Failed to open \"" << _source << "\"");
        }

        if (openFlags & GDAL_OF_UPDATE)
        {
            writable = true;
        }

        // Open a specific layer within the data source, if applicable:
        _layerHandle = open_OGR_layer(_dsHandle, layerName);

        if (!_layerHandle)
        {
            return Status(Status::ResourceUnavailable, util::make_string()
                << "Failed to open layer \"" << layerName << "\" from \"" << _source << "\"");
        }

#if 0
        // if the user provided a profile, use that:
        if (profile.valid())
        {
            featureProfile.extent = profile.extent();
        }

        // otherwise extract one from the layer:
        else
#endif
        {
            // extract the SRS and Extent:
            OGRSpatialReferenceH srHandle = OGR_L_GetSpatialRef(_layerHandle);
            if (!srHandle)
            {
                return Status(Status::ResourceUnavailable, util::make_string()
                    << "No spatial reference found in \"" << _source << "\"");
            }

            SRS srs;
            char* wktbuf;
            if (OSRExportToWkt(srHandle, &wktbuf) == OGRERR_NONE)
            {
                srs = SRS(wktbuf);
                CPLFree(wktbuf);
                if (!srs.valid())
                {
                    return Status(Status::ResourceUnavailable, util::make_string()
                        << "Unrecognized SRS found in \"" << _source << "\"");
                }
            }

            // extract the full extent of the layer:
            OGREnvelope env;
            if (OGR_L_GetExtent(_layerHandle, &env, 1) != OGRERR_NONE)
            {
                return Status(Status::ResourceUnavailable, util::make_string()
                    << "Invalid extent returned from \"" << _source << "\"");
            }

            GeoExtent extent(srs, env.MinX, env.MinY, env.MaxX, env.MaxY);
            if (!extent.valid())
            {
                return Status(Status::ResourceUnavailable, util::make_string()
                    << "Invalid extent returned from \"" << _source << "\"");
            }

            // Made it!
            _featureProfile.extent = extent;
        }

        // Get the feature count
        unsigned featureCount = OGR_L_GetFeatureCount(_layerHandle, 1);

#if 0
        // establish the feature schema:
        initSchema();
#endif

#if 0
        // establish the geometry type for this feature layer:
        OGRwkbGeometryType wkbType = OGR_FD_GetGeomType(OGR_L_GetLayerDefn(_layerHandle));
        if (
            wkbType == wkbPolygon ||
            wkbType == wkbPolygon25D)
        {
            _geometryType = Geometry::Type::Polygon;
        }
        else if (
            wkbType == wkbLineString ||
            wkbType == wkbLineString25D)
        {
            _geometryType = Geometry::Type::LineString;
        }
        else if (
            wkbType == wkbLinearRing)
        {
            _geometryType = Geometry::Type::LineString;
        }
        else if (
            wkbType == wkbPoint ||
            wkbType == wkbPoint25D)
        {
            _geometryType = Geometry::Type::Points;
        }
        else if (
            wkbType == wkbMultiPoint ||
            wkbType == wkbMultiPoint25D)
        {
            _geometryType = Geometry::Type::MultiPoints;
        }
        else if (
            //wkbType == wkbGeometryCollection ||
            //wkbType == wkbGeometryCollection25D ||
            wkbType == wkbMultiLineString ||
            wkbType == wkbMultiLineString25D)
        {
            _geometryType = Geometry::Type::MultiLineString;
        }
        else if (
            wkbType == wkbMultiPolygon ||
            wkbType == wkbMultiPolygon25D)
        {
            _geometryType = Geometry::Type::MultiPolygon;
        }
#endif
    }

#if 0
    // finally, if we established a profile, set it for this source.
    if (_featureProfile)
    {
        if (options().geoInterp().isSet())
        {
            featureProfile->geoInterp() = options().geoInterp().get();
        }
        setFeatureProfile(featureProfile);
    }

    else
    {
        return Status(Status::ResourceUnavailable, "Failed to establish a valid feature profile");
    }
#endif

    Log::info() << "OGR feature source " << _source << " opened OK" << std::endl;

    return { };
}

void
OGRFeatureSource::close()
{
    if (_layerHandle)
    {
#if 0
        if (_needsSync)
        {
            OGR_L_SyncToDisk(_layerHandle); // for writing only
            const char* name = OGR_FD_GetName(OGR_L_GetLayerDefn(_layerHandle));
            std::stringstream buf;
            buf << "REPACK " << name;
            std::string bufStr;
            bufStr = buf.str();
            OE_DEBUG << LC << "SQL: " << bufStr << std::endl;
            OGR_DS_ExecuteSQL(_dsHandle, bufStr.c_str(), 0L, 0L);
        }
#endif
        _layerHandle = nullptr;
    }

    if (_dsHandle)
    {
        OGRReleaseDataSource(_dsHandle);
        _dsHandle = nullptr;
    }

    //init();
    //return FeatureSource::closeImplementation();
}

#endif // GDAL_FOUND
