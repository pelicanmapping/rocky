
/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "LabelSystem.h"

#if IMGUI_VERSION_NUM >= 19200 && defined(_WIN32)
#define USE_DYNAMIC_FONTS
#endif

using namespace ROCKY_NAMESPACE;


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
            ImGui::SetNextWindowPos(ImVec2{ i.position.x + style.offset.x, i.position.y + style.offset.y }, ImGuiCond_Always, ImVec2{ style.pivot.x, style.pivot.y });
            ImGui::Begin(i.uid.c_str(), nullptr, i.windowFlags);

#ifdef USE_DYNAMIC_FONTS
            // Load the font if necessary
            auto& font = styleDetail.fonts[i.viewID];
            if (font == nullptr && !styleDetail.fontName.empty())
            {
                font = getOrCreateFont(styleDetail.fontName, i.context);
            }

            ImGui::PushFont(font, style.textSize);
#endif
            ImGuiEx::TextOutlined(
                ImVec4(style.outlineColor[0], style.outlineColor[1], style.outlineColor[2], style.outlineColor[3]),
                style.outlineSize,
                label.text.c_str());

#ifdef USE_DYNAMIC_FONTS
            ImGui::PopFont();
#endif

            auto size = ImGui::GetWindowSize();

            ImGui::End();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);

            // experimental: callout lines
            if (false)
            {
                ImVec2 a{ i.position.x, i.position.y };
                ImVec2 b{ i.position.x + style.offset.x, i.position.y + style.offset.y };
                ImVec2 UL = { 0.0, 0.0 };

                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(ImVec2{ std::max(a.x, b.x), std::max(a.y, b.y) });
                auto flags = (i.windowFlags | ImGuiWindowFlags_NoBackground) & ~ImGuiWindowFlags_AlwaysAutoResize;
                ImGui::Begin((i.uid + "_callout").c_str(), nullptr, flags);
                auto* drawList = ImGui::GetWindowDrawList();
                auto calloutColor = style.borderColor.as(Color::ABGR);
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
LabelSystem::initialize(VSGContext& vsg)
{
    //nop
}

void
LabelSystem::update(VSGContext& vsg)
{
    // process any objects marked dirty
    _registry.read([&](entt::registry& reg)
        {
            Label::eachDirty(reg, [&](entt::entity e)
                {
                });

            LabelStyle::eachDirty(reg, [&](entt::entity e)
                {
                    auto& style = reg.get<LabelStyle>(e);
                    auto& styleDetail = reg.get<detail::LabelStyleDetail>(e);
                    if (styleDetail.fontName != style.fontName)
                    {
                        styleDetail.fonts.fill(nullptr);
                        styleDetail.fontName = style.fontName;
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
    //nop
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
