/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ECS.h"

ROCKY_ABOUT(entt, ENTT_VERSION);

using namespace ROCKY_NAMESPACE;

ecs::SystemsManagerGroup::SystemsManagerGroup(BackgroundServices& bg)
{
    vsg::observer_ptr<SystemsManagerGroup> weak_self(this);

    auto entity_compiler = [weak_self](jobs::cancelable& cancelable)
        {
            Log()->info("Entity compiler thread starting up.");
            while (!cancelable.canceled())
            {
                vsg::ref_ptr<SystemsManagerGroup> self(weak_self);
                if (!self)
                    break;

                // normally this will be signaled to wake up, but the timeout will
                // assure that we don't wait forever during shutdown.
                if (self->buildInput.wait(std::chrono::milliseconds(1000)))
                {
                    ecs::BuildBatch batch;

                    if (self->buildInput.pop(batch))
                    {
                        // a group to combine all compiles into one operation
                        auto group = vsg::Group::create();

                        for(auto& item : batch.items)
                        {
                            batch.system->invokeCreateOrUpdate(item, *batch.runtime);

                            if (item.new_node)
                            {
                                group->addChild(item.new_node);
                            }
                        }

                        // compile everything (creates any new vulkan objects)
                        if (group->children.size() > 0)
                        {
                            // compile all the results at once:
                            batch.runtime->compile(group);

                            // queue the results so the merger will pick em up
                            // (in SystemsManagerGroup::update)
                            self->buildOutput.emplace(std::move(batch));
                        }
                    }
                }
            }
            Log()->info("Entity compiler thread terminating.");
        };

    bg.start("rocky::entity_compiler", entity_compiler);
}


void
ecs::SystemsManagerGroup::update(Runtime& runtime)
{
    // update all systems
    for (auto& system : systems)
    {
        system->update(runtime);
    }

    // process any new nodes that were compiled
    ecs::BuildBatch batch;
    while (buildOutput.pop(batch))
    {
        for (auto& item : batch.items)
        {
            batch.system->mergeCreateOrUpdateResults(item);
        }
    }
}