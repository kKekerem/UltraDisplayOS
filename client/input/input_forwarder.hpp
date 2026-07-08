#pragma once

#include "shared/util/result.hpp"
#include "shared/protocol/messages.hpp"
#include <functional>
#include <vector>

namespace ud {

struct InputEvent {
    enum class Type {
        Keyboard,
        Mouse,
        Gamepad
    } type;
    
    // Union or payload struct would go here based on event type
    std::vector<uint8_t> payload;
};

class InputForwarder {
public:
    InputForwarder();
    ~InputForwarder();

    // Initializes libevdev listeners on /dev/input/event* nodes
    Result<void> init();

    // Starts listening loop
    void start(std::function<void(const InputEvent&)> send_callback);
    void stop();

private:
    std::vector<int> evdev_fds_;
    bool running_{false};
    
    void poll_loop(std::function<void(const InputEvent&)> send_callback);
};

} // namespace ud
