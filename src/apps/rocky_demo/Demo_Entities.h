/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <unordered_set>

using namespace ROCKY_NAMESPACE;

namespace
{
    std::string entityLabel(entt::entity entity)
    {
        if (entity == entt::null)
            return "null";

        std::stringstream buf;
        buf << entt::to_entity(entity);
        return buf.str();
    }

    std::string componentName(std::string_view raw)
    {
        std::string name(raw);

        for (auto prefix : { "struct ", "class ", "enum " })
        {
            auto pos = name.find(prefix);
            while (pos != std::string::npos)
            {
                name.erase(pos, std::strlen(prefix));
                pos = name.find(prefix);
            }
        }

        auto pos = name.rfind("::");
        if (pos != std::string::npos)
            name = name.substr(pos + 2);

        return name.empty() ? "(unnamed)" : name;
    }

    template<typename STORAGE>
    std::string componentName(const STORAGE& storage)
    {
        auto name = componentName(storage.info().name());
        if (name != "(unnamed)")
            return name;

        std::stringstream buf;
        buf << "0x" << std::hex << storage.info().hash();
        return buf.str();
    }

    std::string boolText(bool value)
    {
        return value ? "true" : "false";
    }

    std::string entityText(entt::entity entity)
    {
        return entity == entt::null ? std::string("null") : entityLabel(entity);
    }

    std::string colorText(const Color& value)
    {
        std::stringstream buf;
        buf << value.toHTML() << "  rgba("
            << value.r << ", " << value.g << ", " << value.b << ", " << value.a << ")";
        return buf.str();
    }

    std::string vecText(const glm::fvec2& value)
    {
        std::stringstream buf;
        buf << "(" << value.x << ", " << value.y << ")";
        return buf.str();
    }

    std::string vecText(const glm::ivec2& value)
    {
        std::stringstream buf;
        buf << "(" << value.x << ", " << value.y << ")";
        return buf.str();
    }

    std::string vecText(const glm::dvec3& value)
    {
        std::stringstream buf;
        buf << "(" << value.x << ", " << value.y << ", " << value.z << ")";
        return buf.str();
    }

    std::string vecText(const glm::fvec3& value)
    {
        std::stringstream buf;
        buf << "(" << value.x << ", " << value.y << ", " << value.z << ")";
        return buf.str();
    }

    std::string vecText(const glm::fvec4& value)
    {
        std::stringstream buf;
        buf << "(" << value.x << ", " << value.y << ", " << value.z << ", " << value.w << ")";
        return buf.str();
    }

    std::string matrixText(const glm::dmat4& value)
    {
        std::stringstream buf;
        for (int row = 0; row < 4; ++row)
        {
            if (row > 0)
                buf << "\n";
            buf << "[";
            for (int col = 0; col < 4; ++col)
            {
                if (col > 0)
                    buf << ", ";
                buf << value[col][row];
            }
            buf << "]";
        }
        return buf.str();
    }

    std::string srsText(const SRS& srs)
    {
        return srs.valid() ? srs.string() : std::string("(invalid)");
    }

    std::string vectorSizeText(std::size_t size)
    {
        std::stringstream buf;
        buf << size << (size == 1 ? " item" : " items");
        return buf.str();
    }

    template<typename T>
    std::string vectorPreviewText(const std::vector<T>& values, std::function<std::string(const T&)> formatter)
    {
        std::stringstream buf;
        buf << vectorSizeText(values.size());

        const std::size_t limit = std::min<std::size_t>(values.size(), 4);
        if (limit > 0)
        {
            buf << "  [";
            for (std::size_t i = 0; i < limit; ++i)
            {
                if (i > 0)
                    buf << ", ";
                buf << formatter(values[i]);
            }
            if (values.size() > limit)
                buf << ", ...";
            buf << "]";
        }

        return buf.str();
    }

    void propertyRow(const char* name, const std::string& value)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(name);
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", value.c_str());
    }

    template<typename T>
    void propertyRow(const char* name, T value)
    {
        std::stringstream buf;
        buf << value;
        propertyRow(name, buf.str());
    }

    void propertyLabel(const char* name)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(name);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
    }

    std::string imguiID(const char* name)
    {
        return "##" + std::string(name);
    }

    template<typename T>
    void clampValue(T& value, T minValue, T maxValue)
    {
        value = std::max(minValue, std::min(maxValue, value));
    }

    bool editBoolRow(const char* name, bool& value)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        return ImGui::Checkbox(id.c_str(), &value);
    }

    bool editFloatRow(
        const char* name,
        float& value,
        float step = 0.1f,
        const char* format = "%.3f",
        bool clamp = false,
        float minValue = 0.0f,
        float maxValue = 0.0f)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        if (ImGui::InputFloat(id.c_str(), &value, step, step * 10.0f, format))
        {
            if (clamp)
                clampValue(value, minValue, maxValue);
            return true;
        }
        return false;
    }

    bool editDoubleRow(
        const char* name,
        double& value,
        double step = 0.1,
        const char* format = "%.3lf",
        bool clamp = false,
        double minValue = 0.0,
        double maxValue = 0.0)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        if (ImGui::InputDouble(id.c_str(), &value, step, step * 10.0, format))
        {
            if (clamp)
                clampValue(value, minValue, maxValue);
            return true;
        }
        return false;
    }

    bool editIntRow(
        const char* name,
        int& value,
        int step = 1,
        bool clamp = false,
        int minValue = 0,
        int maxValue = 0)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        if (ImGui::InputInt(id.c_str(), &value, step, step * 10))
        {
            if (clamp)
                clampValue(value, minValue, maxValue);
            return true;
        }
        return false;
    }

    bool editUInt16Row(const char* name, std::uint16_t& value)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        unsigned temp = value;
        if (ImGui::InputScalar(id.c_str(), ImGuiDataType_U32, &temp, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal))
        {
            value = static_cast<std::uint16_t>(std::min<unsigned>(temp, 0xFFFF));
            return true;
        }
        return false;
    }

    bool editUInt32Row(const char* name, std::uint32_t& value)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        return ImGui::InputScalar(id.c_str(), ImGuiDataType_U32, &value, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    }

    bool editStringRow(const char* name, std::string& value)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        return ImGui::InputText(id.c_str(), &value);
    }

    bool editColorRow(const char* name, Color& value)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        return ImGui::ColorEdit4(id.c_str(), &value[0]);
    }

    bool editVec2Row(
        const char* name,
        glm::fvec2& value,
        bool clamp = false,
        float minValue = 0.0f,
        float maxValue = 0.0f)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        if (ImGui::InputFloat2(id.c_str(), &value[0], "%.3f"))
        {
            if (clamp)
            {
                clampValue(value.x, minValue, maxValue);
                clampValue(value.y, minValue, maxValue);
            }
            return true;
        }
        return false;
    }

    bool editVec2Row(const char* name, glm::ivec2& value)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        return ImGui::InputInt2(id.c_str(), &value[0]);
    }

    bool editLineTopologyRow(const char* name, LineTopology& value)
    {
        propertyLabel(name);
        auto id = imguiID(name);
        const char* items[] = { "Strip", "Segments" };
        int current = value == LineTopology::Strip ? 0 : 1;
        if (ImGui::Combo(id.c_str(), &current, items, 2))
        {
            value = current == 0 ? LineTopology::Strip : LineTopology::Segments;
            return true;
        }
        return false;
    }

    bool beginProperties(const char* id)
    {
        if (ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            return true;
        }

        return false;
    }

    void endProperties()
    {
        ImGui::EndTable();
    }

    bool componentHeader(const char* name)
    {
        return ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen);
    }

    void componentFooter()
    {
        ImGui::TreePop();
    }

    template<typename T>
    void markInspected(std::unordered_set<entt::id_type>& inspected)
    {
        inspected.emplace(entt::type_hash<T>::value());
    }

    template<typename T>
    void markDirty(entt::registry& registry, T& component)
    {
        component.dirty(registry);
    }

    void markDirty(entt::registry&, ActiveState&) { }
    void markDirty(entt::registry&, Declutter&) { }
    void markDirty(entt::registry&, PixelScale&) { }
    void markDirty(entt::registry&, Visibility&) { }

    void renderVec3Table(const char* name, const std::vector<glm::dvec3>& values)
    {
        if (values.empty())
            return;

        if (ImGui::TreeNode(name))
        {
            if (ImGui::BeginTable(name, 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 45.0f);
                ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (std::size_t i = 0; i < values.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", i);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.8g", values[i].x);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.8g", values[i].y);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.8g", values[i].z);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    void renderFloatVec3Table(const char* name, const std::vector<glm::fvec3>& values)
    {
        if (values.empty())
            return;

        if (ImGui::TreeNode(name))
        {
            if (ImGui::BeginTable(name, 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 45.0f);
                ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (std::size_t i = 0; i < values.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", i);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.6g", values[i].x);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.6g", values[i].y);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.6g", values[i].z);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    void renderVec2Table(const char* name, const std::vector<glm::fvec2>& values)
    {
        if (values.empty())
            return;

        if (ImGui::TreeNode(name))
        {
            if (ImGui::BeginTable(name, 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 45.0f);
                ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (std::size_t i = 0; i < values.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", i);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.6g", values[i].x);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.6g", values[i].y);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    void renderColorTable(const char* name, const std::vector<Color>& values)
    {
        if (values.empty())
            return;

        if (ImGui::TreeNode(name))
        {
            if (ImGui::BeginTable(name, 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 45.0f);
                ImGui::TableSetupColumn("R", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("A", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (std::size_t i = 0; i < values.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", i);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", values[i].r);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", values[i].g);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", values[i].b);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", values[i].a);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    void inspectLineStyle(LineStyle& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("LineStyle properties"))
        {
            propertyRow("owner", entityText(value.owner));
            changed |= editColorRow("color", value.color);
            changed |= editFloatRow("width", value.width, 0.1f, "%.1f", true, 0.0f, 1000.0f);
            changed |= editUInt16Row("stipplePattern", value.stipplePattern);
            changed |= editIntRow("stippleFactor", value.stippleFactor, 1, true, 1, 64);
            changed |= editFloatRow("resolution", value.resolution, 1000.0f, "%.0f", true, 0.0f, 1000000000.0f);
            changed |= editFloatRow("depthOffset", value.depthOffset, 1.0f, "%.1f");
            changed |= editBoolRow("useGeometryColors", value.useGeometryColors);
            changed |= editBoolRow("transparencyBin", value.transparencyBin);
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectLineGeometry(LineGeometry& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("LineGeometry properties"))
        {
            propertyRow("owner", entityText(value.owner));
            changed |= editLineTopologyRow("topology", value.topology);
            propertyRow("srs", srsText(value.srs));
            propertyRow("points", vectorSizeText(value.points.size()));
            propertyRow("colors", vectorSizeText(value.colors.size()));
            endProperties();
        }
        renderVec3Table("points", value.points);
        renderColorTable("colors", value.colors);
        if (changed)
            markDirty(registry, value);
    }

    void inspectLine(Line& value, entt::registry&)
    {
        if (beginProperties("Line properties"))
        {
            propertyRow("owner", entityText(value.owner));
            propertyRow("style", entityText(value.style));
            propertyRow("geometry", entityText(value.geometry));
            endProperties();
        }
    }

    void inspectMeshGeometry(MeshGeometry& value, entt::registry&)
    {
        if (beginProperties("MeshGeometry properties"))
        {
            propertyRow("owner", entityText(value.owner));
            propertyRow("vertices", vectorSizeText(value.vertices.size()));
            propertyRow("colors", vectorSizeText(value.colors.size()));
            propertyRow("normals", vectorSizeText(value.normals.size()));
            propertyRow("uvs", vectorSizeText(value.uvs.size()));
            propertyRow("indices", vectorSizeText(value.indices.size()));
            propertyRow("srs", srsText(value.srs));
            endProperties();
        }
        renderVec3Table("vertices", value.vertices);
        renderFloatVec3Table("normals", value.normals);
        renderVec2Table("uvs", value.uvs);
    }

    void inspectMeshStyle(MeshStyle& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("MeshStyle properties"))
        {
            propertyRow("owner", entityText(value.owner));
            changed |= editColorRow("color", value.color);
            changed |= editBoolRow("useGeometryColors", value.useGeometryColors);
            changed |= editFloatRow("depthOffset", value.depthOffset, 1.0f, "%.1f");
            propertyRow("texture", entityText(value.texture));
            changed |= editBoolRow("wireframe", value.wireframe);
            changed |= editBoolRow("lighting", value.lighting);
            changed |= editUInt32Row("stipplePattern", value.stipplePattern);
            changed |= editBoolRow("writeDepth", value.writeDepth);
            changed |= editBoolRow("drawBackfaces", value.drawBackfaces);
            changed |= editBoolRow("twoPassAlpha", value.twoPassAlpha);
            changed |= editBoolRow("transparencyBin", value.transparencyBin);
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectMesh(Mesh& value, entt::registry&)
    {
        if (beginProperties("Mesh properties"))
        {
            propertyRow("owner", entityText(value.owner));
            propertyRow("geometry", entityText(value.geometry));
            propertyRow("style", entityText(value.style));
            endProperties();
        }
    }

    void inspectPointStyle(PointStyle& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("PointStyle properties"))
        {
            propertyRow("owner", entityText(value.owner));
            changed |= editColorRow("color", value.color);
            changed |= editFloatRow("width", value.width, 0.1f, "%.1f", true, 0.0f, 1000.0f);
            changed |= editFloatRow("antialias", value.antialias, 0.01f, "%.2f", true, 0.0f, 1.0f);
            changed |= editFloatRow("depthOffset", value.depthOffset, 1.0f, "%.1f");
            changed |= editBoolRow("useGeometryColors", value.useGeometryColors);
            changed |= editBoolRow("useGeometryWidths", value.useGeometryWidths);
            changed |= editBoolRow("transparencyBin", value.transparencyBin);
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectPointGeometry(PointGeometry& value, entt::registry&)
    {
        if (beginProperties("PointGeometry properties"))
        {
            propertyRow("owner", entityText(value.owner));
            propertyRow("srs", srsText(value.srs));
            propertyRow("points", vectorSizeText(value.points.size()));
            propertyRow("colors", vectorSizeText(value.colors.size()));
            propertyRow("widths", vectorSizeText(value.widths.size()));
            endProperties();
        }
        renderVec3Table("points", value.points);
        renderColorTable("colors", value.colors);
    }

    void inspectPoint(Point& value, entt::registry&)
    {
        if (beginProperties("Point properties"))
        {
            propertyRow("owner", entityText(value.owner));
            propertyRow("style", entityText(value.style));
            propertyRow("geometry", entityText(value.geometry));
            endProperties();
        }
    }

    void inspectLabelStyle(LabelStyle& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("LabelStyle properties"))
        {
            propertyRow("owner", entityText(value.owner));
            changed |= editStringRow("fontName", value.fontName);
            changed |= editColorRow("textColor", value.textColor);
            changed |= editFloatRow("textSize", value.textSize, 0.5f, "%.1f", true, 0.0f, 512.0f);
            changed |= editFloatRow("textOutlineSize", value.textOutlineSize, 0.1f, "%.1f", true, 0.0f, 100.0f);
            changed |= editColorRow("textOutlineColor", value.textOutlineColor);
            changed |= editVec2Row("textPivot", value.textPivot, true, 0.0f, 1.0f);
            changed |= editVec2Row("textOffset", value.textOffset);
            propertyRow("icon", value.icon ? "set" : "null");
            changed |= editFloatRow("iconSizePixels", value.iconSizePixels, 0.5f, "%.1f", true, 0.0f, 4096.0f);
            changed |= editFloatRow("iconRotationDegrees", value.iconRotationDegrees, 1.0f, "%.1f");
            changed |= editVec2Row("iconPivot", value.iconPivot, true, 0.0f, 1.0f);
            changed |= editFloatRow("borderSize", value.borderSize, 0.1f, "%.1f", true, 0.0f, 100.0f);
            changed |= editColorRow("borderColor", value.borderColor);
            changed |= editColorRow("backgroundColor", value.backgroundColor);
            changed |= editVec2Row("padding", value.padding);
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectLabel(Label& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("Label properties"))
        {
            propertyRow("owner", entityText(value.owner));
            changed |= editStringRow("text", value.text);
            propertyRow("style", entityText(value.style));
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectTransform(Transform& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("Transform properties"))
        {
            propertyRow("owner", entityText(value.owner));
            propertyRow("position.srs", srsText(value.position.srs));
            if (value.position.valid())
            {
                bool geodetic = value.position.srs.isGeodetic();
                changed |= editDoubleRow("position.x", value.position.x, 0.1, "%.8lf", geodetic, -180.0, 180.0);
                changed |= editDoubleRow("position.y", value.position.y, 0.1, "%.8lf", geodetic, -90.0, 90.0);
                changed |= editDoubleRow("position.z", value.position.z, 1.0f, "%.3lf");
            }
            else
            {
                propertyRow("position", "(invalid)");
            }
            propertyRow("localMatrix", matrixText(value.localMatrix));
            changed |= editDoubleRow("radius", value.radius, 1.0, "%.3lf", true, 0.0, 1000000000000.0);
            changed |= editBoolRow("topocentric", value.topocentric);
            changed |= editBoolRow("horizonCulled", value.horizonCulled);
            changed |= editBoolRow("frustumCulled", value.frustumCulled);
            propertyRow("revision", value.revision);
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectVisibility(Visibility& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("Visibility properties"))
        {
            std::stringstream frame;
            for (std::size_t i = 0; i < value.visible.size(); ++i)
            {
                if (i > 0)
                {
                    frame << ", ";
                }
                frame << i << ":" << value.frame[i];
            }
            for (std::size_t i = 0; i < value.visible.size(); ++i)
            {
                auto label = "visible[" + std::to_string(i) + "]";
                changed |= editBoolRow(label.c_str(), value.visible[i]);
            }
            propertyRow("frame", frame.str());
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectActiveState(ActiveState& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("ActiveState properties"))
        {
            changed |= editBoolRow("active", value.active);
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectDeclutter(Declutter& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("Declutter properties"))
        {
            changed |= editFloatRow("priority", value.priority, 0.1f, "%.3f");
            changed |= editDoubleRow("rect.xmin", value.rect.xmin);
            changed |= editDoubleRow("rect.ymin", value.rect.ymin);
            changed |= editDoubleRow("rect.xmax", value.rect.xmax);
            changed |= editDoubleRow("rect.ymax", value.rect.ymax);
            propertyRow("rect.width", value.rect.width());
            propertyRow("rect.height", value.rect.height());
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

    void inspectPixelScale(PixelScale& value, entt::registry& registry)
    {
        bool changed = false;
        if (beginProperties("PixelScale properties"))
        {
            changed |= editBoolRow("enabled", value.enabled);
            changed |= editFloatRow("minPixels", value.minPixels, 0.5f, "%.1f", true, 0.0f, 1000000.0f);
            changed |= editFloatRow("maxPixels", value.maxPixels, 0.5f, "%.1f", true, value.minPixels, 1000000.0f);
            endProperties();
        }
        if (changed)
            markDirty(registry, value);
    }

#ifdef ROCKY_HAS_IMGUI
    void inspectWidget(Widget& value, entt::registry&)
    {
        if (beginProperties("Widget properties"))
        {
            propertyRow("render", value.render ? "set" : "null");
            endProperties();
        }
    }
#endif

    void inspectNodeGraph(NodeGraph& value, entt::registry&)
    {
        if (beginProperties("NodeGraph properties"))
        {
            propertyRow("owner", entityText(value.owner));
            propertyRow("node", value.node ? std::string(value.node->className()) : std::string("null"));
            endProperties();
        }
    }

    void inspectMeshTexture(MeshTexture& value, entt::registry&)
    {
        if (beginProperties("MeshTexture properties"))
        {
            propertyRow("owner", entityText(value.owner));
            propertyRow("imageInfo", value.imageInfo ? "set" : "null");
            endProperties();
        }
    }

    template<typename T, typename INSPECTOR>
    void inspectIfPresent(entt::registry& registry, entt::entity entity, const char* name, INSPECTOR&& inspector, std::unordered_set<entt::id_type>& inspected)
    {
        markInspected<T>(inspected);

        if (auto* component = registry.try_get<T>(entity))
        {
            if (componentHeader(name))
            {
                inspector(*component, registry);
                componentFooter();
            }
        }
    }

    struct EntityInfo
    {
        entt::entity entity = entt::null;
        std::vector<std::string> components;
    };

    std::vector<std::string> componentsFor(entt::registry& registry, entt::entity entity)
    {
        std::vector<std::string> components;
        for (auto&& [id, storage] : registry.storage())
        {
            if (id != entt::type_hash<entt::entity>::value() && storage.contains(entity))
                components.emplace_back(componentName(storage));
        }

        std::sort(components.begin(), components.end());
        return components;
    }

    std::vector<EntityInfo> collectEntities(entt::registry& registry)
    {
        std::vector<EntityInfo> entities;
        for (auto [entity] : registry.storage<entt::entity>().each())
        {
            if (registry.valid(entity))
                entities.push_back({ entity, componentsFor(registry, entity) });
        }

        std::sort(entities.begin(), entities.end(), [](auto& lhs, auto& rhs)
            {
                return entt::to_entity(lhs.entity) < entt::to_entity(rhs.entity);
            });

        return entities;
    }

}

auto Demo_Entities = [](Application& app)
{
#ifdef ROCKY_HAS_IMGUI

    static entt::entity selected = entt::null;

    app.registry.read([&](entt::registry& registry)
        {
            auto entities = collectEntities(registry);

            if (selected != entt::null && !registry.valid(selected))
                selected = entt::null;

            ImGui::Text("Entities: %zu", entities.size());
            auto tableFlags =
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("Entities", 2, tableFlags, ImVec2(0.0f, 220.0f)))
            {
                ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Components", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                for (auto& info : entities)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    auto label = entityLabel(info.entity) + "##entity";
                    if (ImGui::Selectable(label.c_str(), selected == info.entity, ImGuiSelectableFlags_SpanAllColumns))
                        selected = info.entity;

                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", info.components.size());
                }

                ImGui::EndTable();
            }

            ImGui::SeparatorText("Selected entity");

            ImGui::BeginChild("Component inspector", ImVec2(0.0f, 420.0f), true);
            if (selected == entt::null)
            {
                ImGui::TextUnformatted("No entity selected");
            }
            else if (!registry.valid(selected))
            {
                ImGui::TextUnformatted("Selected entity no longer exists");
            }
            else
            {
                auto components = componentsFor(registry, selected);
                ImGui::Text("Entity %s", entityLabel(selected).c_str());
                ImGui::Text("Components: %zu", components.size());

                std::unordered_set<entt::id_type> inspected;

                inspectIfPresent<LineStyle>(registry, selected, "LineStyle", inspectLineStyle, inspected);
                inspectIfPresent<LineGeometry>(registry, selected, "LineGeometry", inspectLineGeometry, inspected);
                inspectIfPresent<Line>(registry, selected, "Line", inspectLine, inspected);
                inspectIfPresent<MeshGeometry>(registry, selected, "MeshGeometry", inspectMeshGeometry, inspected);
                inspectIfPresent<MeshStyle>(registry, selected, "MeshStyle", inspectMeshStyle, inspected);
                inspectIfPresent<Mesh>(registry, selected, "Mesh", inspectMesh, inspected);
                inspectIfPresent<PointStyle>(registry, selected, "PointStyle", inspectPointStyle, inspected);
                inspectIfPresent<PointGeometry>(registry, selected, "PointGeometry", inspectPointGeometry, inspected);
                inspectIfPresent<Point>(registry, selected, "Point", inspectPoint, inspected);
                inspectIfPresent<LabelStyle>(registry, selected, "LabelStyle", inspectLabelStyle, inspected);
                inspectIfPresent<Label>(registry, selected, "Label", inspectLabel, inspected);
                inspectIfPresent<Transform>(registry, selected, "Transform", inspectTransform, inspected);
                inspectIfPresent<Visibility>(registry, selected, "Visibility", inspectVisibility, inspected);
                inspectIfPresent<ActiveState>(registry, selected, "ActiveState", inspectActiveState, inspected);
                inspectIfPresent<Declutter>(registry, selected, "Declutter", inspectDeclutter, inspected);
                inspectIfPresent<PixelScale>(registry, selected, "PixelScale", inspectPixelScale, inspected);
#ifdef ROCKY_HAS_IMGUI
                inspectIfPresent<Widget>(registry, selected, "Widget", inspectWidget, inspected);
#endif
                inspectIfPresent<NodeGraph>(registry, selected, "NodeGraph", inspectNodeGraph, inspected);
                inspectIfPresent<MeshTexture>(registry, selected, "MeshTexture", inspectMeshTexture, inspected);

                for (auto&& [id, storage] : registry.storage())
                {
                    if (id == entt::type_hash<entt::entity>::value() || !storage.contains(selected) || inspected.count(id) != 0)
                        continue;

                    auto name = componentName(storage);
                    if (componentHeader(name.c_str()))
                    {
                        if (beginProperties("Unknown component properties"))
                        {
                            propertyRow("type", name);
                            propertyRow("hash", storage.info().hash());
                            propertyRow("storageSize", storage.size());
                            propertyRow("properties", "No read-only property viewer is registered for this component type");
                            endProperties();
                        }
                        componentFooter();
                    }
                }
            }
            ImGui::EndChild();
        });

#endif
};
