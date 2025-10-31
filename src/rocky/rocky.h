/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Log.h>
#include <rocky/GDALImageLayer.h>
#include <rocky/GDALElevationLayer.h>
#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>
#include <rocky/MBTilesImageLayer.h>
#include <rocky/MBTilesElevationLayer.h>
#include <rocky/AzureImageLayer.h>
#include <rocky/GDALFeatureSource.h>
#include <rocky/contrib/EarthFileImporter.h>
#include <rocky/ECS.h>

#ifdef ROCKY_HAS_VSG
#include <rocky/vsg/Application.h>
#include <rocky/vsg/NodeLayer.h>
#include <rocky/vsg/NodePager.h>
#include <rocky/vsg/GeoTransform.h>
#include <rocky/vsg/ecs/FeatureView.h>
#include <rocky/vsg/ecs/EntityNode.h>
#include <rocky/vsg/ecs/ECSTypes.h>
#include <rocky/vsg/ecs/ECSVisitors.h>
#include <rocky/vsg/ecs/WidgetSystem.h>
#endif

// NOTE: do NOT add any imgui-related includes here