/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileModelFactory.h"
#include "Map.h"
#include "Metrics.h"
#include "Notify.h"
#include "ElevationLayer.h"
#include "ImageLayer.h"

#define LC "[TerrainTileModelFactory] "

using namespace ROCKY_NAMESPACE;

CreateTileManifest::CreateTileManifest()
{
    _includesElevation = false;
    _includesConstraints = false;
    _progressive.setDefault(false);
}

void
CreateTileManifest::insert(shared_ptr<Layer> layer)
{
    if (layer)
    {
        _layers[layer->uid()] = layer->getRevision();

        if (std::dynamic_pointer_cast<ElevationLayer>(layer))
        {
            _includesElevation = true;
        }

        //else if (std::dynamic_pointer_cast<TerrainConstraintLayer>(layer))        
        //{
        //    _includesConstraints = true;
        //}
    }
}

bool
CreateTileManifest::excludes(const Layer* layer) const
{
    return !empty() && _layers.find(layer->uid()) == _layers.end();
}

bool
CreateTileManifest::empty() const
{
    return _layers.empty();
}

bool
CreateTileManifest::inSyncWith(const Map* map) const
{
    for(auto& iter : _layers)
    {
        auto layer = map->getLayerByUID(iter.first);

        // note: if the layer is null, it was removed, so let it pass.
        if (layer && layer->getRevision() != iter.second)
        {
            return false;
        }
    }
    return true;
}

void
CreateTileManifest::updateRevisions(const Map* map)
{
    for (auto& iter : _layers)
    {
        auto layer = map->getLayerByUID(iter.first);
        if (layer)
        {
            iter.second = layer->getRevision();
        }
    }
}

bool
CreateTileManifest::includes(const Layer* layer) const
{
    return includes(layer->uid());
}

bool
CreateTileManifest::includes(UID uid) const
{
    return empty() || _layers.find(uid) != _layers.end();
}

bool
CreateTileManifest::includesElevation() const
{
    return empty() || _includesElevation;
}

bool
CreateTileManifest::includesConstraints() const
{
    return _includesConstraints;
}

void
CreateTileManifest::setProgressive(bool value)
{
    _progressive = value;
}

//.........................................................................

TerrainTileModelFactory::TerrainTileModelFactory()
{
    //nop
}

TerrainTileModel
TerrainTileModelFactory::createTileModel(
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    const IOOptions& io)
{
    ROCKY_PROFILING_ZONE;

    // Make a new model:
    TerrainTileModel model;
    model.key = key;
    model.revision = map->dataModelRevision();

    // assemble all the components:
    addColorLayers(model, map, key, manifest, io, false);

    unsigned border = 0u;
    addElevation(model, map, key, manifest, border, io);

    return std::move(model);
}

TerrainTileModel
TerrainTileModelFactory::createStandaloneTileModel(
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    const IOOptions& io)
{
    ROCKY_PROFILING_ZONE;

    // Make a new model:
    TerrainTileModel model;
    model.key = key;
    model.revision = map->dataModelRevision();

    // assemble all the components:
    addColorLayers(model, map, key, manifest, io, true);

    addStandaloneElevation(model, map, key, manifest, 0u, io);

    return std::move(model);
}

bool
TerrainTileModelFactory::addImageLayer(
    TerrainTileModel& model,
    shared_ptr<const ImageLayer> imageLayer,
    const TileKey& key,
    const IOOptions& io)
{
    ROCKY_PROFILING_ZONE;
    ROCKY_PROFILING_ZONE_TEXT(imageLayer->getName());

    if (imageLayer->isOpen() &&
        imageLayer->isKeyInLegalRange(key) &&
        imageLayer->mayHaveData(key))
    {
        auto result = imageLayer->createImage(key, io);
        if (result.value.valid())
        {
            TerrainTileModel::ColorLayer m;
            m.layer = imageLayer;
            m.revision = imageLayer->getRevision();
            m.image = result.value;
            model.colorLayers.emplace_back(std::move(m));

            //if (imageLayer->isShared())
            //{
            //    model.sharedLayerIndices.push_back(model.colorLayers.size());
            //}

            if (imageLayer->isDynamic() || imageLayer->getAsyncLoading())
            {
                model.requiresUpdate = true;
            }

            return true;
        }
        else if (result.status.failed() && io.services().log)
        {
            //io.services().log().warn << result.status.message << std::endl;
        }
    }
    return false;
}

void
TerrainTileModelFactory::addStandaloneImageLayer(
    TerrainTileModel& model,
    shared_ptr<const ImageLayer> imageLayer,
    const TileKey& key,
    const IOOptions& io)
{
    TileKey key_to_use = key;

    bool added = false;
    while (key_to_use.valid() & !added)
    {
        if (addImageLayer(model, imageLayer, key_to_use, io))
            added = true;
        else
            key_to_use.makeParent();
    }
}

void
TerrainTileModelFactory::addColorLayers(
    TerrainTileModel& model,
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    const IOOptions& io,
    bool standalone)
{
    ROCKY_PROFILING_ZONE;

    int order = 0;

    auto layers = map->getLayers<Layer>();

    for(auto layer : layers)
    {
        // skip layers that are closed
        if (!layer->isOpen())
            continue;

        if (layer->getRenderType() != layer->RENDERTYPE_TERRAIN_SURFACE)
            continue;

        if (manifest.excludes(layer.get()))
            continue;

        auto imageLayer = ImageLayer::cast(layer);
        if (imageLayer)
        {
            if (standalone)
            {
                addStandaloneImageLayer(model, imageLayer, key, io);
            }
            else
            {
                addImageLayer(model, imageLayer, key, io);
            }
        }
        else // non-image kind of TILE layer (e.g., splatting)
        {
            TerrainTileModel::ColorLayer colorModel;
            colorModel.layer = layer;
            colorModel.revision = layer->getRevision();
            model.colorLayers.push_back(std::move(colorModel));
        }
    }
}

bool
TerrainTileModelFactory::addElevation(
    TerrainTileModel& model,
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    unsigned border,
    const IOOptions& io)
{
    ROCKY_PROFILING_ZONE;
    ROCKY_PROFILING_ZONE_TEXT("Elevation");

    bool needElevation = manifest.includesElevation();
    auto layers = map->getLayers<ElevationLayer>();
    //std::vector<shared_ptr<const ElevationLayer>> layers;
    //map->getLayers(layers);

    int combinedRevision = map->dataModelRevision();
    if (!manifest.empty())
    {
        for (const auto& layer : layers)
        {
            if (needElevation == false && !manifest.excludes(layer.get()))
            {
                needElevation = true;
            }
            combinedRevision += layer->getRevision();
        }
    }
    if (!needElevation)
        return false;

    ROCKY_TODO("Write the Elevation Fetcher");

#if 0 // TODO
    osg::ref_ptr<ElevationTexture> elevTex;

    const bool acceptLowerRes = false;

    if (map->getElevationPool()->getTile(key, acceptLowerRes, elevTex, &_workingSet, progress))
    {
        if (elevTex.valid())
        {
            model->elevation().revision() = combinedRevision;
            model->elevation().texture() = Texture::create(elevTex.get());
            model->elevation().texture()->category() = LABEL_ELEVATION;

            if (_options.useNormalMaps() == true)
            {
                // Make a normal map if it doesn't already exist
                elevTex->generateNormalMap(map, &_workingSet, progress);

                if (elevTex->getNormalMapTexture())
                {
                    elevTex->getNormalMapTexture()->setName(key.str() + ":normalmap");
                    model->normalMap().texture() = Texture::create(elevTex->getNormalMapTexture());
                    model->normalMap().texture()->category() = LABEL_NORMALMAP;
                }
            }

            // Keep the heightfield pointer around for legacy 3rd party usage (VRF)
            model->elevation().heightField() = elevTex->getHeightField();
        }
    }
#endif

    return true;
}

bool
TerrainTileModelFactory::addStandaloneElevation(
    TerrainTileModel& model,
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    unsigned border,
    const IOOptions& io)
{
    TileKey key_to_use = key;

    bool added = false;
    while (key_to_use.valid() & !added)
    {
        if (addElevation(model, map, key_to_use, manifest, border, io))
            added = true;
        else
            key_to_use.makeParent();
    }

    return added;
}

#if 0
namespace
{
    //#define DEBUG_TEXTURES

    struct DebugTexture2D : public osg::Texture2D
    {
        DebugTexture2D(osg::Image* image) : osg::Texture2D(image) { }
        virtual ~DebugTexture2D() {
            OE_INFO << "Deleted texture " << getName() << std::endl;
        }
        void releaseGLObjects(osg::State* state) const {
            osg::Texture2D::releaseGLObjects(state);
            OE_INFO << "Released texture " << getName() << std::endl;
        }
    };

    osg::Texture2D* createTexture2D(const osg::Image* image)
    {
#ifdef DEBUG_TEXTURES
        return new DebugTexture2D(const_cast<osg::Image*>(image));
#else
        return new osg::Texture2D(const_cast<osg::Image*>(image));
#endif
    }
}

Texture::Ptr
TerrainTileModelFactory::createImageTexture(
    const osg::Image* image,
    const ImageLayer* layer) const
{
    if (image == nullptr || layer == nullptr)
        return nullptr;

    osg::Texture* tex = nullptr;
    bool hasMipMaps = false;
    bool isCompressed = false;

    if (image->requiresUpdateCall())
    {
        // image sequences and other data that updates itself
        // shall not be mipmapped/compressed here
        tex = createTexture2D(image);
    }
    else
    {
        // figure out the texture compression method to use (if any)
        std::string compressionMethod = layer->getCompressionMethod();
        if (compressionMethod.empty())
            compressionMethod = _options.textureCompression().get();

        GLenum pixelFormat = image->getPixelFormat();
        GLenum internalFormat = image->getInternalTextureFormat();
        GLenum dataType = image->getDataType();

        // Fix incorrect internal format if necessary
        if (internalFormat == pixelFormat)
        {
            int bits = dataType == GL_UNSIGNED_BYTE ? 8 : 16;
            if (pixelFormat == GL_RGB) internalFormat = bits == 8 ? GL_RGB8 : GL_RGB16;
            else if (pixelFormat == GL_RGBA) internalFormat = bits == 8 ? GL_RGBA8 : GL_RGBA16;
            else if (pixelFormat == GL_RG) internalFormat = bits == 8 ? GL_RG8 : GL_RG16;
            else if (pixelFormat == GL_RED) internalFormat = bits == 8 ? GL_R8 : GL_R16;
        }

        if (image->r() == 1)
        {
            osg::ref_ptr<const osg::Image> compressed = ImageUtils::compressImage(image, compressionMethod);
            const osg::Image* mipmapped = ImageUtils::mipmapImage(compressed.get());

            tex = createTexture2D(mipmapped);

            hasMipMaps = mipmapped->isMipmap();
            isCompressed = mipmapped->isCompressed();

            if (layer->getCompressionMethod() == "gpu" && !mipmapped->isCompressed())
                tex->setInternalFormatMode(tex->USE_S3TC_DXT5_COMPRESSION);
        }

        else // if (image->r() > 1)
        {
            std::vector< osg::ref_ptr<osg::Image> > images;
            ImageUtils::flattenImage(image, images);

            // Make sure we are using a proper sized internal format
            for (int i = 0; i < images.size(); ++i)
            {
                images[i]->setInternalTextureFormat(internalFormat);
            }

            osg::ref_ptr<const osg::Image> compressed;
            for (auto& ref : images)
            {
                compressed = ImageUtils::compressImage(ref.get(), compressionMethod);
                ref = const_cast<osg::Image*>(ImageUtils::mipmapImage(compressed.get()));

                hasMipMaps = compressed->isMipmap();
                isCompressed = compressed->isCompressed();
            }

            osg::Texture2DArray* tex2dArray = new osg::Texture2DArray();

            tex2dArray->setTextureSize(image[0].s(), image[0].t(), images.size());
            tex2dArray->setInternalFormat(images[0]->getInternalTextureFormat());
            tex2dArray->setSourceFormat(images[0]->getPixelFormat());
            for (int i = 0; i < (int)images.size(); ++i)
                tex2dArray->setImage(i, const_cast<osg::Image*>(images[i].get()));

            tex = tex2dArray;

            if (layer->getCompressionMethod() == "gpu" && !isCompressed)
                tex->setInternalFormatMode(tex->USE_S3TC_DXT5_COMPRESSION);
        }

        tex->setDataVariance(osg::Object::STATIC);
    }

    tex->setWrap( osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE );
    tex->setWrap( osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE );
    tex->setResizeNonPowerOfTwoHint(false);

    osg::Texture::FilterMode magFilter =
        layer ? layer->options().magFilter().get() : osg::Texture::LINEAR;
    osg::Texture::FilterMode minFilter =
        layer ? layer->options().minFilter().get() : osg::Texture::LINEAR;

    tex->setFilter( osg::Texture::MAG_FILTER, magFilter );
    tex->setFilter( osg::Texture::MIN_FILTER, minFilter );
    tex->setMaxAnisotropy( 4.0f );

    // Disable mip mapping if we don't have it
    if (!hasMipMaps)
    {
        tex->setFilter( osg::Texture::MIN_FILTER, osg::Texture::LINEAR );
    }

    tex->setUnRefImageDataAfterApply(Registry::instance()->unRefImageDataAfterApply().get());

    // For GL_RED, swizzle the RGBA all to RED in order to match old GL_LUMINANCE behavior
    for(unsigned i=0; i< tex->getNumImages(); ++i)
    {
        if (tex->getImage(i) && tex->getImage(i)->getPixelFormat() == GL_RED)
        {
            tex->setSwizzle(osg::Vec4i(GL_RED, GL_RED, GL_RED, GL_RED));
            break;
        }
    }

    return Texture::create(tex);
}

Texture::Ptr
TerrainTileModelFactory::createCoverageTexture(const osg::Image* image) const
{
    osg::Texture2D* tex = createTexture2D(image);
    tex->setDataVariance(osg::Object::STATIC);

    tex->setInternalFormat(LandCover::getTextureFormat());

    tex->setWrap( osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE );
    tex->setWrap( osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE );
    tex->setResizeNonPowerOfTwoHint(false);

    tex->setFilter( osg::Texture::MAG_FILTER, osg::Texture::NEAREST );
    tex->setFilter( osg::Texture::MIN_FILTER, osg::Texture::NEAREST );

    tex->setMaxAnisotropy( 1.0f );

    tex->setUnRefImageDataAfterApply(Registry::instance()->unRefImageDataAfterApply().get());

    return Texture::create(tex);
}
#endif