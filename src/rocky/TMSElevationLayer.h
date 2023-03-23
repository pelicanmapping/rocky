/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/TMS.h>
#include <rocky/ElevationLayer.h>
#include <rocky/URI.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Elevation layer reading from a TMS (Tile Map Service) endpoint
     */
    class ROCKY_EXPORT TMSElevationLayer : public Inherit<ElevationLayer, TMSElevationLayer>
    {
    public:
        //! Construct an empty TMS layer
        TMSElevationLayer();
        TMSElevationLayer(const Config&);

        //! Destructor
        virtual ~TMSElevationLayer() { }

        //! Base URL for TMS requests
        void setURI(const URI& value) { _options.uri = value; }
        const optional<URI>& uri() const { return _options.uri; }

        //! Options TMS "type"; only possible setting is "google" which will
        //! invert the Y axis for tile indices
        void setTMSType(const std::string& value) { _options.tmsType = value; }
        const optional<std::string>& tmsType() const { return _options.tmsType; }

        //! Data format to request from the service
        void setFormat(const std::string& value) { _options.format = value; }
        const optional<std::string>& format() const { return _options.format; }

        //! Whether the layer contains coverage data
        void setCoverage(bool value) { _options.coverage = value; }
        const optional<bool>& coverage() const { return _options.coverage; }

        //! Serialize
        Config getConfig() const override;

    public: // Layer

        Status openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

        //! Creates a raster image for the given tile key
        Result<GeoHeightfield> createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const override;

        //! Writes a raster image for he given tile key (if open for writing)
        //virtual Status writeImageImplementation(const TileKey& key, const osg::Image* image, ProgressCallback* progress) const override;

    protected: // Layer

        //! Called by constructors
        //virtual void init() override;

        //virtual bool isWritingSupported() const override { return true; }


    private:
        TMS::Driver _driver;
        TMS::Options _options;        
        void construct(const Config&);
    };
}


#if 0
    /**
     * Elevation layer connected to a TMS (Tile Map Service) facility
     */
    class OSGEARTH_EXPORT TMSElevationLayer : public ElevationLayer
    {
    public: // serialization
        class OSGEARTH_EXPORT Options : public ElevationLayer::Options, public TMS::Options {
        public:
            META_LayerOptions(osgEarth, Options, ElevationLayer::Options);
            virtual Config getConfig() const;
        private:
            void fromConfig(const Config& conf);
        };

    public:
        META_Layer(osgEarth, TMSElevationLayer, Options, ElevationLayer, TMSElevation);

        //! Base URL for TMS requests
        void setURL(const URI& value);
        const URI& getURL() const;

        //! Options TMS "type"; only possible setting is "google" which will
        //! invert the Y axis for tile indices
        void setTMSType(const std::string& value);
        const std::string& getTMSType() const;

        //! Data format to request from the service
        void setFormat(const std::string& value);
        const std::string& getFormat() const;

    public: // Layer
        
        //! Establishes a connection to the TMS repository
        virtual Status openImplementation() override;
        virtual Status closeImplementation() override;

        //! Creates a heightfield for the given tile key
        virtual GeoHeightField createHeightFieldImplementation(const TileKey& key, ProgressCallback* progress) const override;

        //! Writes a raster image for he given tile key (if open for writing)
        virtual Status writeHeightFieldImplementation(const TileKey& key, const osg::HeightField* hf, ProgressCallback* progress) const override;

    protected: // Layer

        //! Called by constructors
        virtual void init() override;

        virtual bool isWritingSupported() const override { return true; }

    protected:

        //! Destructor
        virtual ~TMSElevationLayer() { }

    private:
        osg::ref_ptr<TMSImageLayer> _imageLayer;
    };

} // namespace osgEarth

OSGEARTH_SPECIALIZE_CONFIG(osgEarth::TMSImageLayer::Options);
OSGEARTH_SPECIALIZE_CONFIG(osgEarth::TMSElevationLayer::Options);

#endif