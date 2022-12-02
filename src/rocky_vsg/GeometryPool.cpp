/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeometryPool.h"
#include "TerrainSettings.h"
#include <rocky/Notify.h>
//#include <vsg/commands/BindVertexBuffers.h>
//#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/DrawIndexed.h>

#undef LC
#define LC "[GeometryPool] "

using namespace rocky;

GeometryPool::GeometryPool() :
    _enabled(true),
    _debug(false),
    _mutex("GeometryPool"),
    _keygate("GeometryPool.keygate")
{
    //ROCKY_TODO("ADJUST_UPDATE_TRAV_COUNT(this, +1)");

    // activate debugging mode
    if ( getenv("ROCKY_DEBUG_REX_GEOMETRY_POOL") != 0L )
    {
        _debug = true;
    }

    if ( ::getenv("ROCKY_REX_NO_POOL") )
    {
        _enabled = false;
        ROCKY_INFO << LC << "Geometry pool disabled (environment)" << std::endl;
    }
}

void
GeometryPool::getPooledGeometry(
    const TileKey& tileKey,
    const Map* map,
    const TerrainSettings& settings,
    vsg::ref_ptr<SharedGeometry>& out,
    Cancelable* progress)
{
    // convert to a unique-geometry key:
    GeometryKey geomKey;
    createKeyForTileKey( tileKey, settings.tileSize, geomKey );

    // make our globally shared EBO if we need it
    {
        util::ScopedLock lock(_mutex);
        if (_defaultIndices == nullptr)
        {
            _defaultIndices = createIndices(settings);
        }
    }

    ROCKY_TODO("MeshEditor meshEditor(tileKey, tileSize, map, nullptr);");

    if ( _enabled )
    {
        // Protect access on a per key basis to prevent the same key from being created twice.  
        // This was causing crashes with multiple windows opening and closing.
        util::ScopedGate<GeometryKey> gatelock(_keygate, geomKey);

        // first check the sharing cache:
        //if (!meshEditor.hasEdits())
        {
            util::ScopedLock lock(_mutex);
            auto i = _sharedGeometries.find(geomKey);
            if (i != _sharedGeometries.end())
            {
                // found it:
                out = i->second.get();
            }
        }

        if (!out.valid())
        {
            out = createGeometry(
                tileKey,
                settings,
                //meshEditor,
                progress);

            // only store as a shared geometry if there are no constraints.
            if (out.valid()) //&& !meshEditor.hasEdits())
            {
                util::ScopedLock lock(_mutex);
                _sharedGeometries[geomKey] = out.get();
            }
        }
    }

    else
    {
        out = createGeometry(
            tileKey,
            settings,
            //meshEditor,
            progress);
    }
}

void
GeometryPool::createKeyForTileKey(
    const TileKey& tileKey,
    unsigned tileSize,
    GeometryKey& out) const
{
    out.lod  = tileKey.getLOD();
    out.tileY = tileKey.getProfile()->getSRS()->isGeographic()? tileKey.getTileY() : 0;
    out.size = tileSize;
}

int
GeometryPool::getNumSkirtElements(
    unsigned tileSize,
    float skirtRatio) const
{
    return skirtRatio > 0.0f ? (tileSize-1) * 4 * 6 : 0;
}

namespace
{
    int getMorphNeighborIndexOffset(unsigned col, unsigned row, int rowSize)
    {
        if ( (col & 0x1)==1 && (row & 0x1)==1 ) return rowSize+2;
        if ( (row & 0x1)==1 )                   return rowSize+1;
        if ( (col & 0x1)==1 )                   return 2;
        return 1;
    }
}

#define addSkirtDataForIndex(P, INDEX, HEIGHT) \
{ \
    verts->set(P, (*verts)[INDEX] ); \
    normals->set(P, (*normals)[INDEX] ); \
    uvs->set(P, (*uvs)[INDEX] ); \
    uvs->at(P).z = (float)((int)uvs->at(P).z | VERTEX_SKIRT); \
    if ( neighbors ) neighbors->set(P, (*neighbors)[INDEX] ); \
    if ( neighborNormals ) neighborNormals->set(P, (*neighborNormals)[INDEX] ); \
    ++P; \
    verts->set(P, (*verts)[INDEX] - ((*normals)[INDEX])*(HEIGHT) ); \
    normals->set(P, (*normals)[INDEX] ); \
    uvs->set(P, (*uvs)[INDEX] ); \
    uvs->at(P).z = (float)((int)uvs->at(P).z | VERTEX_SKIRT); \
    if ( neighbors ) neighbors->set(P, (*neighbors)[INDEX] - ((*normals)[INDEX])*(HEIGHT) ); \
    if ( neighborNormals ) neighborNormals->set(P, (*neighborNormals)[INDEX] ); \
    ++P; \
}

#define addSkirtTriangles(P, INDEX0, INDEX1) \
{ \
    indices->set(P++, (INDEX0));   \
    indices->set(P++, (INDEX0)+1); \
    indices->set(P++, (INDEX1));   \
    indices->set(P++, (INDEX1));   \
    indices->set(P++, (INDEX0)+1); \
    indices->set(P++, (INDEX1)+1); \
}

vsg::ref_ptr<vsg::ushortArray>
GeometryPool::createIndices(
    const TerrainSettings& settings) const
{
    ROCKY_HARD_ASSERT(settings.tileSize > 0u);

    // Attempt to calculate the number of verts in the surface geometry.
    bool needsSkirt = settings.skirtRatio > 0.0f;
    uint32_t tileSize = settings.tileSize;

    unsigned numVertsInSurface = (tileSize*tileSize);
    unsigned numVertsInSkirt = needsSkirt ? (tileSize - 1) * 2u * 4u : 0;
    unsigned numVerts = numVertsInSurface + numVertsInSkirt;
    unsigned numIndicesInSurface = (tileSize - 1) * (tileSize - 1) * 6;
    unsigned numIncidesInSkirt = getNumSkirtElements(tileSize, settings.skirtRatio);
    unsigned numIndices = numIndicesInSurface + numIncidesInSkirt;

    //GLenum mode = UseGpuTessellation ? GL_PATCHES : GL_TRIANGLES;

    auto indices = vsg::ushortArray::create(numIndices);

    //osg::ref_ptr<SharedDrawElements> primSet = new SharedDrawElements(mode);
    //primSet->reserveElements(numIndiciesInSurface + numIncidesInSkirt);

    // tessellate the surface:
    unsigned p = 0;
    for (unsigned j = 0; j < tileSize - 1; ++j)
    {
        for (unsigned i = 0; i < tileSize - 1; ++i)
        {
            int i00 = j * tileSize + i;
            int i01 = i00 + tileSize;
            int i10 = i00 + 1;
            int i11 = i01 + 1;

            unsigned k = j * tileSize + i;

            indices->set(p++, i01);
            indices->set(p++, i00);
            indices->set(p++, i11);

            indices->set(p++, i00);
            indices->set(p++, i10);
            indices->set(p++, i11);
        }
    }

    if (needsSkirt)
    {
        // add the elements for the skirt:
        int skirtBegin = numVertsInSurface;
        int skirtEnd = skirtBegin + numVertsInSkirt;
        int i;
        for (i = skirtBegin; i < (int)skirtEnd - 3; i += 2)
        {
            addSkirtTriangles(p, i, i + 2);
        }
        addSkirtTriangles(p, i, skirtBegin);
    }

    //primSet->setElementBufferObject(new osg::ElementBufferObject());

    return indices;
    //return primSet.release();
}



void
GeometryPool::tessellateSurface(
    unsigned tileSize,
    vsg::ref_ptr<vsg::ushortArray> indices) const
{
}

namespace
{
    struct Locator
    {
        shared_ptr<SRS> _srs;
        dmat4 _xform{ 1 }, _inverse{ 1 };

        Locator(const GeoExtent& extent)
        {
            _srs = extent.getSRS();
            ROCKY_HARD_ASSERT(_srs);

            _xform = dmat4(
                extent.width(), 0.0, 0.0, 0.0,
                0.0, extent.height(), 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                extent.xMin(), extent.yMin(), 0.0, 1.0);

            _inverse = glm::inverse(_xform);
        }

        void worldToUnit(const dvec3& world, dvec3& unit) const {
            if (_srs->isGeographic())
                unit = _srs->getEllipsoid().geocentricToGeodetic(world) * _inverse;
            else
                unit = world * _inverse;
        }

        void unitToWorld(const dvec3& unit, dvec3& world) const {
            world = unit * _xform;
            if (_srs->isGeographic())
                world = _srs->getEllipsoid().geodeticToGeocentric(world);
        }
    };
}

vsg::ref_ptr<SharedGeometry>
GeometryPool::createGeometry(
    const TileKey& tileKey,
    const TerrainSettings& settings,
    //MeshEditor& editor,
    Cancelable* progress) const
{
    // Establish a local reference frame for the tile:
    dvec3 centerWorld;
    GeoPoint centroid = tileKey.getExtent().getCentroid();
    centroid.toWorld( centerWorld );

    dmat4 world2local, local2world;
    centroid.createWorldToLocal(world2local);
    local2world = glm::inverse(world2local);// .invert(world2local);

    // Attempt to calculate the number of verts in the surface geometry.
    bool needsSkirt = settings.skirtRatio > 0.0f;

    auto tileSize = settings.tileSize;
    const uint32_t numVertsInSurface    = (tileSize*tileSize);
    const uint32_t numVertsInSkirt      = needsSkirt ? (tileSize-1)*2u * 4u : 0;
    const uint32_t numVerts             = numVertsInSurface + numVertsInSkirt;
    const uint32_t numIndiciesInSurface = (tileSize-1) * (tileSize-1) * 6;
    const uint32_t numIncidesInSkirt    = getNumSkirtElements(tileSize, settings.skirtRatio);

    ROCKY_TODO("GLenum mode = gpuTessellation ? GL_PATCHES : GL_TRIANGLES;");

    Sphere tileBound;
    //osg::BoundingSphere tileBound;

    ROCKY_TODO("Make the geometry!!");
//    osg::ref_ptr<osg::VertexBufferObject> vbo = new osg::VertexBufferObject();

    //SharedDrawElements* primSet = nullptr;

    // the initial vertex locations:
    auto verts = vsg::vec3Array::create(numVerts);
    //osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array();
    //verts->setVertexBufferObject(vbo.get());
    //verts->reserve( numVerts );
    //verts->setBinding(verts->BIND_PER_VERTEX);
    //geom->setVertexArray( verts.get() );

    // the surface normals (i.e. extrusion vectors)
    auto normals = vsg::vec3Array::create(numVerts);
    //osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    //normals->setVertexBufferObject(vbo.get());
    //normals->reserve( numVerts );
    //normals->setBinding(normals->BIND_PER_VERTEX);
    //geom->setNormalArray( normals.get() );

    // tex coord is [0..1] across the tile. The 3rd dimension tracks whether the
    // vert is masked: 0=yes, 1=no
    bool populate_uvs = true;
    auto uvs = vsg::vec3Array::create(numVerts);
    //osg::ref_ptr<osg::Vec3Array> texCoords = new osg::Vec3Array();
    //texCoords->setBinding(texCoords->BIND_PER_VERTEX);
    //texCoords->setVertexBufferObject(vbo.get());
    //texCoords->reserve( numVerts );
    //geom->setTexCoordArray(texCoords.get());

    vsg::ref_ptr<vsg::vec3Array> neighbors;
    vsg::ref_ptr<vsg::vec3Array> neighborNormals;
    if (settings.morphTerrain == true)
    {
        // neighbor positions (for morphing)
        neighbors = vsg::vec3Array::create(numVerts);
        //neighbors = new osg::Vec3Array();
        //neighbors->setBinding(neighbors->BIND_PER_VERTEX);
        //neighbors->setVertexBufferObject(vbo.get());
        //neighbors->reserve( numVerts );
        //geom->setNeighborArray(neighbors.get());

        neighborNormals = vsg::vec3Array::create(numVerts);
        //neighborNormals = new osg::Vec3Array();
        //neighborNormals->setVertexBufferObject(vbo.get());
        //neighborNormals->reserve( numVerts );
        //neighborNormals->setBinding(neighborNormals->BIND_PER_VERTEX);
        //geom->setNeighborNormalArray( neighborNormals.get() );
    }

#if 0
    if (editor.hasEdits())
    {
        bool tileHasData = editor.createTileMesh(
            geom.get(),
            tileSize,
            skirtRatio,
            mode,
            progress);

        if (geom->empty())
            return nullptr;
    }

    else // default mesh - no constraintsv
#endif
    {
        dvec3 unit;
        dvec3 model;
        dvec3 modelLTP;
        dvec3 modelPlusOne;
        dvec3 normal;

        Locator locator(tileKey.getExtent());

        for (unsigned row = 0; row < tileSize; ++row)
        {
            float ny = (float)row / (float)(tileSize - 1);
            for (unsigned col = 0; col < tileSize; ++col)
            {
                float nx = (float)col / (float)(tileSize - 1);
                unsigned i = row * tileSize + col;

                unit = dvec3(nx, ny, 0.0);
                locator.unitToWorld(unit, model);
                modelLTP = model * world2local;
                verts->set(i, vsg::vec3(modelLTP.x, modelLTP.y, modelLTP.z));
                //verts->push_back(modelLTP);

                tileBound.expandBy(modelLTP);

                if (populate_uvs)
                {
                    // Use the Z coord as a type marker
                    float marker = VERTEX_VISIBLE;
                    uvs->set(i, vsg::vec3(nx, ny, marker));
                    //xCoords->push_back(osg::Vec3f(nx, ny, marker));
                }

                unit.z = 1.0;
                //unit.z() = 1.0f;
                locator.unitToWorld(unit, modelPlusOne);
                normal = (modelPlusOne*world2local) - modelLTP;
                normal = glm::normalize(normal);
                //normal.normalize();
                normals->set(i, vsg::vec3(normal.x, normal.y, normal.z));
                //normals->push_back(normal);

                // neighbor:
                if (neighbors)
                {
                    auto& modelNeighborLTP = (*verts)[verts->size() - getMorphNeighborIndexOffset(col, row, tileSize)];
                    neighbors->set(i, modelNeighborLTP);
                }

                if (neighborNormals)
                {
                    auto& modelNeighborNormalLTP = (*normals)[normals->size() - getMorphNeighborIndexOffset(col, row, tileSize)];
                    neighborNormals->set(i, modelNeighborNormalLTP);
                }
            }
        }

        if (needsSkirt)
        {
            // calculate the skirt extrusion height
            float height = (float)tileBound.radius * settings.skirtRatio;

            // Normal tile skirt first:
            unsigned skirtIndex = verts->size();
            unsigned p = skirtIndex;

            // first, create all the skirt verts, normals, and texcoords.
            for (int c = 0; c < (int)tileSize - 1; ++c)
                addSkirtDataForIndex(p, c, height); //south

            for (int r = 0; r < (int)tileSize - 1; ++r)
                addSkirtDataForIndex(p, r*tileSize + (tileSize - 1), height); //east

            for (int c = tileSize - 1; c > 0; --c)
                addSkirtDataForIndex(p, (tileSize - 1)*tileSize + c, height); //north

            for (int r = tileSize - 1; r > 0; --r)
                addSkirtDataForIndex(p, r*tileSize, height); //west
        }

        // By default we tessellate the surface, but if there's a masking set
        // it might replace some or all of our surface geometry.
        //bool tessellateSurface = true;

//        if (tessellateSurface) // && primSet == nullptr)

        auto indices =
            _enabled ? _defaultIndices : createIndices(settings);

        //{
        //    if (_enabled)
        //        primSet = _defaultPrimSet.get();
        //    else
        //        primSet = createPrimitiveSet(settings);
        //}

        //if (primSet)
        //{
        //    geom->setDrawElements(primSet);
        //}
    //}

#if 0
    // if we are using GL4, create the GL4 tile model.
        if (/*using GL4*/true)
        {
            unsigned size = geom->getVertexArray()->size();

            geom->_verts.reserve(size);

            for (unsigned i = 0; i < size; ++i)
            {
                GL4Vertex v;

                v.position = (*geom->getVertexArray())[i];
                v.normal = (*geom->getNormalArray())[i];
                v.uv = (*geom->getTexCoordArray())[i];

                if (geom->getNeighborArray())
                    v.neighborPosition = (*geom->getNeighborArray())[i];

                if (geom->getNeighborNormalArray())
                    v.neighborNormal = (*geom->getNeighborNormalArray())[i];

                geom->_verts.emplace_back(std::move(v));
            }
        }
#endif

        //return geom.release();

        // the geometry:
        auto geom = SharedGeometry::create();
        //geom->verts = verts;
        //geom->normals = normals;
        //geom->uvs = uvs;
        //geom->neighbors = neighbors;
        //geom->neighborNormals = neighborNormals;
        //geom->indices = indices;

#if 0
        geom->addChild(vsg::BindVertexBuffers::create(
            0,
            vsg::DataList{ verts, normals, uvs, neighbors, neighborNormals }));

        geom->addChild(vsg::BindIndexBuffer::create(indices));

        geom->addChild(vsg::DrawIndexed::create(
            indices->size(), 1, 0, 0, 0));
#else
        geom->assignArrays(vsg::DataList{
            verts, normals, uvs, neighbors, neighborNormals });

        geom->assignIndices(indices);

        geom->commands.push_back(
            vsg::DrawIndexed::create(
                indices->size(), // index count
                1,               // instance count
                0,               // first index
                0,               // vertex offset
                0));             // first instance
#endif

        return geom;
    }
}

#if 0
void
GeometryPool::traverse(osg::NodeVisitor& nv)
{
    if (nv.getVisitorType() == nv.UPDATE_VISITOR && _enabled)
    {
        Threading::ScopedMutexLock lock(_geometryMapMutex);

        std::vector<GeometryKey> keys;

        for(auto& iter : _geometryMap)
        {
            if (iter.second.get()->referenceCount() == 1)
            {
                keys.push_back(iter.first);
                //iter.second->releaseGLObjects(nullptr);
                OE_DEBUG << "Releasing: " << iter.second.get() << std::endl;
            }
        }

        for(auto& key : keys)
        {
            _geometryMap.erase(key);
        }
    }

    osg::Group::traverse(nv);
}
#endif

void
GeometryPool::clear()
{
    //releaseGLObjects(nullptr);
    util::ScopedLock lock(_mutex);
    _sharedGeometries.clear();
}

#if 0
void
GeometryPool::resizeGLObjectBuffers(unsigned maxsize)
{
    if (!_enabled)
        return;

    // collect all objects in a thread safe manner
    util::ScopedLock lock(_geometryMapMutex);

    for (GeometryMap::const_iterator i = _geometryMap.begin(); i != _geometryMap.end(); ++i)
    {
        i->second->resizeGLObjectBuffers(maxsize);
    }

    // the shared primitive set
    if (_defaultPrimSet.valid())
    {
        _defaultPrimSet->resizeGLObjectBuffers(maxsize);
    }
}

void
GeometryPool::releaseGLObjects(osg::State* state) const
{
    if (!_enabled)
        return;

    // collect all objects in a thread safe manner
    util::ScopedLock lock(_geometryMapMutex);

    for (auto& entry : _geometryMap)
    {
        entry.second->releaseGLObjects(state);
    }

    // the shared primitive set
    if (_defaultPrimSet.valid())
    {
        _defaultPrimSet->releaseGLObjects(state);
    }
}
#endif
