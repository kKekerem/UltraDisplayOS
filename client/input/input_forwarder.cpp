#include "client/input/input_forwarder.hpp"
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <dirent.h>
#include <iostream>
#include <string.h>

namespace ud {

InputForwarder::InputForwarder() {}

InputForwarder::~InputForwarder() {
    stop();
    for (int fd : evdev_fds_) {
        close(fd);
    }
}

Result<void> InputForwarder::init() {
    DIR* dir = opendir("/dev/input");
    if (!dir) return Result<void>::create_error("Cannot open /dev/input");

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            std::string path = std::string("/dev/input/") + entry->d_name;
            int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                struct libevdev* dev = nullptr;
                int rc = libevdev_new_from_fd(fd, &dev);
                if (rc < 0) {
                    close(fd);
                    continue;
                }
                
                // Only keep devices with keyboard, mouse or gamepad capabilities
                if (libevdev_has_event_type(dev, EV_KEY) || libevdev_has_event_type(dev, EV_REL) || libevdev_has_event_type(dev, EV_ABS)) {
                    evdev_fds_.push_back(fd);
                } else {
                    close(fd);
                }
                libevdev_free(dev);
            }
        }
    }
    closedir(dir);

    if (evdev_fds_.empty()) {
        return Result<void>::create_error("No valid input devices found");
    }

    return Result<void>::create_success();
}

void InputForwarder::start(std::function<void(const InputEvent&)> send_callback) {
    if (running_) return;
    running_ = true;
    poll_loop(send_callback);
}

void InputForwarder::stop() {
    running_ = false;
}

void InputForwarder::poll_loop(std::function<void(const InputEvent&)> send_callback) {
    std::vector<struct pollfd> fds;
    for (int fd : evdev_fds_) {
        fds.push_back({fd, POLLIN, 0});
    }

    while (running_) {
        int ret = poll(fds.data(), fds.size(), 100);
        if (ret > 0) {
            for (size_t i = 0; i < fds.size(); ++i) {
                if (fds[i].revents & POLLIN) {
                    struct input_event ev;
                    while (read(fds[i].fd, &ev, sizeof(ev)) > 0) {
                        InputEvent out_event;
                        // Map type based on simple heuristic, in reality more specific mapping needed
                        if (ev.type == EV_KEY) {
                            out_event.type = InputEvent::Type::Keyboard;
                        } else if (ev.type == EV_REL) {
                            out_event.type = InputEvent::Type::Mouse;
                        } else if (ev.type == EV_ABS) {
                            out_event.type = InputEvent::Type::Gamepad;
                        } else {
                            continue;
                        }
                        
                        out_event.payload.resize(sizeof(struct input_event));
                        memcpy(out_event.payload.data(), &ev, sizeof(ev));
                        
                        send_callback(out_event);
                    }
                }
            }
        }
    }
}

} // namespace ud
