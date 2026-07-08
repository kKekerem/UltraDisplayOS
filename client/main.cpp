#include "shared/util/log.hpp"
#include "shared/util/thread.hpp"
#include "client/tui/app.hpp"

using namespace ud;

int main() {
    init_logging(false /* is_server */, true /* enable_debug */);
    set_thread_name("UD_Client");

    UD_LOG_INFO("main", "UltraDisplay Client starting...");

    TuiApp app;
    if (!app.init()) {
        UD_LOG_ERROR("main", "Failed to initialize TUI");
        return 1;
    }

    app.run();

    UD_LOG_INFO("main", "UltraDisplay Client shutting down.");
    return 0;
}
