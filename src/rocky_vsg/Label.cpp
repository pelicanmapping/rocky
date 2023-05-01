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
}

void
Label::setText(const std::string& value)
{
    _text = value;
    if (_value)
    {
        _value->value() = _text;
        _textNode->setup(0, {});
    }
}

const std::string&
Label::text() const
{
    return _text;
}

void
Label::createNode(Runtime& runtime)
{
    if (!_textNode)
    {
        auto font = runtime.defaultFont.get();
        if (!font)
        {
            Log::warn() << "No font loaded" << std::endl;
            return;
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

        auto layout = vsg::StandardLayout::create();
        _textNode = vsg::Text::create();

        // currently vsg::GpuLayoutTechnique is the only technique that supports dynamic update of the text parameters
        _textNode->technique = vsg::GpuLayoutTechnique::create();

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

        _value = vsg::stringValue::create(_text);
        _textNode->text = _value;
        _textNode->font = font;
        _textNode->layout = layout;
        _textNode->setup(255, runtime.readerWriterOptions); // allocate enough space for max possible characters?

        node = _textNode;
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
