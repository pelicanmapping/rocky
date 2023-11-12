/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Label.h"
#include "PixelScaleTransform.h"
#include "json.h"
#include "engine/Runtime.h"

#include <vsg/vk/State.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/text/CpuLayoutTechnique.h>
#include <vsg/text/GpuLayoutTechnique.h>
#include <vsg/text/Font.h>
#include <vsg/state/DepthStencilState.h>
#include <vsg/io/read.h>

using namespace ROCKY_NAMESPACE;

#define LABEL_MAX_NUM_CHARS 255

namespace
{
    // 2-OCT-2023 GpuLayoutTechnique leaks memory when a Label's node
    // gets destructed, so temporarily we will create a singleton and share it.
    vsg::ref_ptr<vsg::GpuLayoutTechnique> text_technique_shared;
}

Label::Label()
{
    text = "Hello, world";

    if (!text_technique_shared)
    {
        text_technique_shared = vsg::GpuLayoutTechnique::create();
    }
}

void
Label::dirty()
{
    if (node)
    {
        if (style.font != appliedStyle.font ||
            style.pointSize != appliedStyle.pointSize ||
            style.outlineSize != appliedStyle.outlineSize ||
            style.horizontalAlignment != appliedStyle.horizontalAlignment ||
            style.verticalAlignment != appliedStyle.verticalAlignment)
        {
            nodeDirty = true;
            appliedStyle = style;
        }
        else if (text != appliedText)
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(text.length() < LABEL_MAX_NUM_CHARS, void(), "Text string is too long");
            appliedText = text;
            if (valueBuffer)
            {
                valueBuffer->value() = vsg::make_string(text);
                valueBuffer->dirty();
                textNode->setup(LABEL_MAX_NUM_CHARS, options);
            }
        }
    }
}

vsg::ref_ptr< vsg::GpuLayoutTechnique> technique;

void
Label::initializeNode(const ECS::NodeComponent::Params& params)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(style.font.valid(), void());

    options = params.readerWriterOptions;

    double size = style.pointSize;

    // Billboard = false because of https://github.com/vsg-dev/VulkanSceneGraph/discussions/985
    // Workaround: use a PixelScaleTransform with unrotate=true
    layout = vsg::StandardLayout::create();
    layout->billboard = false;
    layout->billboardAutoScaleDistance = 0.0;
    layout->position = vsg::vec3(0.0, 0.0, 0.0);
    layout->horizontal = vsg::vec3(size, 0.0, 0.0);
    layout->vertical = vsg::vec3(0.0, size, 0.0); // layout->billboard ? vsg::vec3(0.0, size, 0.0) : vsg::vec3(0.0, 0.0, size);
    layout->color = vsg::vec4(1.0, 0.9, 1.0, 1.0);
    layout->outlineWidth = style.outlineSize;
    layout->horizontalAlignment = style.horizontalAlignment;
    layout->verticalAlignment = style.verticalAlignment;
    params.sharedObjects->share(layout);

    valueBuffer = vsg::stringValue::create(text);

    textNode = vsg::Text::create();
    textNode->font = style.font;
    textNode->text = valueBuffer;
    textNode->layout = layout;
    textNode->technique = text_technique_shared;
    textNode->setup(LABEL_MAX_NUM_CHARS, options); // allocate enough space for max possible characters?

#if 0
    node = textNode;
#else
    auto pst = PixelScaleTransform::create();
    pst->unrotate = true;
    pst->addChild(textNode);
    node = pst;
#endif
}

JSON
Label::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    set(j, "text", text);
    return j.dump();
}
