
/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "LabelSystem.h"

#include <algorithm>
#include <cfloat>

#if IMGUI_VERSION_NUM >= 19200
#define USE_DYNAMIC_FONTS
#endif

using namespace ROCKY_NAMESPACE;

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        struct LabelDetail
        {
        };

        struct LabelStyleDetail
        {
            std::string fontName;
            ViewLocal<ImFont*> fonts;
            ImGuiImage iconImage;

            LabelStyleDetail() {
                fonts.fill(nullptr);
            }
        };
    }
}

LabelSystem::LabelSystem(Registry& registry) :
    Inherit(registry)
{
    // configure EnTT to automatically add the necessary components when a Widget is constructed
    registry.write([&](entt::registry& reg)
        {
            reg.on_construct<Label>().connect<&LabelSystem::on_construct_Label>(*this);
            reg.on_destroy<Label>().connect<&LabelSystem::on_destroy_Label>(*this);

            reg.on_construct<LabelStyle>().connect<&LabelSystem::on_construct_LabelStyle>(*this);
            reg.on_destroy<LabelStyle>().connect<&LabelSystem::on_destroy_LabelStyle>(*this);

            auto e = reg.create();
            reg.emplace<Label::Dirty>(e);
            reg.emplace<LabelStyle::Dirty>(e);

            // a default style for labels that don't have one
            _defaultStyleEntity = reg.create();
            reg.emplace<LabelStyle>(_defaultStyleEntity);
        });

    _renderFunction = [&](WidgetInstance& i)
        {
            auto& label = i.registry.get<Label>(i.entity);

            auto&& [style, styleDetail] = i.registry.get<LabelStyle, detail::LabelStyleDetail>(
                label.style != entt::null ? label.style : _defaultStyleEntity);

            ImGui::SetCurrentContext(i.context);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.borderSize);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ style.padding.x, style.padding.y });
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(style.borderColor[0], style.borderColor[1], style.borderColor[2], style.borderColor[3]));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(style.backgroundColor[0], style.backgroundColor[1], style.backgroundColor[2], style.backgroundColor[3]));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(style.textColor[0], style.textColor[1], style.textColor[2], style.textColor[3]));

#ifdef USE_DYNAMIC_FONTS
            // Load the font if necessary
            auto& font = styleDetail.fonts[i.view.viewID];
            if (font == nullptr && !styleDetail.fontName.empty())
            {
                font = getOrCreateFont(styleDetail.fontName, i.context);
            }

            ImGui::PushFont(font, style.textSize);
#endif

            // Layout is in screen coordinates, with "anchor" being the point that
            // must remain fixed. When there is an icon, anchor is its iconPivot.
            const ImVec2 anchor{ i.position.x, i.position.y };
            const ImVec2 padding{ style.padding.x, style.padding.y };

            const bool hasText = !label.text.empty();
            const bool hasIcon = styleDetail.iconImage && style.iconSizePixels > 0.0f;

            ImVec2 textDrawPos = anchor;
            ImVec2 iconDrawPos = anchor;
            ImVec2 rectMin{ FLT_MAX, FLT_MAX };
            ImVec2 rectMax{ -FLT_MAX, -FLT_MAX };

            auto expand = [&](const ImVec2& p)
                {
                    rectMin.x = std::min(rectMin.x, p.x);
                    rectMin.y = std::min(rectMin.y, p.y);
                    rectMax.x = std::max(rectMax.x, p.x);
                    rectMax.y = std::max(rectMax.y, p.y);
                };

            if (hasIcon)
            {
                const float size = style.iconSizePixels;
                const float cos_a = cosf(glm::radians(style.iconRotationDegrees));
                const float sin_a = sinf(glm::radians(style.iconRotationDegrees));
                const ImVec2 half{ size * 0.5f, size * 0.5f };
                const ImVec2 pivot{
                    size * style.iconPivot.x,
                    size * style.iconPivot.y };
                const ImVec2 pivotFromCenter{
                    pivot.x - half.x,
                    pivot.y - half.y };
                const ImVec2 rotatedPivotFromCenter{
                    pivotFromCenter.x * cos_a - pivotFromCenter.y * sin_a,
                    pivotFromCenter.x * sin_a + pivotFromCenter.y * cos_a };
                const ImVec2 center{
                    anchor.x - rotatedPivotFromCenter.x,
                    anchor.y - rotatedPivotFromCenter.y };

                iconDrawPos = ImVec2{
                    center.x - half.x,
                    center.y - half.y };

                const ImVec2 corners[4] = {
                    ImVec2(-half.x, -half.y),
                    ImVec2(half.x, -half.y),
                    ImVec2(half.x, half.y),
                    ImVec2(-half.x, half.y)
                };

                for (auto& corner : corners)
                {
                    expand(ImVec2{
                        center.x + corner.x * cos_a - corner.y * sin_a,
                        center.y + corner.x * sin_a + corner.y * cos_a });
                }
            }

            if (hasText)
            {
                ImFont* imguiFont = ImGui::GetFont();
                const float fontSize = ImGui::GetFontSize();
                const ImVec2 textSize = imguiFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.text.c_str());
                const float outlineSize = std::max(0.0f, style.textOutlineSize);
                const ImVec2 labelSize{
                    textSize.x + 2.0f * outlineSize,
                    textSize.y + 2.0f * outlineSize };
                const ImVec2 labelPivotPos{
                    anchor.x + static_cast<float>(style.textOffset.x),
                    anchor.y + static_cast<float>(style.textOffset.y) };
                const ImVec2 labelMin{
                    labelPivotPos.x - labelSize.x * style.textPivot.x,
                    labelPivotPos.y - labelSize.y * style.textPivot.y };

                textDrawPos = ImVec2{
                    labelMin.x + outlineSize,
                    labelMin.y + outlineSize };

                expand(labelMin);
                expand(ImVec2{ labelMin.x + labelSize.x, labelMin.y + labelSize.y });
            }

            if (!hasText && !hasIcon)
            {
                expand(anchor);
                expand(anchor);
            }

            const ImVec2 windowPos{
                rectMin.x - padding.x,
                rectMin.y - padding.y };
            const ImVec2 windowSize{
                std::max(1.0f, rectMax.x - rectMin.x + 2.0f * padding.x),
                std::max(1.0f, rectMax.y - rectMin.y + 2.0f * padding.y) };

            ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
            ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
            auto windowFlags = i.windowFlags & ~ImGuiWindowFlags_AlwaysAutoResize;
            ImGui::Begin(i.uid.c_str(), nullptr, windowFlags);

            if (hasIcon)
            {
                ImGui::SetCursorScreenPos(iconDrawPos);
                styleDetail.iconImage.render(ImVec2{ style.iconSizePixels, style.iconSizePixels }, style.iconRotationDegrees);
            }

            if (hasText)
            {
                ImGui::SetCursorScreenPos(textDrawPos);
                ImGuiEx::TextOutlined(
                    ImVec4(style.textOutlineColor[0], style.textOutlineColor[1], style.textOutlineColor[2], style.textOutlineColor[3]),
                    style.textOutlineSize,
                    label.text.c_str());
            }

#ifdef USE_DYNAMIC_FONTS
            ImGui::PopFont();
#endif

            auto size = ImGui::GetWindowSize();

            i.checkFocus();

            ImGui::End();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);

            // experimental: callout lines
            if (false)
            {
                ImVec2 a{ i.position.x, i.position.y };
                ImVec2 b{ i.position.x + style.textOffset.x, i.position.y + style.textOffset.y };
                ImVec2 UL = { 0.0, 0.0 };

                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(ImVec2{ std::max(a.x, b.x), std::max(a.y, b.y) });
                auto flags = (i.windowFlags | ImGuiWindowFlags_NoBackground) & ~ImGuiWindowFlags_AlwaysAutoResize;
                ImGui::Begin((i.uid + "_callout").c_str(), nullptr, flags);
                auto* drawList = ImGui::GetWindowDrawList();
                auto calloutColor = style.borderColor.as(Color::Format::ABGR);
                ImVec2 start{ a.x - UL.x, a.y - UL.y };
                ImVec2 end{ b.x - UL.x, b.y - UL.y };
                drawList->AddLine(start, end, calloutColor, style.borderSize);
                ImGui::End();
            }

            // update a decluttering record to reflect our widget's size
            if (auto* dc = i.registry.try_get<Declutter>(i.entity))
            {
                dc->rect = Rect(size.x, size.y);
            }
        };
}

void
LabelSystem::initialize(VSGContext vsg)
{
    //nop
}

void
LabelSystem::update(VSGContext vsg)
{
    // process any objects marked dirty
    _registry.read([&](entt::registry& reg)
        {
            Label::eachDirty(reg, [&](entt::entity e)
                {
                    // nop
                });

            LabelStyle::eachDirty(reg, [&](entt::entity e)
                {
                    auto&& [style, styleDetail] = reg.get<LabelStyle, detail::LabelStyleDetail>(e);

                    if (styleDetail.fontName != style.fontName)
                    {
                        styleDetail.fonts.fill(nullptr);
                        styleDetail.fontName = style.fontName;
                    }

                    if (style.icon && !styleDetail.iconImage)
                    {
                        styleDetail.iconImage = ImGuiImage(style.icon, vsg);
                    }
                    else if (!style.icon && styleDetail.iconImage)
                    {
                        styleDetail.iconImage = ImGuiImage();
                    }
                });
        });
}

ImFont*
LabelSystem::getOrCreateFont(const std::string& fontName, ImGuiContext* imgc)
{
    auto& fonts = _fontsCache[imgc];
    auto& font = fonts[fontName].font;
    if (font == nullptr)
    {
#ifdef USE_DYNAMIC_FONTS
        font = ImGui::GetIO().Fonts->AddFontFromFileTTF(fontName.c_str());
#endif
    }
    return font;
}

void
LabelSystem::on_construct_Label(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);

    r.emplace<detail::LabelDetail>(e);

    Label::dirty(r, e);

    if (r.all_of<Widget>(e))
    {
        Log()->warn("LabelSystem: you added a Label to an entity already containing a Widget; stealing your Widget!");
    }

    auto& widget = r.get_or_emplace<Widget>(e);
    widget.render = _renderFunction;
}

void
LabelSystem::on_update_Label(entt::registry& r, entt::entity e)
{
    Label::dirty(r, e);
}

void
LabelSystem::on_destroy_Label(entt::registry& r, entt::entity e)
{
    r.remove<detail::LabelDetail>(e);
}

void
LabelSystem::on_construct_LabelStyle(entt::registry& r, entt::entity e)
{
    r.emplace<detail::LabelStyleDetail>(e);
    LabelStyle::dirty(r, e);
}

void
LabelSystem::on_update_LabelStyle(entt::registry& r, entt::entity e)
{
    LabelStyle::dirty(r, e);
}

void
LabelSystem::on_destroy_LabelStyle(entt::registry& r, entt::entity e)
{
    r.remove<detail::LabelStyleDetail>(e);
}
