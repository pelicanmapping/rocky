/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ECSNode.h"

#include "MeshSystem.h"
#include "LineSystem.h"
#include "IconSystem.h"
#include "LabelSystem.h"
#include "WidgetSystem.h"
#include "TransformSystem.h"
#include "NodeGraph.h"

ROCKY_ABOUT(entt, ENTT_VERSION);

using namespace ROCKY_NAMESPACE;


ECSNode::ECSNode(Registry& reg) :
    registry(reg)
{
    factory.start();
}

ECSNode::ECSNode(Registry& reg, bool addDefaultSystems) :
    ECSNode(reg)
{
    if (addDefaultSystems)
    {
        add(TransformSystem::create(registry));
        add(MeshSystemNode::create(registry));
        add(NodeSystemNode::create(registry));
        add(LineSystemNode::create(registry));
        add(IconSystemNode::create(registry));
        add(LabelSystemNode::create(registry));
#ifdef ROCKY_HAS_IMGUI
        add(WidgetSystemNode::create(registry));
#endif
    }
}

ECSNode::~ECSNode()
{
    factory.quit();
}

void
ECSNode::initialize(VSGContext& vsgcontext)
{
    for (auto& child : children)
    {
        auto systemNode = child->cast<detail::SystemNodeBase>();
        if (systemNode)
        {
            systemNode->factory = &factory;
        }
    }

    for (auto& system : systems)
    {
        system->initialize(vsgcontext);
    }
}

void
ECSNode::update(VSGContext& vsgcontext)
{
    // update all systems
    for (auto& system : systems)
    {
        system->update(vsgcontext);
    }

    factory.mergeResults(registry, vsgcontext);
}




void
detail::EntityNodeFactory::start()
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
                    detail::BuildBatch batch;

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
detail::EntityNodeFactory::quit()
{
    if (buffers)
    {
        buffers = nullptr;
        thread.join();
    }
}

void
detail::EntityNodeFactory::mergeResults(Registry& r, VSGContext& vsgcontext)
{
    if (buffers)
    {
        BuildBatch batch;

        while (buffers->output.pop(batch))
        {
            auto [lock, registry] = r.read();

            for (auto& item : batch.items)
            {
                batch.system->mergeCreateOrUpdateResults(registry, item, vsgcontext);
            }
        }
    }
}
