/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Component.h>
#include <string_view>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS component that plays an audio file through the AudioSystem.
    *
    * Attach this component to an entity, usually alongside a Transform for 3D
    * positioning. Call play(registry) to start or restart playback. If looping
    * is false, play() plays the file once.
    */
    struct AudioSource : public Component<AudioSource>
    {
        using Component<AudioSource>::dirty;

        enum class Command
        {
            None,
            Play,
            Stop
        };

        //! Audio file to play.
        std::string location;

        //! Whether playback should loop.
        bool looping = false;

        //! Stream from disk instead of pre-decoding the entire file.
        bool stream = true;

        //! Whether to spatialize this source in 3D.
        bool spatialized = true;

        //! Linear gain multiplier.
        float gain = 1.0f;

        //! Distance at which attenuation begins, in world units.
        float minDistance = 1.0f;

        //! Distance at which attenuation stops increasing, in world units.
        float maxDistance = 1000000.0f;

        //! Attenuation rolloff factor.
        float rolloff = 1.0f;

        //! Revision for source configuration.
        Revision revision = 0;

        //! Revision for playback commands.
        Revision commandRevision = 0;

        //! Most recent playback command.
        Command command = Command::None;

        AudioSource() = default;

        AudioSource(std::string_view in_location, bool in_looping = false) :
            location(in_location),
            looping(in_looping) { }

        //! Mark source configuration dirty.
        void dirty(entt::registry& registry) {
            ++revision;
            Component<AudioSource>::dirty(registry);
        }

        //! Start or restart playback. If looping is false, this plays once.
        void play(entt::registry& registry) {
            command = Command::Play;
            ++commandRevision;
            Component<AudioSource>::dirty(registry);
        }

        //! Stop playback.
        void stop(entt::registry& registry) {
            command = Command::Stop;
            ++commandRevision;
            Component<AudioSource>::dirty(registry);
        }
    };
}
