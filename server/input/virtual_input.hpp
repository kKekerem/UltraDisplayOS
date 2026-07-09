#pragma once

#include "shared/util/result.hpp"
#include "client/input/input_forwarder.hpp" // For InputEvent struct definition


#include <windows.h>

namespace ud {

class VirtualInput {
public:
    VirtualInput();
    ~VirtualInput();

    Result<void> init();

    // Injects the received event into the Windows input queue via SendInput
    void inject_event(const InputEvent& event);

private:
    void inject_keyboard(const InputEvent& event);
    void inject_mouse(const InputEvent& event);
    void inject_gamepad(const InputEvent& event);
};

} // namespace ud
