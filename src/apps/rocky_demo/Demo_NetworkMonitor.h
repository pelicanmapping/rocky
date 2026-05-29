/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/NetworkMonitor.h>
#include "helpers.h"

#include <algorithm>
#include <array>
#include <optional>
#include <set>

using namespace ROCKY_NAMESPACE;

namespace
{
    enum class DemoNetworkRequestType
    {
        Network = 0,
        File,
        Cache,
        Count
    };

    enum class DemoNetworkRequestStatus
    {
        OK = 0,
        Cache,
        NotFound,
        Error,
        Blacklisted,
        Canceled,
        Pending,
        Count
    };

    constexpr std::array<const char*, static_cast<std::size_t>(DemoNetworkRequestType::Count)> networkRequestTypeLabels = {
        "Network", "File", "Cache"
    };

    constexpr std::array<const char*, static_cast<std::size_t>(DemoNetworkRequestStatus::Count)> networkRequestStatusLabels = {
        "OK", "Cache", "Not found", "Error", "Blacklisted", "Canceled", "Pending"
    };

    int networkRequestTypeIndex(const std::string& type)
    {
        auto lower = detail::toLower(type);
        if (lower.find("cache") != std::string::npos) return static_cast<int>(DemoNetworkRequestType::Cache);
        if (lower.find("file") != std::string::npos) return static_cast<int>(DemoNetworkRequestType::File);
        return static_cast<int>(DemoNetworkRequestType::Network);
    }

    int networkRequestStatusIndex(const std::string& status)
    {
        auto lower = detail::toLower(status);
        if (lower.find("cache") != std::string::npos) return static_cast<int>(DemoNetworkRequestStatus::Cache);
        if (lower.find("not found") != std::string::npos) return static_cast<int>(DemoNetworkRequestStatus::NotFound);
        if (lower.find("blacklisted") != std::string::npos) return static_cast<int>(DemoNetworkRequestStatus::Blacklisted);
        if (lower.find("cancel") != std::string::npos) return static_cast<int>(DemoNetworkRequestStatus::Canceled);
        if (lower.find("pending") != std::string::npos) return static_cast<int>(DemoNetworkRequestStatus::Pending);
        if (lower.find("ok") != std::string::npos) return static_cast<int>(DemoNetworkRequestStatus::OK);
        return static_cast<int>(DemoNetworkRequestStatus::Error);
    }

    ImVec4 networkRequestColor(const NetworkMonitor::Request& request)
    {
        if (!request.complete)
        {
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        switch (static_cast<DemoNetworkRequestStatus>(networkRequestStatusIndex(request.status)))
        {
        case DemoNetworkRequestStatus::OK:
            return ImVec4(0.25f, 0.95f, 0.35f, 1.0f);
        case DemoNetworkRequestStatus::Cache:
            return ImVec4(0.25f, 0.85f, 1.0f, 1.0f);
        case DemoNetworkRequestStatus::Blacklisted:
            return ImVec4(1.0f, 0.62f, 0.18f, 1.0f);
        case DemoNetworkRequestStatus::Canceled:
            return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
        case DemoNetworkRequestStatus::Pending:
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        default:
            return ImVec4(1.0f, 0.28f, 0.28f, 1.0f);
        }
    }

    bool networkRequestContainsText(const NetworkMonitor::Request& request, const std::string& filter)
    {
        if (filter.empty())
        {
            return true;
        }

        return
            detail::toLower(request.uri).find(filter) != std::string::npos ||
            detail::toLower(request.layer).find(filter) != std::string::npos ||
            detail::toLower(request.type).find(filter) != std::string::npos ||
            detail::toLower(request.status).find(filter) != std::string::npos ||
            detail::toLower(request.detail).find(filter) != std::string::npos;
    }
}

auto Demo_NetworkMonitor = [](Application& app)
{
    auto monitor = app.io().services().networkMonitor;
    if (!monitor)
    {
        ImGui::TextUnformatted("No network monitor is installed.");
        return;
    }

    static bool enabled = true;
    static bool autoScroll = true;
    static char textFilter[128] = {};
    static std::array<bool, static_cast<std::size_t>(DemoNetworkRequestType::Count)> typeFilter = { true, true, true };
    static std::array<bool, static_cast<std::size_t>(DemoNetworkRequestStatus::Count)> statusFilter = { true, true, true, true, true, true, true };
    static NetworkMonitor::RequestId expandedRequest = 0;
    static std::size_t lastTableSize = 0;

    monitor->setEnabled(enabled);

    if (enabled)
    {
        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("NetworkMonitorControls"))
    {
        ImGuiLTable::Checkbox("Enabled", &enabled);
        ImGuiLTable::End();
    }

    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("Filter", textFilter, sizeof(textFilter));

    const float comboBuffer = 50.0f;

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Blacklisted").x + comboBuffer);
    if (ImGui::BeginCombo("Status", "Status..."))
    {
        for (std::size_t i = 0; i < networkRequestStatusLabels.size(); ++i)
        {
            ImGui::Checkbox(networkRequestStatusLabels[i], &statusFilter[i]);
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Network").x + comboBuffer);
    if (ImGui::BeginCombo("From", "From..."))
    {
        for (std::size_t i = 0; i < networkRequestTypeLabels.size(); ++i)
        {
            ImGui::Checkbox(networkRequestTypeLabels[i], &typeFilter[i]);
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Autoscroll", &autoScroll);

    bool jumpToEnd = false;
    ImGui::SameLine();
    if (ImGui::Button("Latest"))
    {
        jumpToEnd = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        monitor->clear();
        expandedRequest = 0;
    }

    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        monitor->saveCSV("network_requests.csv");
    }

    auto requests = monitor->requests();
    std::string filterLower = detail::toLower(textFilter);
    std::set<std::string> uniqueURIs;
    std::optional<NetworkMonitor::Clock::time_point> minTime;
    std::optional<NetworkMonitor::Clock::time_point> maxTime;
    std::size_t filteredCount = 0;
    std::size_t rowCount = 0;

    auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
    ImVec2 tableSize(0.0f, lineHeight * 18.0f);
    if (ImGui::BeginTable(
        "NetworkMonitorRequests",
        6,
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_Borders,
        tableSize))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableSetupColumn("Time(ms)", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto& request : requests)
        {
            auto typeIndex = networkRequestTypeIndex(request.type);
            auto statusIndex = networkRequestStatusIndex(request.status);

            if (!typeFilter[typeIndex] || !statusFilter[statusIndex])
            {
                continue;
            }

            if (!networkRequestContainsText(request, filterLower))
            {
                continue;
            }

            auto requestEnd = request.complete ? request.endTime : NetworkMonitor::Clock::now();
            minTime = minTime.has_value() ? std::min(minTime.value(), request.startTime) : request.startTime;
            maxTime = maxTime.has_value() ? std::max(maxTime.value(), requestEnd) : requestEnd;
            uniqueURIs.insert(request.uri);
            ++filteredCount;
            ++rowCount;

            ImGui::PushID(static_cast<int>(request.id));

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(request.layer.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.1f", request.elapsedMilliseconds());

            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(request.bytesReceived));

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(request.type.c_str());

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, networkRequestColor(request));
            ImGui::TextUnformatted(request.status.c_str());
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Copy"))
            {
                ImGui::SetClipboardText(request.uri.c_str());
            }
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, networkRequestColor(request));
            bool selected = expandedRequest == request.id;
            if (ImGui::Selectable(request.uri.c_str(), selected))
            {
                expandedRequest = selected ? 0 : request.id;
            }
            ImGui::PopStyleColor();

            if (expandedRequest == request.id && !request.detail.empty())
            {
                ImGui::TextWrapped("%s", request.detail.c_str());
            }

            ImGui::PopID();
        }

        if (rowCount != lastTableSize && autoScroll)
        {
            jumpToEnd = true;
        }
        lastTableSize = rowCount;

        if (jumpToEnd || ImGui::GetScrollY() == ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndTable();
    }

    double totalSeconds = 0.0;
    if (minTime.has_value() && maxTime.has_value() && maxTime.value() > minTime.value())
    {
        totalSeconds = std::chrono::duration<double>(maxTime.value() - minTime.value()).count();
    }

    ImGui::Text(
        "%u / %u requests | unique %u | %.1f s",
        static_cast<unsigned>(filteredCount),
        static_cast<unsigned>(requests.size()),
        static_cast<unsigned>(uniqueURIs.size()),
        totalSeconds);
};
