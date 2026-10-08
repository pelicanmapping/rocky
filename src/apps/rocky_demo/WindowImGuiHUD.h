/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Application.h>
#include <imgui.h>
#include <cfloat>
#include <functional>

// Application-owned, passive ImGui HUD. The application must outlive this helper.
// Construct/destroy during setup/update or after run(), never during record traversal.
class WindowImGuiHUD
{
public:
    struct State : public vsg::Inherit<vsg::Object, State>
    {
        bool visible = false;
        ImVec2 mousePosition{ -FLT_MAX, -FLT_MAX };
    };

    using DrawFunction = std::function<void(ImGuiContext*, const ImVec2&)>;

    //! Attaches a final pass and a non-consuming mouse observer to one managed window.
    //! The draw callback runs with this HUD's context current, inside an ImGui frame.
    WindowImGuiHUD(rocky::Application&, const rocky::Window&, DrawFunction);

    //! Detaches callbacks and waits for outstanding GPU work before releasing the HUD.
    ~WindowImGuiHUD();

    WindowImGuiHUD(const WindowImGuiHUD&) = delete;
    WindowImGuiHUD& operator=(const WindowImGuiHUD&) = delete;

    //! Returns the controls shared with the demo UI; change them only in the application's frame loop.
    vsg::ref_ptr<State> state() const { return _state; }

private:
    class HUDNode;
    class MouseObserver;

    //! Restores final rendering order after a new view is appended during an update.
    void moveToEnd();

    //! Removes this installation; safe to call again after the window has been removed.
    void detach();

    rocky::Application& _app;
    rocky::Window _window;
    vsg::ref_ptr<State> _state;
    vsg::ref_ptr<HUDNode> _node;
    vsg::ref_ptr<MouseObserver> _events;
    rocky::CallbackSubscriptions _subscriptions;
};
