/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <algorithm>
#include <random>

using namespace ROCKY_NAMESPACE;

namespace
{
    struct ScriptDemoState
    {
        std::vector<entt::entity> entities;
        std::vector<entt::entity> styles;
        std::mt19937 rng{ std::random_device{}() };
        bool initialized = false;
        unsigned emitted = 0;
        unsigned fleetFlights = 0;
        float speedMin = 90000.0f;
        float speedMax = 260000.0f;
        float emitInterval = 10.0f;

        float randomSpeed()
        {
            std::uniform_real_distribution<float> dist(speedMin, speedMax);
            return dist(rng);
        }

        Color randomLabelColor()
        {
            std::uniform_real_distribution<float> dist(0.35f, 1.0f);
            auto color = Color(dist(rng), dist(rng), dist(rng), 1.0f);
            color[emitted % 3] = 1.0f;
            return color;
        }
    };

    entt::entity createScriptLabelStyle(entt::registry& registry, ScriptDemoState& state)
    {
        auto entity = registry.create();
        auto& style = registry.emplace<LabelStyle>(entity);
        style.fontName = std::filesystem::path(ROCKY_DEMO_DEFAULT_FONT).lexically_normal().string();
        style.textSize = 18.0f;
        style.textOutlineSize = 2.0f;
        style.textColor = state.randomLabelColor();
        style.textOutlineColor = StockColor::Black;
        style.backgroundColor = Color(0.0f, 0.0f, 0.0f, 0.65f);
        style.borderColor = style.textColor;
        style.borderSize = 1.0f;
        style.padding = { 5.0f, 3.0f };
        style.textPivot = { 0.5f, 1.0f };
        style.textOffset = { 0, -10 };

        state.styles.emplace_back(entity);
        return entity;
    }

    class FlightScript : public Script::Runner
    {
    public:
        FlightScript(
            std::string in_name,
            const GeoPoint& in_start,
            const GeoPoint& in_end,
            float in_speed,
            float in_emitInterval,
            entt::entity in_labelStyle,
            ScriptDemoState* in_state) :
            name(std::move(in_name)),
            start(in_start.transform(SRS::ECEF)),
            end(in_end.transform(SRS::ECEF)),
            speed(in_speed),
            emitInterval(in_emitInterval),
            labelStyle(in_labelStyle),
            state(in_state)
        {
            auto offset = (glm::dvec3)end - (glm::dvec3)start;
            distance = glm::length(offset);
            direction = distance > 0.0 ? offset / distance : glm::dvec3(0.0, 0.0, 0.0);
        }

        std::shared_ptr<Script::Runner> clone() const override {
            return std::make_shared<FlightScript>(*this);
        }

        void OnCreate(entt::registry& registry, entt::entity entity) override
        {
            if (auto* transform = registry.try_get<Transform>(entity))
            {
                transform->position = start;
                transform->dirty(registry);
            }

            updateLabel(registry, entity);
        }

        void OnUpdate(entt::registry& registry, entt::entity entity, float deltaTime) override
        {
            if (distance <= 0.0)
            {
                land(registry, entity);
                return;
            }

            if (traveled >= distance)
                return;

            traveled = std::min(distance, traveled + static_cast<double>(speed) * deltaTime);
            GeoPoint current = positionAt(traveled);

            if (auto* transform = registry.try_get<Transform>(entity))
            {
                transform->position = current;
                transform->dirty(registry);
            }

            emitClock += deltaTime;
            while (emitClock >= emitInterval && traveled < distance)
            {
                emitClock -= emitInterval;
                emit(registry, current);
            }

            updateLabel(registry, entity);

            if (traveled >= distance)
            {
                land(registry, entity);
            }
        }

    protected:
        virtual const char* behaviorName() const {
            return "Straight";
        }

        virtual GeoPoint positionAt(double distanceAlongPath) const
        {
            return GeoPoint(SRS::ECEF, (glm::dvec3)start + direction * distanceAlongPath);
        }

        virtual std::shared_ptr<Script::Runner> createChildRunner(
            const std::string& childName,
            const GeoPoint& childStart,
            const GeoPoint& childEnd,
            float childSpeed,
            entt::entity childStyle) const
        {
            return std::make_shared<FlightScript>(
                childName,
                childStart,
                childEnd,
                childSpeed,
                emitInterval,
                childStyle,
                state);
        }

        void updateLabel(entt::registry& registry, entt::entity entity)
        {
            if (auto* label = registry.try_get<Label>(entity))
            {
                auto remaining = std::max(0.0, distance - traveled);
                label->text = name + " (" + behaviorName() + ")\n" +
                    std::to_string(static_cast<int>(remaining / 1000.0)) + " km remaining";
                Label::dirty(registry, entity);
            }
        }

        void land(entt::registry& registry, entt::entity entity)
        {
            traveled = distance;

            if (auto* label = registry.try_get<Label>(entity))
            {
                label->text = name + " (" + behaviorName() + ")\nLanded";
                Label::dirty(registry, entity);
            }
        }

        void emit(entt::registry& registry, const GeoPoint& position)
        {
            if (!state)
                return;

            auto emittedName = name + "." + std::to_string(++state->emitted);
            auto entity = registry.create();
            auto childEnd = destinationForChild(state->emitted);
            auto childStyle = createScriptLabelStyle(registry, *state);
            auto childSpeed = state->randomSpeed();

            auto& label = registry.emplace<Label>(entity, emittedName);
            label.style = childStyle;

            auto& transform = registry.emplace<Transform>(entity);
            transform.position = position;

            registry.emplace<Script>(
                entity,
                createChildRunner(
                    emittedName,
                    position,
                    childEnd,
                    childSpeed,
                    childStyle));

            state->entities.emplace_back(entity);
        }

        GeoPoint destinationForChild(unsigned sequence) const
        {
            auto base = end.transform(SRS::WGS84);
            double lonOffset = ((sequence % 7) - 3) * 8.0;
            double latOffset = (((sequence / 7) % 5) - 2) * 5.0;
            double lon = std::clamp(base.x + lonOffset, -180.0, 180.0);
            double lat = std::clamp(base.y + latOffset, -80.0, 80.0);
            return GeoPoint(SRS::WGS84, lon, lat, base.z);
        }

        std::string name;
        GeoPoint start;
        GeoPoint end;
        glm::dvec3 direction = { 0.0, 0.0, 0.0 };
        double distance = 0.0;
        double traveled = 0.0;
        float speed = 0.0f;
        float emitInterval = 10.0f;
        float emitClock = 0.0f;
        entt::entity labelStyle = entt::null;
        ScriptDemoState* state = nullptr;
    };

    class ArcingFlightScript : public FlightScript
    {
    public:
        using FlightScript::FlightScript;

        std::shared_ptr<Script::Runner> clone() const override {
            return std::make_shared<ArcingFlightScript>(*this);
        }

    protected:
        const char* behaviorName() const override {
            return "Arc";
        }

        GeoPoint positionAt(double distanceAlongPath) const override
        {
            auto t = distance > 0.0 ? distanceAlongPath / distance : 1.0;
            auto straight = (glm::dvec3)start + direction * distanceAlongPath;
            auto up = glm::normalize(straight);
            return GeoPoint(SRS::ECEF, straight + up * std::sin(glm::pi<double>() * t) * 500000.0);
        }

        std::shared_ptr<Script::Runner> createChildRunner(
            const std::string& childName,
            const GeoPoint& childStart,
            const GeoPoint& childEnd,
            float childSpeed,
            entt::entity childStyle) const override
        {
            return std::make_shared<ArcingFlightScript>(
                childName,
                childStart,
                childEnd,
                childSpeed,
                emitInterval,
                childStyle,
                state);
        }
    };

    class WeavingFlightScript : public FlightScript
    {
    public:
        using FlightScript::FlightScript;

        std::shared_ptr<Script::Runner> clone() const override {
            return std::make_shared<WeavingFlightScript>(*this);
        }

    protected:
        const char* behaviorName() const override {
            return "Weave";
        }

        GeoPoint positionAt(double distanceAlongPath) const override
        {
            auto t = distance > 0.0 ? distanceAlongPath / distance : 1.0;
            auto straight = (glm::dvec3)start + direction * distanceAlongPath;
            auto side = glm::cross(direction, glm::normalize((glm::dvec3)start));
            if (glm::length(side) <= 0.000001)
                side = { 0.0, 0.0, 1.0 };
            side = glm::normalize(side);
            return GeoPoint(SRS::ECEF, straight + side * std::sin(glm::two_pi<double>() * t * 4.0) * 300000.0);
        }

        std::shared_ptr<Script::Runner> createChildRunner(
            const std::string& childName,
            const GeoPoint& childStart,
            const GeoPoint& childEnd,
            float childSpeed,
            entt::entity childStyle) const override
        {
            return std::make_shared<WeavingFlightScript>(
                childName,
                childStart,
                childEnd,
                childSpeed,
                emitInterval,
                childStyle,
                state);
        }
    };

    class AirportFleetScript : public Script::Runner
    {
    public:
        AirportFleetScript(ScriptDemoState* in_state) :
            state(in_state) { }

        std::shared_ptr<Script::Runner> clone() const override {
            return std::make_shared<AirportFleetScript>(*this);
        }

        void OnCreate(entt::registry& registry, entt::entity entity) override
        {
            spawnFlights(registry);
        }

        void OnDestroy(entt::registry& registry, entt::entity entity) override
        {
            cleanup(registry);
            if (state)
            {
                state->entities.erase(
                    std::remove(state->entities.begin(), state->entities.end(), entity),
                    state->entities.end());
                state->fleetFlights = 0;
            }
        }

        void OnUpdate(entt::registry& registry, entt::entity entity, float deltaTime) override
        {
            for (auto iter = flights.begin(); iter != flights.end();)
            {
                auto& flight = *iter;
                if (!registry.valid(flight.entity))
                {
                    iter = flights.erase(iter);
                    continue;
                }

                flight.traveled = std::min(flight.distance, flight.traveled + static_cast<double>(flight.speed) * deltaTime);
                auto current = GeoPoint(SRS::ECEF, (glm::dvec3)flight.start + flight.direction * flight.traveled);

                if (auto* transform = registry.try_get<Transform>(flight.entity))
                {
                    transform->position = current;
                    transform->dirty(registry);
                }

                if (auto* label = registry.try_get<Label>(flight.entity))
                {
                    if (flight.traveled >= flight.distance)
                    {
                        label->text = "Fleet " + flight.code + "\nLanded";
                    }
                    else
                    {
                        auto remaining = std::max(0.0, flight.distance - flight.traveled);
                        label->text = "Fleet " + flight.code + "\n" +
                            std::to_string(static_cast<int>(remaining / 1000.0)) + " km remaining";
                    }
                    Label::dirty(registry, flight.entity);
                }

                ++iter;
            }

            if (state)
                state->fleetFlights = static_cast<unsigned>(flights.size());
        }

    private:
        struct Flight
        {
            entt::entity entity = entt::null;
            entt::entity style = entt::null;
            std::string code;
            GeoPoint start;
            GeoPoint end;
            glm::dvec3 direction = { 0.0, 0.0, 0.0 };
            double distance = 0.0;
            double traveled = 0.0;
            float speed = 0.0f;
        };

        void spawnFlights(entt::registry& registry)
        {
            if (!state)
                return;

            const GeoPoint lax(SRS::WGS84, -118.4085, 33.9416, 180000.0);

            struct Destination
            {
                const char* code;
                double lon;
                double lat;
            };

            const Destination destinations[] = {
                { "JFK", -73.7781, 40.6413 },
                { "BOS", -71.0096, 42.3656 },
                { "MIA", -80.2870, 25.7959 },
                { "ORD", -87.9073, 41.9742 },
                { "DFW", -97.0403, 32.8998 },
                { "DEN", -104.6737, 39.8561 },
                { "SEA", -122.3088, 47.4502 },
                { "SFO", -122.3790, 37.6213 },
                { "ATL", -84.4277, 33.6407 },
                { "IAD", -77.4565, 38.9531 }
            };

            for (auto& dest : destinations)
            {
                Flight flight;
                flight.code = dest.code;
                flight.start = lax.transform(SRS::ECEF);
                flight.end = GeoPoint(SRS::WGS84, dest.lon, dest.lat, 180000.0).transform(SRS::ECEF);
                flight.speed = state->randomSpeed();

                auto offset = (glm::dvec3)flight.end - (glm::dvec3)flight.start;
                flight.distance = glm::length(offset);
                flight.direction = flight.distance > 0.0 ? offset / flight.distance : glm::dvec3(0.0, 0.0, 0.0);

                flight.entity = registry.create();
                flight.style = createScriptLabelStyle(registry, *state);

                auto& label = registry.emplace<Label>(flight.entity, std::string("Fleet ") + flight.code);
                label.style = flight.style;

                auto& transform = registry.emplace<Transform>(flight.entity);
                transform.position = flight.start;

                state->entities.emplace_back(flight.entity);
                flights.emplace_back(flight);
            }

            state->fleetFlights = static_cast<unsigned>(flights.size());
        }

        void destroyFlight(entt::registry& registry, const Flight& flight)
        {
            if (state)
            {
                state->entities.erase(
                    std::remove(state->entities.begin(), state->entities.end(), flight.entity),
                    state->entities.end());

                state->styles.erase(
                    std::remove(state->styles.begin(), state->styles.end(), flight.style),
                    state->styles.end());
            }

            if (registry.valid(flight.entity))
                registry.destroy(flight.entity);

            if (registry.valid(flight.style))
                registry.destroy(flight.style);
        }

        void cleanup(entt::registry& registry)
        {
            for (auto& flight : flights)
            {
                destroyFlight(registry, flight);
            }
            flights.clear();
        }

        ScriptDemoState* state = nullptr;
        std::vector<Flight> flights;
    };

    template<class RUNNER_T>
    void createScriptLabel(
        entt::registry& registry,
        ScriptDemoState& state,
        const std::string& name,
        const GeoPoint& start,
        const GeoPoint& end)
    {
        auto entity = registry.create();
        auto labelStyle = createScriptLabelStyle(registry, state);
        auto speed = state.randomSpeed();

        auto& label = registry.emplace<Label>(entity, name);
        label.style = labelStyle;

        auto& transform = registry.emplace<Transform>(entity);
        transform.position = start.transform(SRS::ECEF);

        registry.emplace<Script>(
            entity,
            std::make_shared<RUNNER_T>(
                name,
                start,
                end,
                speed,
                state.emitInterval,
                labelStyle,
                &state));

        state.entities.emplace_back(entity);
    }

    void resetScriptDemo(Application& app, ScriptDemoState& state)
    {
        app.registry.write([&](entt::registry& registry)
            {
                auto entities = state.entities;
                auto styles = state.styles;

                for (auto entity : entities)
                {
                    if (registry.valid(entity))
                        registry.destroy(entity);
                }

                for (auto entity : styles)
                {
                    if (registry.valid(entity))
                        registry.destroy(entity);
                }

                state.entities.clear();
                state.styles.clear();
                state.initialized = true;
                state.emitted = 0;
                state.fleetFlights = 0;

                createScriptLabel<FlightScript>(
                    registry,
                    state,
                    "Script Alpha",
                    GeoPoint(SRS::WGS84, -122.4194, 37.7749, 250000.0),
                    GeoPoint(SRS::WGS84, -73.9352, 40.7306, 250000.0));

                createScriptLabel<ArcingFlightScript>(
                    registry,
                    state,
                    "Script Bravo",
                    GeoPoint(SRS::WGS84, -118.2437, 34.0522, 300000.0),
                    GeoPoint(SRS::WGS84, -0.1278, 51.5074, 300000.0));

                createScriptLabel<WeavingFlightScript>(
                    registry,
                    state,
                    "Script Charlie",
                    GeoPoint(SRS::WGS84, 139.6917, 35.6895, 350000.0),
                    GeoPoint(SRS::WGS84, 151.2093, -33.8688, 350000.0));

                auto fleetManager = registry.create();
                registry.emplace<Script>(
                    fleetManager,
                    std::make_shared<AirportFleetScript>(&state));
                state.entities.emplace_back(fleetManager);
            });

        app.vsgcontext->requestFrame();
    }
}

auto Demo_Script = [](Application& app)
{
    static ScriptDemoState state;

    if (!state.initialized)
    {
        resetScriptDemo(app, state);
    }

    if (ImGuiLTable::Begin("script demo"))
    {
        ImGuiLTable::Text("Entities", "%zu", state.entities.size());
        ImGuiLTable::Text("Emitted", "%u", state.emitted);
        ImGuiLTable::Text("Fleet flights", "%u", state.fleetFlights);
        ImGuiLTable::Text("Speed range", "%.0f-%.0f m/s", state.speedMin, state.speedMax);
        ImGuiLTable::Text("Emit interval", "%.0f s", state.emitInterval);

        if (ImGuiLTable::Button("Reset"))
        {
            resetScriptDemo(app, state);
        }

        ImGuiLTable::End();
    }
};
