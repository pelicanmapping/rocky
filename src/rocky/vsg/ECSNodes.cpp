/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ECSNodes.h"

ROCKY_ABOUT(entt, ENTT_VERSION);

using namespace ROCKY_NAMESPACE;

#if 0
ecs::ECSNode::ECSNode(ecs::Registry& reg) : //, BackgroundServices& bg) :
    _registry(reg)
{
    vsg::observer_ptr<ECSNode> weak_self(this);

    auto entity_compiler = [weak_self](jobs::cancelable& cancelable)
        {
            Log()->info("Entity compiler thread starting up.");
            while (!cancelable.canceled())
            {
                vsg::ref_ptr<ECSNode> self(weak_self);
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

                        for (auto& item : batch.items)
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

    _background.start("rocky::entity_compiler", entity_compiler);
}
#endif

ecs::ECSNode::ECSNode(ecs::Registry& reg) :
    registry(reg)
{
    compiler.start();
}

ecs::ECSNode::~ECSNode()
{
    compiler.quit();
}


void
ecs::ECSNode::update(Runtime& runtime)
{
    // update all systems
    for (auto& system : systems)
    {
        system->update(runtime);
    }

    compiler.mergeCompiledNodes(registry, runtime);
}




void
ecs::EntityNodeCompiler::start()
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
                        auto group = vsg::Group::create();

                        for (auto& item : batch.items)
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
                            buffers->output.emplace(std::move(batch));
                        }
                    }
                }
            }
            Log()->info("Entity compiler thread terminating.");
        };

    thread = std::thread(task);
}

void
ecs::EntityNodeCompiler::quit()
{
    if (buffers)
    {
        buffers = nullptr;
        thread.join();
    }
}

void
ecs::EntityNodeCompiler::mergeCompiledNodes(Registry& r, Runtime& runtime)
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
