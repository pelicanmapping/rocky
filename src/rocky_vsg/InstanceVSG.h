/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Common.h>
#include <rocky/Instance.h>

namespace rocky
{
    class ROCKY_VSG_EXPORT InstanceVSG : public Inherit<Instance, InstanceVSG>
    {
    public:
        InstanceVSG();
    };
}
