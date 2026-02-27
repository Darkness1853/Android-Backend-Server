#include <string.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "zmq.h"

using namespace std;

struct location
{
    float latitude;
    float longitude;
    float altitude;
};

void gui(locaiton *loc){

}

void server(locaiton *loc) {
    int socket(int domain, int type, int protocol);
    int server = socket(AF_INET, SOCK_STREAM, 0);
}


int main(){

    static location locationInfo;

    thread gui_thread(gui, &locationInfo);

    thread server_thread(server, &locationInfo);

    gui.join();
    server.join();

    return 0;
}