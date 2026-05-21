/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "AudioSystem.h"
#include "TransformDetail.h"

#include <rocky/ecs/Visibility.h>
#include <rocky/miniaudio.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    inline vsg::dvec3 getTrans(const vsg::dmat4& m) {
        return vsg::dvec3(m[3][0], m[3][1], m[3][2]);
    }

    inline vsg::dvec3 getYAxis(const vsg::dmat4& m) {
        return vsg::dvec3(m[1][0], m[1][1], m[1][2]);
    }

    inline vsg::dvec3 getZAxis(const vsg::dmat4& m) {
        return vsg::dvec3(m[2][0], m[2][1], m[2][2]);
    }

    inline ma_uint32 flagsFor(const AudioSource& source)
    {
        ma_uint32 flags = 0;

        if (source.stream)
            flags |= MA_SOUND_FLAG_STREAM;

        if (source.looping)
            flags |= MA_SOUND_FLAG_LOOPING;

        if (!source.spatialized)
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

        return flags;
    }

    struct SoundDeleter
    {
        void operator()(ma_sound* sound) const
        {
            if (sound)
            {
                ma_sound_uninit(sound);
                delete sound;
            }
        }
    };
}

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        struct AudioSourceDetail
        {
            std::unique_ptr<ma_sound, SoundDeleter> sound;
            std::string location;
            bool stream = true;
            Revision commandRevision = -1;
        };
    }

    struct AudioSystem::Impl
    {
        std::mutex mutex;
        ma_engine engine = {};
        bool initialized = false;

        ma_result initialize()
        {
            std::scoped_lock lock(mutex);

            if (initialized)
                return MA_SUCCESS;

            auto config = ma_engine_config_init();
            config.listenerCount = MA_ENGINE_MAX_LISTENERS;

            auto result = ma_engine_init(&config, &engine);
            if (result == MA_SUCCESS)
            {
                initialized = true;

                for (ma_uint32 i = 0; i < ma_engine_get_listener_count(&engine); ++i)
                {
                    ma_engine_listener_set_enabled(&engine, i, MA_TRUE);
                }
            }

            return result;
        }

        void shutdown()
        {
            std::scoped_lock lock(mutex);

            if (initialized)
            {
                ma_engine_uninit(&engine);
                initialized = false;
            }
        }

        void applyConfig(const AudioSource& source, detail::AudioSourceDetail& detail)
        {
            if (!detail.sound)
                return;

            auto* sound = detail.sound.get();
            ma_sound_set_volume(sound, source.gain);
            ma_sound_set_looping(sound, source.looping ? MA_TRUE : MA_FALSE);
            ma_sound_set_spatialization_enabled(sound, source.spatialized ? MA_TRUE : MA_FALSE);
            ma_sound_set_positioning(sound, ma_positioning_absolute);
            ma_sound_set_attenuation_model(sound, ma_attenuation_model_inverse);
            ma_sound_set_min_distance(sound, source.minDistance);
            ma_sound_set_max_distance(sound, source.maxDistance);
            ma_sound_set_rolloff(sound, source.rolloff);
        }

        bool load(const AudioSource& source, detail::AudioSourceDetail& detail)
        {
            detail.sound.reset();

            if (source.location.empty())
            {
                detail.location.clear();
                return false;
            }

            auto sound = new ma_sound;
            auto result = ma_sound_init_from_file(
                &engine,
                source.location.c_str(),
                flagsFor(source),
                nullptr,
                nullptr,
                sound);

            if (result != MA_SUCCESS)
            {
                Log()->warn("AudioSystem: failed to load \"{}\" (miniaudio result {}).", source.location, (int)result);
                detail.location.clear();
                detail.stream = source.stream;
                delete sound;
                return false;
            }

            detail.location = source.location;
            detail.stream = source.stream;
            detail.sound.reset(sound);
            applyConfig(source, detail);
            return true;
        }

        void sync(const AudioSource& source, detail::AudioSourceDetail& detail)
        {
            if (source.location.empty())
            {
                detail.sound.reset();
                detail.location.clear();
                detail.commandRevision = source.commandRevision;
                return;
            }

            bool wasPlaying = detail.sound && ma_sound_is_playing(detail.sound.get());

            if (!detail.sound ||
                detail.location != source.location ||
                detail.stream != source.stream)
            {
                if (!load(source, detail))
                    return;

                if (wasPlaying)
                {
                    ma_sound_seek_to_pcm_frame(detail.sound.get(), 0);
                    ma_sound_start(detail.sound.get());
                }
            }
            else
            {
                applyConfig(source, detail);
            }

            if (!detail.sound || detail.commandRevision == source.commandRevision)
                return;

            switch (source.command)
            {
            case AudioSource::Command::Play:
                ma_sound_seek_to_pcm_frame(detail.sound.get(), 0);
                if (auto result = ma_sound_start(detail.sound.get()); result != MA_SUCCESS)
                {
                    Log()->warn("AudioSystem: failed to play \"{}\" (miniaudio result {}).", source.location, (int)result);
                }
                break;

            case AudioSource::Command::Stop:
                ma_sound_stop(detail.sound.get());
                break;

            case AudioSource::Command::None:
                break;
            }

            detail.commandRevision = source.commandRevision;
        }
    };
}

AudioSystem::AudioSystem(Registry& registry) :
    System(registry),
    _impl(std::make_unique<Impl>())
{
    registry.write([&](entt::registry& reg)
        {
            reg.on_construct<AudioSource>().connect<&AudioSystem::on_construct_AudioSource>(*this);
            reg.on_update<AudioSource>().connect<&AudioSystem::on_update_AudioSource>(*this);
            reg.on_destroy<AudioSource>().connect<&AudioSystem::on_destroy_AudioSource>(*this);

            auto e = reg.create();
            reg.emplace<AudioSource::Dirty>(e);
        });
}

AudioSystem::~AudioSystem()
{
    _registry.write([&](entt::registry& reg)
        {
            reg.view<detail::AudioSourceDetail>().each([](auto& detail)
                {
                    detail.sound.reset();
                });
        });

    _impl->shutdown();
}

void
AudioSystem::initialize(VSGContext)
{
    auto result = _impl->initialize();
    if (result != MA_SUCCESS)
    {
        status = Failure(Failure::ServiceUnavailable, "AudioSystem failed to initialize miniaudio");
        Log()->warn("AudioSystem: failed to initialize miniaudio (result {}).", (int)result);
    }
}

void
AudioSystem::update(VSGContext)
{
    if (status.failed())
        return;

    auto result = _impl->initialize();
    if (result != MA_SUCCESS)
    {
        status = Failure(Failure::ServiceUnavailable, "AudioSystem failed to initialize miniaudio");
        Log()->warn("AudioSystem: failed to initialize miniaudio (result {}).", (int)result);
        return;
    }

    _registry.read([&](entt::registry& reg)
        {
            AudioSource::eachDirty(reg, [&](entt::entity e)
                {
                    auto* source = reg.try_get<AudioSource>(e);
                    auto* detail = reg.try_get<detail::AudioSourceDetail>(e);

                    if (!source || !detail)
                        return;

                    std::scoped_lock lock(_impl->mutex);
                    _impl->sync(*source, *detail);
                });
        });
}

void
AudioSystem::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed())
        return;

    auto* state = record.getState();
    if (!state)
        return;

    auto viewID = record.getCommandBuffer()->viewID;
    auto worldFromView = vsg::inverse(state->modelviewMatrixStack.top());
    auto listenerPosition = getTrans(worldFromView);
    auto listenerForward = vsg::normalize(-getZAxis(worldFromView));
    auto listenerUp = vsg::normalize(getYAxis(worldFromView));

    _registry.read([&](entt::registry& reg)
        {
            std::scoped_lock lock(_impl->mutex);

            if (!_impl->initialized)
                return;

            ma_uint32 listenerCount = ma_engine_get_listener_count(&_impl->engine);
            ma_uint32 listenerIndex = viewID < listenerCount ? viewID : 0;

            ma_engine_listener_set_position(
                &_impl->engine,
                listenerIndex,
                (float)listenerPosition.x,
                (float)listenerPosition.y,
                (float)listenerPosition.z);

            ma_engine_listener_set_direction(
                &_impl->engine,
                listenerIndex,
                (float)listenerForward.x,
                (float)listenerForward.y,
                (float)listenerForward.z);

            ma_engine_listener_set_world_up(
                &_impl->engine,
                listenerIndex,
                (float)listenerUp.x,
                (float)listenerUp.y,
                (float)listenerUp.z);

            reg.view<AudioSource, detail::AudioSourceDetail, TransformDetail>().each(
                [&](entt::entity e, const AudioSource& source, detail::AudioSourceDetail& audioDetail, TransformDetail& transformDetail)
                {
                    if (!audioDetail.sound || !source.spatialized)
                        return;

                    auto* active = reg.try_get<ActiveState>(e);
                    if (active && !active->active)
                    {
                        ma_sound_stop(audioDetail.sound.get());
                        return;
                    }

                    auto& transformView = transformDetail.views[viewID];
                    if (transformView.revision < 0)
                        return;

                    auto sourcePosition = getTrans(transformView.model);
                    auto distance = vsg::length(sourcePosition - listenerPosition);

                    // miniaudio's max distance clamps attenuation; enforce Rocky's
                    // maxDistance as a hard cutoff so distant sources are silent.
                    ma_sound_set_volume(
                        audioDetail.sound.get(),
                        distance <= (double)source.maxDistance ? source.gain : 0.0f);

                    ma_sound_set_position(
                        audioDetail.sound.get(),
                        (float)sourcePosition.x,
                        (float)sourcePosition.y,
                        (float)sourcePosition.z);
                });
        });
}

void
AudioSystem::on_construct_AudioSource(entt::registry& registry, entt::entity entity)
{
    (void)registry.get_or_emplace<ActiveState>(entity);
    registry.emplace<detail::AudioSourceDetail>(entity);
    Component<AudioSource>::dirty(registry, entity);
}

void
AudioSystem::on_update_AudioSource(entt::registry& registry, entt::entity entity)
{
    Component<AudioSource>::dirty(registry, entity);
}

void
AudioSystem::on_destroy_AudioSource(entt::registry& registry, entt::entity entity)
{
    if (registry.all_of<detail::AudioSourceDetail>(entity))
    {
        std::scoped_lock lock(_impl->mutex);
        registry.remove<detail::AudioSourceDetail>(entity);
    }
}
