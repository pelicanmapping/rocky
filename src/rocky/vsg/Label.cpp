/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Label.h"
#include "json.h"

#include <vsg/vk/State.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/text/CpuLayoutTechnique.h>
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

JSON
Label::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    set(j, "text", text);
    return j.dump();
}
