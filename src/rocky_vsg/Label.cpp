/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Label.h"
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

Label::Label()
{
    text = "Hello, world";
}

void
Label::dirty()
{
    if (valueBuffer)
    {
        valueBuffer->value() = text;
    }

    if (textNode)
    {
        if (font.valid() && textNode->font != font)
        {
            textNode->font = font;
        }
        textNode->setup(LABEL_MAX_NUM_CHARS, options);
    }
}

void
Label::initializeNode(const ECS::NodeComponent::Params& params)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(font.valid(), void());

    options = params.readerWriterOptions;

    layout = vsg::StandardLayout::create();
    const double size = 240000.0;
    layout->billboard = true;
    layout->billboardAutoScaleDistance = size;
    layout->position = vsg::vec3(0.0, 0.0, 0.0);
    layout->horizontal = vsg::vec3(size, 0.0, 0.0);
    layout->vertical = layout->billboard ? vsg::vec3(0.0, size, 0.0) : vsg::vec3(0.0, 0.0, size);
    layout->color = vsg::vec4(1.0, 0.9, 1.0, 1.0);
    layout->outlineWidth = 0.1;
    layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
    layout->verticalAlignment = vsg::StandardLayout::BOTTOM_ALIGNMENT;

    valueBuffer = vsg::stringValue::create(text);

    textNode = vsg::Text::create();
    textNode->font = font;
    textNode->text = valueBuffer;
    textNode->layout = layout;
    textNode->technique = vsg::GpuLayoutTechnique::create();
    textNode->setup(LABEL_MAX_NUM_CHARS, options); // allocate enough space for max possible characters?

    node = textNode;
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
