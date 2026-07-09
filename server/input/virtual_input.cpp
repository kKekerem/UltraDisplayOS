#include "server/input/virtual_input.hpp"
#include <iostream>
#include <winsock2.h>

#ifndef EV_KEY
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#endif

// A minimal replication of linux input_event for decoding
struct linux_input_event {
    struct timeval time;
    unsigned short type;
    unsigned short code;
    unsigned int value;
};

namespace ud {

VirtualInput::VirtualInput() {}

VirtualInput::~VirtualInput() {}

Result<void> VirtualInput::init() {
    // In a real application, we might initialize virtual drivers here (e.g. ViGEm for gamepads)
    return {};
}

void VirtualInput::inject_event(const InputEvent& event) {
    if (event.payload.size() < sizeof(linux_input_event)) return;

    if (event.type == InputEvent::Type::Keyboard) {
        inject_keyboard(event);
    } else if (event.type == InputEvent::Type::Mouse) {
        inject_mouse(event);
    } else if (event.type == InputEvent::Type::Gamepad) {
        inject_gamepad(event);
    }
}

void VirtualInput::inject_keyboard(const InputEvent& event) {
    const linux_input_event* ev = reinterpret_cast<const linux_input_event*>(event.payload.data());
    
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    
    // Very naive mapping, assumes linux keycode is close to Windows VK or scan code
    // A proper implementation would use a translation table
    input.ki.wScan = ev->code; 
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;
    
    if (ev->value == 0) {
        input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    } else {
        input.ki.dwFlags = KEYEVENTF_SCANCODE;
    }

    SendInput(1, &input, sizeof(INPUT));
}

void VirtualInput::inject_mouse(const InputEvent& event) {
    const linux_input_event* ev = reinterpret_cast<const linux_input_event*>(event.payload.data());
    
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    
    // Linux EV_REL mapping
    // REL_X = 0, REL_Y = 1, REL_WHEEL = 8
    if (ev->code == 0) { // REL_X
        input.mi.dx = ev->value;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
    } else if (ev->code == 1) { // REL_Y
        input.mi.dy = ev->value;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
    } else if (ev->code == 8) { // REL_WHEEL
        input.mi.mouseData = ev->value * 120; // Windows wheel delta
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    } else {
        return; // Other rel events ignored for now
    }
    
    SendInput(1, &input, sizeof(INPUT));
}

void VirtualInput::inject_gamepad(const InputEvent& event) {
    // Native SendInput doesn't support gamepad well, typically XInput or ViGEm bus is used here
    // Leaving as implemented but non-functional without ViGEm dependency
    (void)event;
}

} // namespace ud
