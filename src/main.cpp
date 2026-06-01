#include "location.hpp"
#include "server.hpp"
#include "gui.hpp"
#include <thread>

int main() {
    Location locationInfo;

    std::thread gui_thread(run_gui, &locationInfo);
    std::thread server_thread(run_server, &locationInfo, 5050);

    gui_thread.join();
    server_thread.join();

    return 0;
}