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
                if (self->entityCompileJobs.wait(std::chrono::milliseconds(1000)))
                {
                    SystemNodeBase::EntityCompileBatch batch;

                    if (self->entityCompileJobs.pop(batch))
                    {
                        // a group to combine all compiles into one operation
                        auto group = vsg::Group::create();

                        // collect the results so we can adjust the revision all at once 
                        // after compilation
                        std::vector<std::tuple<entt::entity, SystemNodeBase::CreateOrUpdateData, SystemNodeBase::EntityCompileBatch*>> results;
                        results.reserve(batch.entities.size());

                        for (auto entity : batch.entities)
                        {
                            SystemNodeBase::CreateOrUpdateData result;

                            batch.system->create_or_update(entity, result, *batch.runtime);

                            if (result.new_node)
                            {
                                if (result.new_node_needs_compilation)
                                {
                                    // this group contains everything that needs to be compiled:
                                    group->addChild(result.new_node);
                                }
                                results.emplace_back(entity, std::move(result), &batch);
                            }
                        }

                        // compile everything (creates any new vulkan objects)
                        if (group->children.size() > 0)
                        {
                            // compile all the results at once:
                            batch.runtime->compile(group);

                            // move the compiled results to the staging area. The next iteration
                            // will pick this up and install them.
                            for (auto& [entity, result, batch] : results)
                            {
                                batch->system->finish_update(entity, result);
                            }
                        }
                    }
                }
            }
            Log()->info("Entity compiler thread terminating.");
        };

    bg.start("rocky::entity_compiler", entity_compiler);
}
