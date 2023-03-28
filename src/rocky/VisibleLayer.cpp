/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "VisibleLayer.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;

#define LC "[VisibleLayer] \"" << getName() << "\" "

VisibleLayer::VisibleLayer() :
    super()
{
    construct(JSON());
}

VisibleLayer::VisibleLayer(const JSON& conf) :
    super(conf)
{
    construct(conf);
}

void
VisibleLayer::construct(const JSON& conf)
{
    const auto j = parse_json(conf);
    get_to(j, "visible", _visible);
    get_to(j, "opacity", _opacity);
}

JSON
VisibleLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "visible", _visible);
    set(j, "opacity", _opacity);
    return j.dump();
}
