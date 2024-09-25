/* <editor-fold desc="MIT License">

Copyright(c) 2021 Don Burns, Roland Hill and Robert Osfield.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "SendEventsToImGui.h"
#include <imgui.h>

#include <vsg/ui/KeyEvent.h>
#include <vsg/ui/PointerEvent.h>
#include <vsg/ui/ScrollWheelEvent.h>

#include <iostream>

using namespace vsgImGui;

SendEventsToImGui::SendEventsToImGui() :
    _dragging(false)
{
    t0 = std::chrono::high_resolution_clock::now();

    _initKeymap();
}

SendEventsToImGui::~SendEventsToImGui()
{
}

uint32_t SendEventsToImGui::_convertButton(uint32_t button)
{
    return button == 1 ? 0 : button == 3 ? 1
                                         : button;
}

void SendEventsToImGui::_initKeymap()
{
    // clang-format off
    _vsg2imgui[vsg::KEY_Undefined]     = ImGuiKey_None;
    _vsg2imgui[vsg::KEY_Space]         = ImGuiKey_Space;
    _vsg2imgui[vsg::KEY_0]             = ImGuiKey_0;
    _vsg2imgui[vsg::KEY_1]             = ImGuiKey_1;
    _vsg2imgui[vsg::KEY_2]             = ImGuiKey_2;
    _vsg2imgui[vsg::KEY_3]             = ImGuiKey_3;
    _vsg2imgui[vsg::KEY_4]             = ImGuiKey_4;
    _vsg2imgui[vsg::KEY_5]             = ImGuiKey_5;
    _vsg2imgui[vsg::KEY_6]             = ImGuiKey_6;
    _vsg2imgui[vsg::KEY_7]             = ImGuiKey_7;
    _vsg2imgui[vsg::KEY_8]             = ImGuiKey_8;
    _vsg2imgui[vsg::KEY_9]             = ImGuiKey_9;
    _vsg2imgui[vsg::KEY_a]             = ImGuiKey_A;
    _vsg2imgui[vsg::KEY_b]             = ImGuiKey_B;
    _vsg2imgui[vsg::KEY_c]             = ImGuiKey_C;
    _vsg2imgui[vsg::KEY_d]             = ImGuiKey_D;
    _vsg2imgui[vsg::KEY_e]             = ImGuiKey_E;
    _vsg2imgui[vsg::KEY_f]             = ImGuiKey_F;
    _vsg2imgui[vsg::KEY_g]             = ImGuiKey_G;
    _vsg2imgui[vsg::KEY_h]             = ImGuiKey_H;
    _vsg2imgui[vsg::KEY_i]             = ImGuiKey_I;
    _vsg2imgui[vsg::KEY_j]             = ImGuiKey_J;
    _vsg2imgui[vsg::KEY_k]             = ImGuiKey_K;
    _vsg2imgui[vsg::KEY_l]             = ImGuiKey_L;
    _vsg2imgui[vsg::KEY_m]             = ImGuiKey_M;
    _vsg2imgui[vsg::KEY_n]             = ImGuiKey_N;
    _vsg2imgui[vsg::KEY_o]             = ImGuiKey_O;
    _vsg2imgui[vsg::KEY_p]             = ImGuiKey_P;
    _vsg2imgui[vsg::KEY_q]             = ImGuiKey_Q;
    _vsg2imgui[vsg::KEY_r]             = ImGuiKey_R;
    _vsg2imgui[vsg::KEY_s]             = ImGuiKey_S;
    _vsg2imgui[vsg::KEY_t]             = ImGuiKey_T;
    _vsg2imgui[vsg::KEY_u]             = ImGuiKey_U;
    _vsg2imgui[vsg::KEY_v]             = ImGuiKey_V;
    _vsg2imgui[vsg::KEY_w]             = ImGuiKey_W;
    _vsg2imgui[vsg::KEY_x]             = ImGuiKey_X;
    _vsg2imgui[vsg::KEY_y]             = ImGuiKey_Y;
    _vsg2imgui[vsg::KEY_z]             = ImGuiKey_Z;
    _vsg2imgui[vsg::KEY_Quote]         = ImGuiKey_Apostrophe;
    _vsg2imgui[vsg::KEY_Leftparen]     = ImGuiKey_LeftBracket;
    _vsg2imgui[vsg::KEY_Rightparen]    = ImGuiKey_RightBracket;
    _vsg2imgui[vsg::KEY_Comma]         = ImGuiKey_Comma;
    _vsg2imgui[vsg::KEY_Minus]         = ImGuiKey_Minus;
    _vsg2imgui[vsg::KEY_Period]        = ImGuiKey_Period;
    _vsg2imgui[vsg::KEY_Slash]         = ImGuiKey_Slash;
    _vsg2imgui[vsg::KEY_Semicolon]     = ImGuiKey_Semicolon;
    _vsg2imgui[vsg::KEY_Equals]        = ImGuiKey_Equal;
    _vsg2imgui[vsg::KEY_Backslash]     = ImGuiKey_Backslash;
    _vsg2imgui[vsg::KEY_BackSpace]     = ImGuiKey_Backspace;
    _vsg2imgui[vsg::KEY_Tab]           = ImGuiKey_Tab;
    _vsg2imgui[vsg::KEY_Return]        = ImGuiKey_Enter;
    _vsg2imgui[vsg::KEY_Pause]         = ImGuiKey_Pause;
    _vsg2imgui[vsg::KEY_Scroll_Lock]   = ImGuiKey_ScrollLock;
    _vsg2imgui[vsg::KEY_Escape]        = ImGuiKey_Escape;
    _vsg2imgui[vsg::KEY_Delete]        = ImGuiKey_Delete;
    _vsg2imgui[vsg::KEY_Home]          = ImGuiKey_Home;
    _vsg2imgui[vsg::KEY_Left]          = ImGuiKey_LeftArrow;
    _vsg2imgui[vsg::KEY_Up]            = ImGuiKey_UpArrow;
    _vsg2imgui[vsg::KEY_Right]         = ImGuiKey_RightArrow;
    _vsg2imgui[vsg::KEY_Down]          = ImGuiKey_DownArrow;
    _vsg2imgui[vsg::KEY_Page_Up]       = ImGuiKey_PageUp;
    _vsg2imgui[vsg::KEY_Page_Down]     = ImGuiKey_PageDown;
    _vsg2imgui[vsg::KEY_End]           = ImGuiKey_End;
    _vsg2imgui[vsg::KEY_Print]         = ImGuiKey_PrintScreen;
    _vsg2imgui[vsg::KEY_Insert]        = ImGuiKey_Insert;
    _vsg2imgui[vsg::KEY_Num_Lock]      = ImGuiKey_NumLock;
    _vsg2imgui[vsg::KEY_KP_Enter]      = ImGuiKey_KeypadEnter;
    _vsg2imgui[vsg::KEY_KP_Equal]      = ImGuiKey_KeypadEqual;
    _vsg2imgui[vsg::KEY_KP_Multiply]   = ImGuiKey_KeypadMultiply;
    _vsg2imgui[vsg::KEY_KP_Add]        = ImGuiKey_KeypadAdd;
    _vsg2imgui[vsg::KEY_KP_Subtract]   = ImGuiKey_KeypadSubtract;
    _vsg2imgui[vsg::KEY_KP_Decimal]    = ImGuiKey_KeypadDecimal;
    _vsg2imgui[vsg::KEY_KP_Divide]     = ImGuiKey_KeypadDivide;
    _vsg2imgui[vsg::KEY_KP_0]          = ImGuiKey_Keypad0;
    _vsg2imgui[vsg::KEY_KP_1]          = ImGuiKey_Keypad1;
    _vsg2imgui[vsg::KEY_KP_2]          = ImGuiKey_Keypad2;
    _vsg2imgui[vsg::KEY_KP_3]          = ImGuiKey_Keypad3;
    _vsg2imgui[vsg::KEY_KP_4]          = ImGuiKey_Keypad4;
    _vsg2imgui[vsg::KEY_KP_5]          = ImGuiKey_Keypad5;
    _vsg2imgui[vsg::KEY_KP_6]          = ImGuiKey_Keypad6;
    _vsg2imgui[vsg::KEY_KP_7]          = ImGuiKey_Keypad7;
    _vsg2imgui[vsg::KEY_KP_8]          = ImGuiKey_Keypad8;
    _vsg2imgui[vsg::KEY_KP_9]          = ImGuiKey_Keypad9;
    _vsg2imgui[vsg::KEY_F1]            = ImGuiKey_F1;
    _vsg2imgui[vsg::KEY_F2]            = ImGuiKey_F2;
    _vsg2imgui[vsg::KEY_F3]            = ImGuiKey_F3;
    _vsg2imgui[vsg::KEY_F4]            = ImGuiKey_F4;
    _vsg2imgui[vsg::KEY_F5]            = ImGuiKey_F5;
    _vsg2imgui[vsg::KEY_F6]            = ImGuiKey_F6;
    _vsg2imgui[vsg::KEY_F7]            = ImGuiKey_F7;
    _vsg2imgui[vsg::KEY_F8]            = ImGuiKey_F8;
    _vsg2imgui[vsg::KEY_F9]            = ImGuiKey_F9;
    _vsg2imgui[vsg::KEY_F10]           = ImGuiKey_F10;
    _vsg2imgui[vsg::KEY_F11]           = ImGuiKey_F11;
    _vsg2imgui[vsg::KEY_F12]           = ImGuiKey_F12;
    _vsg2imgui[vsg::KEY_Shift_L]       = ImGuiKey_LeftShift;
    _vsg2imgui[vsg::KEY_Shift_R]       = ImGuiKey_RightShift;
    _vsg2imgui[vsg::KEY_Control_L]     = ImGuiKey_LeftCtrl;
    _vsg2imgui[vsg::KEY_Control_R]     = ImGuiKey_RightCtrl;
    _vsg2imgui[vsg::KEY_Caps_Lock]     = ImGuiKey_CapsLock;
    _vsg2imgui[vsg::KEY_Meta_L]        = ImGuiKey_Menu;
    _vsg2imgui[vsg::KEY_Meta_R]        = ImGuiKey_Menu;
    _vsg2imgui[vsg::KEY_Alt_L]         = ImGuiKey_LeftAlt;
    _vsg2imgui[vsg::KEY_Alt_R]         = ImGuiKey_RightAlt;
    _vsg2imgui[vsg::KEY_Super_L]       = ImGuiKey_LeftSuper;
    _vsg2imgui[vsg::KEY_Super_R]       = ImGuiKey_RightSuper;
    // clang-format on
}

void SendEventsToImGui::apply(vsg::ButtonPressEvent& buttonPress)
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.WantCaptureMouse)
    {
        uint32_t button = _convertButton(buttonPress.button);
        io.AddMousePosEvent(static_cast<float>(buttonPress.x), static_cast<float>(buttonPress.y));
        io.AddMouseButtonEvent(button, true);

        buttonPress.handled = true;
    }
    else
    {
        _dragging = true;
    }
}

void SendEventsToImGui::apply(vsg::ButtonReleaseEvent& buttonRelease)
{
    ImGuiIO& io = ImGui::GetIO();
    if ((!_dragging) && io.WantCaptureMouse)
    {
        uint32_t button = _convertButton(buttonRelease.button);
        io.AddMousePosEvent(static_cast<float>(buttonRelease.x), static_cast<float>(buttonRelease.y));
        io.AddMouseButtonEvent(button, false);

        buttonRelease.handled = true;
    }

    _dragging = false;
}

void SendEventsToImGui::apply(vsg::MoveEvent& moveEvent)
{
    if (!_dragging)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(static_cast<float>(moveEvent.x), static_cast<float>(moveEvent.y));

        moveEvent.handled = io.WantCaptureMouse;
    }
}

void SendEventsToImGui::apply(vsg::ScrollWheelEvent& scrollWheel)
{
    if (!_dragging)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseWheelEvent(0.0, io.MouseWheel += scrollWheel.delta[1]);

        scrollWheel.handled = io.WantCaptureMouse;
    }
}

void SendEventsToImGui::_updateModifier(ImGuiIO& io, vsg::KeyModifier& modifier, bool pressed)
{
    if (modifier & vsg::MODKEY_Control)
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, pressed);
    }
    if (modifier & vsg::MODKEY_Shift)
    {
        io.AddKeyEvent(ImGuiMod_Shift, pressed);
    }
    if (modifier & vsg::MODKEY_Alt)
    {
        io.AddKeyEvent(ImGuiMod_Alt, pressed);
    }
    if (modifier & vsg::MODKEY_Meta)
    {
        io.AddKeyEvent(ImGuiMod_Super, pressed);
    }
}

void SendEventsToImGui::apply(vsg::KeyPressEvent& keyPress)
{
    ImGuiIO& io = ImGui::GetIO();

    // We should always pass the event to ImGui
    _updateModifier(io, keyPress.keyModifier, true);
    if (keyPress.keyModified >= vsg::KEY_KP_0 && keyPress.keyModified <= vsg::KEY_KP_9) keyPress.keyBase = keyPress.keyModified;
    auto itr = _vsg2imgui.find(keyPress.keyBase);
    auto imguiKey = ImGuiKey_None;
    if (itr != _vsg2imgui.end())
    {
        imguiKey = itr->second;
    }
    else
    {
        // This particular VSG key is not handled. If it should be, please raise an issue or a pull request.
        imguiKey = ImGuiKey_None;
    }
    io.AddKeyEvent(imguiKey, true);

    // Irrespective of whether we recognize the vsg key witin _vsg2imgui, if it's an ascii character, we add it as an input character.
    // If other characters should be allowed please raise an issue and pull request.
    // Adding as an input character on KeyPress allows user to repeat the values until release.
    if (uint16_t c = keyPress.keyModified; c > 0 && c < 255)
    {
        io.AddInputCharacter(c);
    }

    // If ImGui was expecting the keyboard event then we mark it as handled
    keyPress.handled = io.WantCaptureKeyboard;
}

void SendEventsToImGui::apply(vsg::KeyReleaseEvent& keyRelease)
{
    ImGuiIO& io = ImGui::GetIO();

    // We should always pass the event to ImGui
    _updateModifier(io, keyRelease.keyModifier, false);
    if (keyRelease.keyModified >= vsg::KEY_KP_0 && keyRelease.keyModified <= vsg::KEY_KP_9) keyRelease.keyBase = keyRelease.keyModified;
    auto itr = _vsg2imgui.find(keyRelease.keyBase);

    auto imguiKey = ImGuiKey_None;
    if (itr != _vsg2imgui.end())
    {
        imguiKey = itr->second;
    }
    else
    {
        // This particular VSG key is not handled. If it should be, please raise an issue or a pull request.
        imguiKey = ImGuiKey_None;
    }

    io.AddKeyEvent(imguiKey, false);

    // If ImGui was expecting the keyboard event then we mark it as handled
    keyRelease.handled = io.WantCaptureKeyboard;
}

void SendEventsToImGui::apply(vsg::ConfigureWindowEvent& configureWindow)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = static_cast<float>(configureWindow.width);
    io.DisplaySize.y = static_cast<float>(configureWindow.height);
}

void SendEventsToImGui::apply(vsg::FrameEvent& /*frame*/)
{
    ImGuiIO& io = ImGui::GetIO();

    auto t1 = std::chrono::high_resolution_clock::now();

    io.DeltaTime = std::chrono::duration_cast<std::chrono::duration<float>>(t1 - t0).count();

    t0 = t1;
}
