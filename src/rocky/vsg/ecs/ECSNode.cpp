/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ECSNode.h"

ROCKY_ABOUT(entt, ENTT_VERSION);

using namespace ROCKY_NAMESPACE;


ecs::ECSNode::ECSNode(ecs::Registry& reg) :
    registry(reg)
{
    factory.start();
}

ecs::ECSNode::~ECSNode()
{
    factory.quit();
}


void
ecs::ECSNode::update(VSGContext& runtime)
{
    // update all systems
    for (auto& system : systems)
    {
        system->update(runtime);
    }

    factory.mergeResults(registry, runtime);
}




void
ecs::EntityNodeFactory::start()
{
    buffers = std::make_shared<Buffers>();

    auto task = [buffers(this->buffers)]()
        {
            Log()->info("Entity compiler thread starting up.");

            while (buffers.use_count() > 1)
            {
                // normally this will be signaled to wake up, but the timeout will
                // assure that we don't wait forever during shutdown.
                if (buffers->input.wait(std::chrono::milliseconds(500)))
                {
                    ecs::BuildBatch batch;

                    if (buffers->input.pop(batch))
                    {
                        // a group to combine all compiles into one operation
                        auto group = vsg::Objects::create();

                        for (auto& item : batch.items)
                        {
                            batch.system->invokeCreateOrUpdate(item, *batch.context);

                            if (item.new_node)
                            {
                                group->addChild(item.new_node);
                            }
                        }

                        // compile everything (creates any new vulkan objects)
                        if (group->children.size() > 0)
                        {
                            // compile all the results at once:
                            (*batch.context)->compile(group);

                            // queue the results so the merger will pick em up
                            // (in SystemsManagerGroup::update)
                            int count = 0;
                            while (!buffers->output.push(batch) && buffers.use_count() > 1)
                            {
                                //Log()->warn("Failed to enqueue entity reusults - queue overflow - will retry, tries = {}", ++count);
                                std::this_thread::yield();
                            }
                        }
                    }
                }
            }
            Log()->info("Entity compiler thread terminating.");
        };

    thread = std::thread(task);
}

void
ecs::EntityNodeFactory::quit()
{
    if (buffers)
    {
        buffers = nullptr;
        thread.join();
    }
}

void
ecs::EntityNodeFactory::mergeResults(Registry& r, VSGContext& runtime)
{
    if (buffers)
    {
        BuildBatch batch;

        while (buffers->output.pop(batch))
        {
            auto [lock, registry] = r.read();

            for (auto& item : batch.items)
            {
                batch.system->mergeCreateOrUpdateResults(registry, item, runtime);
            }
        }
    }
}
