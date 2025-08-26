/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Feature.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
#ifdef ROCKY_HAS_GDAL

    /**
    * Reads Feature objects from various sources using the GDAL vector drivers.
    */
    class ROCKY_EXPORT GDALFeatureSource : public rocky::Inherit<FeatureSource, GDALFeatureSource>
    {
    public:

        //! URI of source data, like a shapefile or connection string
        option<URI> uri;

        //! Optional name of the specific OGR driver to load
        option<std::string> ogrDriver;

        //! Optional layer name to open, for sources that support layers
        std::string layerName;

        //! Use these to create a feature source from an existing OGR layer handle and SRS.
        //! Leave URI empty if you use this method.
        void* externalLayerHandle = nullptr;
        SRS externalSRS = SRS::WGS84;

        //! GDAL driver open options, each in the format "VAR=VALUE"
        std::vector<std::string> openOptions;

    public:

        //! Opens the source and returns a status indicating success or failure.
        Result<> open();

        //! Closes the source.
        void close();

        //! Create an interator to read features from the source
        FeatureSource::iterator iterate(const IOOptions& io) override;

        //! Number of features, or -1 if the count isn't available
        int featureCount() const override;

        // destructor
        virtual ~GDALFeatureSource();

    private:
        void* _dsHandle = nullptr;
        void* _layerHandle = nullptr;
        int _featureCount = -1;
        std::thread::id _dsHandleThreadId;
        Metadata _metadata;
        std::string _source;

        void* openGDALDataset() const;

        class ROCKY_EXPORT iterator_impl : public FeatureSource::iterator::implementation
        {
        public:
            ~iterator_impl();
            bool hasMore() const override;
            Feature next() override;
        private:
            std::queue<Feature> _queue;
            Feature _lastFeatureReturned;
            GDALFeatureSource* _source = nullptr;
            void* _layerHandle = nullptr;
            const FeatureSource::Metadata* _metadata = nullptr;
            void* _resultSetHandle = nullptr;
            void* _spatialFilterHandle = nullptr;
            void* _nextHandleToQueue = nullptr;
            bool _resultSetEndReached = true;
            const std::size_t _chunkSize = 500;
            Feature::ID _idGenerator = 1;

            void init();
            void readChunk();
            friend class GDALFeatureSource;
        };
    };

#endif // ROCKY_HAS_GDAL
}
