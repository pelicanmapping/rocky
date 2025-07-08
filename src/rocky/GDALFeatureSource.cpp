/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GDALFeatureSource.h"

#ifdef ROCKY_HAS_GDAL

#include <gdal.h> // OGR API
#include <ogr_spatialref.h>
#include <cassert>

using namespace ROCKY_NAMESPACE;

namespace
{
    void* open_OGR_layer(void* ds, const std::string& layerName)
    {
        // Open a specific layer within the data source, if applicable:
        auto handle = GDALDatasetGetLayerByName(ds, layerName.c_str());
        if (!handle)
        {
            unsigned index = layerName.empty() ? 0 : std::atoi(layerName.c_str());
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
        case wkbLineStringM:
        case wkbLineStringZM:
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
                }
            }

            if (out_geom.parts.size() > 0)
            {
                if (out_geom.parts[0].type == Geometry::Type::Points)
                {
                    out_geom.type = Geometry::Type::MultiPoints;
                }
                else if (out_geom.parts[0].type == Geometry::Type::LineString)
                {
                    out_geom.type = Geometry::Type::MultiLineString;
                }
                else if (out_geom.parts[0].type == Geometry::Type::Polygon)
                {
                    if (out_geom.points.empty())
                    {
                        out_geom.type = Geometry::Type::MultiPolygon;
                    }
                    else
                    {
                        out_geom.type = Geometry::Type::Polygon;
                    }
                }
            }
            break;
        }
    }

    void create_feature_from_OGR_handle(void* handle, const SRS& srs, Feature& out_feature)
    {
        out_feature.id = OGR_F_GetFID(handle);
        OGRGeometryH geom_handle = OGR_F_GetGeometryRef(handle);

        out_feature.srs = srs;

        if (geom_handle)
        {
            create_geometry(geom_handle, out_feature.geometry);
            out_feature.dirtyExtent();
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
                    out_feature.fields[name].emplace<long long>(value);
                }
            }
            break;

            case OFTInteger64:
            {
                if (OGR_F_IsFieldSetAndNotNull(handle, i))
                {
                    auto value = OGR_F_GetFieldAsInteger64(handle, i);
                    out_feature.fields[name].emplace<long long>(value);
                }
            }
            break;

            case OFTReal:
            {
                if (OGR_F_IsFieldSetAndNotNull(handle, i))
                {
                    double value = OGR_F_GetFieldAsDouble(handle, i);
                    out_feature.fields[name].emplace<double>(value);
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
                        out_feature.fields[name].emplace<std::string>(svalue);
                    }
                }
            }
            }
        }
    }
}

GDALFeatureSource::~GDALFeatureSource()
{
    close();
}

FeatureSource::iterator
GDALFeatureSource::iterate(const IOOptions& io)
{
    OGRDataSourceH dsHandle = nullptr;
    OGRLayerH layerHandle = externalLayerHandle;

    if (!layerHandle)
    {
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
    }

    auto i = new iterator_impl();

    if (layerHandle)
    {
        i->_source = this;
        i->_dsHandle = dsHandle;
        i->_layerHandle = layerHandle;
        i->init();
    }
    else
    {
        if (dsHandle)
        {
            OGRReleaseDataSource(dsHandle);
        }
    }

    return iterator(i);
}


void
GDALFeatureSource::iterator_impl::init()
{
    _resultSetEndReached = false;

    if (_dsHandle)
    {
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

        std::string expr = "SELECT * FROM " + from;
        _resultSetHandle = GDALDatasetExecuteSQL(_dsHandle, expr.c_str(), _spatialFilterHandle, nullptr);
    }
    else
    {
        // pre-existing layer with no dataset; iterate it directly.
        _resultSetHandle = _layerHandle;
    }

    if (_resultSetHandle)
        OGR_L_ResetReading(_resultSetHandle);

    readChunk();
}

GDALFeatureSource::iterator_impl::~iterator_impl()
{
    if (_nextHandleToQueue)
    {
        OGR_F_Destroy(_nextHandleToQueue);
    }
}

void
GDALFeatureSource::iterator_impl::readChunk()
{
    if (!_resultSetHandle)
        return;

    const SRS& srs = _source->_metadata.extent.valid() ?
        _source->_metadata.extent.srs() :
        _source->externalSRS;

    while (_queue.size() < _chunkSize && !_resultSetEndReached)
    {
        auto handle = OGR_L_GetNextFeature(_resultSetHandle);
        if (handle)
        {
            Feature feature;

            create_feature_from_OGR_handle(handle, srs, feature);

            if (feature.valid())
            {
                if (feature.id == OGRNullFID)
                {
                    feature.id = _idGenerator++;
                }

                _queue.push(std::move(feature));
            }

            OGR_F_Destroy(handle);
        }
        else
        {
            _resultSetEndReached = true;
        }
    }

    if (_chunkSize == ~0)
    {
        OGR_L_ResetReading(_resultSetHandle);
    }
}

bool
GDALFeatureSource::iterator_impl::hasMore() const
{
    return _resultSetHandle && !_queue.empty();
};

Feature
GDALFeatureSource::iterator_impl::next()
{
    assert(hasMore());

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
GDALFeatureSource::open()
{
    // Pre-existing layer handle?
    if (externalLayerHandle)
    {
        _layerHandle = externalLayerHandle;
    }

    // Data source at a URL?
    else
    {
        if (uri.has_value())
        {
            _source = uri->full();

            // ..inside a zip file?
            if (util::endsWith(_source, ".zip", false) || _source.find(".zip/") != std::string::npos)
            {
                _source = util::make_string() << "/vsizip/" << _source;
            }
        }

        // If nothing was set, we're done
        if (_source.empty())
        {
            return Status(Status::ConfigurationError, "No URL, connection, or inline geometry provided");
        }

        // assume we're loading from the URL/connection:
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
        int openFlags = GDAL_OF_VECTOR | GDAL_OF_READONLY;

        if (Log()->level() >= log::level::info)
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

        // Open a specific layer within the data source, if applicable:
        _layerHandle = open_OGR_layer(_dsHandle, layerName);

        if (!_layerHandle)
        {
            return Status(Status::ResourceUnavailable, util::make_string()
                << "Failed to open layer \"" << layerName << "\" from \"" << _source << "\"");
        }

        _featureCount = (int)OGR_L_GetFeatureCount(_layerHandle, 1);

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
        _metadata.extent = extent;
    }

    Log()->debug("GDAL features " + _source + " opened OK");

    return { };
}

int
GDALFeatureSource::featureCount() const
{
    return _featureCount;
}

void
GDALFeatureSource::close()
{
    if (_layerHandle)
    {
        _layerHandle = nullptr;
    }

    if (_dsHandle)
    {
        OGRReleaseDataSource(_dsHandle);
        _dsHandle = nullptr;
    }
}

#endif // ROCKY_HAS_GDAL
