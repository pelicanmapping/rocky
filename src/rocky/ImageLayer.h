/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/TileLayer.h>
#include <rocky/GeoImage.h>
#include <rocky/Color.h>

namespace ROCKY_NAMESPACE
{
    class Color;
    class Profile;

    template<class Key, class Value>
    class DependencyCache
    {
    public:
        std::shared_ptr<Value> operator[](const Key& key)
        {
            const std::lock_guard lock{ _mutex };
            auto itr = _map.find(key);
            if (itr != _map.end())
                return itr->second.lock();
            return nullptr;
        }

        std::shared_ptr<Value> put(const Key& key, const std::shared_ptr<Value>& value)
        {
            const std::lock_guard lock{ _mutex };
            auto itr = _map.find(key);
            if (itr != _map.end())
            {
                std::shared_ptr<Value> preexisting = itr->second.lock();
                if (preexisting)
                    return preexisting;
            }
            _map[key] = value;
            return value;
        }
        
        void clean()
        {
            const std::lock_guard lock{ _mutex };
            for (auto itr = _map.begin(), end = _map.end(); itr != end;)
            {
                if (!itr->second.lock())
                    itr = _map.erase(itr);
                else
                    ++itr;
            }
        }

    private:
        std::unordered_map<Key, std::weak_ptr<Value>> _map;
        std::mutex _mutex;
    };

    /**
     * A map terrain layer containing bitmap image data.
     */
    class ROCKY_EXPORT ImageLayer : public Inherit<TileLayer, ImageLayer>
    {
    public:
        virtual ~ImageLayer() { }

        shared_ptr<Image> getEmptyImage() const {
            return _emptyImage;
        }
        
        //! serialize
        JSON to_json() const override;

    public: // methods

        //! Creates an image for the given tile key.
        //! @param key TileKey for which to create an image
        //! @param progress Optional progress/cancelation callback
        Result<GeoImage> createImage(
            const TileKey& key) const;

        //! Creates an image for the given tile key.
        //! @param key TileKey for which to create an image
        //! @param progress Optional progress/cancelation callback
        Result<GeoImage> createImage(
            const TileKey& key,
            const IOOptions& io) const;

        //! Stores an image in this layer (if writing is enabled).
        //! Returns a status value indicating whether the store succeeded.
        Status writeImage(
            const TileKey& key,
            shared_ptr<Image> image,
            const IOOptions& io);

        //! Returns the compression method prefered by this layer
        //! that you can pass to ImageUtils::compressImage.
        const std::string getCompressionMethod() const;

    public:

        //! Subclass overrides this to generate image data for the key.
        //! The key will always be in the same profile as the layer.
        virtual Result<GeoImage> createImageImplementation(
            const TileKey& key,
            const IOOptions& io) const
        {
            return Result(GeoImage::INVALID);
        }

    protected:

        ImageLayer();

        ImageLayer(const JSON&);

        //! Subclass can override this to write data for a tile key.
        virtual Status writeImageImplementation(
            const TileKey& key,
            shared_ptr<Image> image,
            const IOOptions& io) const;

        //! Modify the bbox if an altitude is set (for culling)
        virtual void modifyTileBoundingBox(
            const TileKey& key,
            Box& box) const;

        //! Post processing image creation entry points
        Result<GeoImage> createImage(
            const GeoImage& canvas,
            const TileKey& key,
            const IOOptions& io);

        //! Override to write an image over top of an existing image
        virtual Result<GeoImage> createImageImplementation(
            const GeoImage& canvas,
            const TileKey& key,
            const IOOptions& io) const { return Result(canvas); }

        //! Override to do something to an image before returning
        //! it from createImage (including a GeoImage read from the cache)
        virtual void postCreateImageImplementation(
            GeoImage& createdImage,
            const TileKey& key,
            const IOOptions& io) const { }

    protected: // Layer

        shared_ptr<Image> _emptyImage;
        optional<std::string> _noDataImageLocation = { };
        optional<Color> _transparentColor = Color(0, 0, 0, 0);
        optional<std::string> _textureCompression;

    private:

        void construct(const JSON&);

        // Creates an image that's in the same profile as the provided key.
        Result<GeoImage> createImageInKeyProfile(
            const TileKey& key,
            const IOOptions& io) const;

        // Fetches multiple images from the TileSource; mosaics/reprojects/crops as necessary, and
        // returns a single tile. This is called by createImageFromTileSource() if the key profile
        // doesn't match the layer profile.
        shared_ptr<Image> assembleImage(
            const TileKey& key,
            const IOOptions& io) const;

        std::shared_ptr<DependencyCache<TileKey, Image>> _dependencyCache;
    };

} // namespace ROCKY_NAMESPACE

