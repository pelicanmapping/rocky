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

Label::Label() :
    super()
{
    underGeoTransform = true;
    horizonCulling = true;
    _text = "Label!";
    textNode = vsg::Text::create();

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
}

void
Label::setText(const std::string& value)
{
    _text = value;
    if (valueBuffer)
    {
        valueBuffer->value() = _text;
        textNode->setup(0, {});
    }
}

const std::string&
Label::text() const
{
    return _text;
}

#if 0
void
Label::setFont(vsg::ref_ptr<vsg::Font> value)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(value, void());
    textNode->font = value;
}

vsg::ref_ptr<vsg::Font>
Label::font() const
{
    return textNode->font;
}
#endif

void
Label::createNode(Runtime& runtime)
{
    if (!valueBuffer)
    {
        if (!textNode->font)
        {
            textNode->font = runtime.defaultFont.value();
            ROCKY_SOFT_ASSERT_AND_RETURN(textNode->font, void(), "No font available for the Label");
        }

        // NOTE: this will (later) happen in a LabelState class and only happen once.
        // In fact we will more likely create a custom shader/shaderset for text so we can do 
        // screen-space rendering and occlusion culling.
        {
            // assign a custom StateSet to options->shaderSets so that subsequent TextGroup::setup(0, options) call will pass in our custom ShaderSet.
            auto shaderSet = runtime.readerWriterOptions->shaderSets["text"] = vsg::createTextShaderSet(runtime.readerWriterOptions);
            auto depthStencilState = vsg::DepthStencilState::create();
            depthStencilState->depthTestEnable = VK_FALSE;
            shaderSet->defaultGraphicsPipelineStates.push_back(depthStencilState);
        }

        // currently vsg::GpuLayoutTechnique is the only technique that supports dynamic update of the text parameters
        textNode->technique = vsg::GpuLayoutTechnique::create();

        valueBuffer = vsg::stringValue::create(_text);
        textNode->text = valueBuffer;
        //_textNode->font = font;
        textNode->layout = layout;
        textNode->setup(255, runtime.readerWriterOptions); // allocate enough space for max possible characters?

        auto sw = vsg::Switch::create();
        sw->addChild(true, textNode);
        node = sw;
    }
}

JSON
Label::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}
