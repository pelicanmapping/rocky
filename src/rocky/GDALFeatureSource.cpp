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
    OGRLayerH open_OGR_layer(void* ds, const std::string& layerName)
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


    void create_feature_from_OGR_handle(void* vhandle, const SRS& srs, const std::vector<std::string>& fieldNames, Feature& out_feature)
    {
        OGRFeatureH handle = (OGRFeatureH)vhandle;
        out_feature.id = OGR_F_GetFID(handle);
        OGRGeometryH geom_handle = OGR_F_GetGeometryRef(handle);

        out_feature.srs = srs;

        if (geom_handle)
        {
            create_geometry(geom_handle, out_feature.geometry);
            out_feature.dirtyExtent();
        }

        int numAttrs = OGR_F_GetFieldCount(handle);

        std::string name;

        for (int i = 0; i < numAttrs; ++i)
        {
            //auto d = OGR_L_GetLayerDefn(handle); // Ensure layer definition is available
            
            OGRFieldDefnH field_handle_ref = OGR_F_GetFieldDefnRef(handle, i);

            if (i < fieldNames.size())
            {
                name = fieldNames[i];
            }
            else
            {
                // get the field name and convert to lower case:
                name = std::string(OGR_Fld_GetNameRef(field_handle_ref));
                name = util::toLower(name);
            }

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

void*
GDALFeatureSource::openGDALDataset() const
{
    int openFlags = GDAL_OF_VECTOR | GDAL_OF_READONLY;
    if (Log()->level() >= log::level::info)
        openFlags |= GDAL_OF_VERBOSE_ERROR;

    std::vector<const char*> driversCC;
    if (ogrDriver.has_value())
    {
        driversCC.emplace_back(ogrDriver.value().c_str());
        driversCC.emplace_back(nullptr);
    }

    std::vector<const char*> openOptionsCC;
    if (!openOptions.empty())
    {
        for (auto& oo : openOptions)
            openOptionsCC.emplace_back(oo.c_str());
        openOptionsCC.emplace_back(nullptr);
    }

    return GDALOpenEx(
        _source.c_str(),
        openFlags,
        driversCC.empty() ? nullptr : driversCC.data(),
        openOptionsCC.empty() ? nullptr : openOptionsCC.data(),
        nullptr);
}

FeatureSource::iterator
GDALFeatureSource::iterate(const IOOptions& io)
{
    OGRDataSourceH dsHandle = nullptr;
    OGRLayerH layerHandle = (OGRLayerH)externalLayerHandle;

    if (!layerHandle)
    {
        dsHandle = (OGRDataSourceH)openGDALDataset();

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
        i->_layerHandle = layerHandle;
        i->_metadata = &_metadata;
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

    _resultSetHandle = _layerHandle;

    if (_resultSetHandle)
        OGR_L_ResetReading((OGRLayerH)_resultSetHandle);

    readChunk();
}

GDALFeatureSource::iterator_impl::~iterator_impl()
{
    if (_nextHandleToQueue)
    {
        OGR_F_Destroy((OGRFeatureH)_nextHandleToQueue);
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
        auto handle = OGR_L_GetNextFeature((OGRLayerH)_resultSetHandle);
        if (handle)
        {
            Feature feature;

            create_feature_from_OGR_handle(handle, srs, _metadata->fieldNames, feature);

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
        OGR_L_ResetReading((OGRLayerH)_resultSetHandle);
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
    // simply iterating over the cursor, making the cursor move convenient to use.
    _lastFeatureReturned = _queue.front();
    _queue.pop();

    return _lastFeatureReturned;
}




Result<>
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
            return Failure(Failure::ConfigurationError, "No URL, connection, or inline geometry provided");
        }

        // assume we're loading from the URL/connection:
        // remember the thread so we don't use the handles illegaly.
        _dsHandleThreadId = std::thread::id();

        // this handle may ONLY be used from this thread!
        // https://github.com/OSGeo/gdal/blob/v2.4.1/gdal/gcore/gdaldataset.cpp#L2577
        _dsHandle = openGDALDataset();

        if (!_dsHandle)
        {
            return Failure(Failure::ResourceUnavailable, "Failed to open \"" + _source + "\"");
        }

        // Open a specific layer within the data source, if applicable:
        _layerHandle = open_OGR_layer(_dsHandle, layerName);

        if (!_layerHandle)
        {
            return Failure(Failure::ResourceUnavailable, 
                "Failed to open layer \"" + layerName + "\" from \"" + _source + "\"");
        }

        _featureCount = (int)OGR_L_GetFeatureCount((OGRLayerH)_layerHandle, 1);

        // Build the field schema:
        auto fetaureDefHandle = OGR_L_GetLayerDefn((OGRLayerH)_layerHandle);
        if (fetaureDefHandle)
        {
            int count = OGR_FD_GetFieldCount(fetaureDefHandle);
            for (int i = 0; i < count; ++i)
            {
                auto fieldDefHandle = OGR_FD_GetFieldDefn(fetaureDefHandle, i);
                if (fieldDefHandle)
                {
                    auto* name = OGR_Fld_GetNameRef(fieldDefHandle);
                    if (name)
                    {
                        _metadata.fieldNames.emplace_back(util::toLower(std::string(name)));
                    }
                }
            }
        }

        // extract the SRS and Extent:
        OGRSpatialReferenceH srHandle = OGR_L_GetSpatialRef((OGRLayerH)_layerHandle);
        if (!srHandle)
        {
            return Failure(Failure::ResourceUnavailable, "No spatial reference found in \"" + _source + "\"");
        }

        SRS srs;
        char* wktbuf;
        if (OSRExportToWkt(srHandle, &wktbuf) == OGRERR_NONE)
        {
            srs = SRS(wktbuf);
            CPLFree(wktbuf);
            if (!srs.valid())
            {
                return Failure(Failure::ResourceUnavailable, "Unrecognized SRS found in \"" + _source + "\"");
            }
        }

        // extract the full extent of the layer:
        OGREnvelope env;
        if (OGR_L_GetExtent((OGRLayerH)_layerHandle, &env, 1) != OGRERR_NONE)
        {
            return Failure(Failure::ResourceUnavailable, "Invalid extent returned from \"" + _source + "\"");
        }

        GeoExtent extent(srs, env.MinX, env.MinY, env.MaxX, env.MaxY);
        if (!extent.valid())
        {
            return Failure(Failure::ResourceUnavailable, "Invalid extent returned from \"" + _source + "\"");
        }

        // Made it!
        _metadata.extent = extent;
    }

    Log()->debug("GDAL features " + _source + " opened OK");

    return ResultVoidOK;
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
        OGRReleaseDataSource((OGRDataSourceH)_dsHandle);
        _dsHandle = nullptr;
    }
}

#endif // ROCKY_HAS_GDAL
