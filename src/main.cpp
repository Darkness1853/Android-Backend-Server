#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <thread>
#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <fstream>
#include <iomanip>
#include "imgui.h"
#include "implot.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

using namespace std;
using json = nlohmann::json;

struct location
{
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    string timeStr;
    int count = 0;
};

atomic<bool> run{true};

void save_to_json(const location& loc) {
    try {
        json j;
        j["latitude"] = loc.latitude;
        j["longitude"] = loc.longitude;
        j["altitude"] = loc.altitude;
        j["current_time"] = loc.timeStr;
        j["messages_count"] = loc.count;
        
        ofstream file("location.json", ios::app);
        if (file.is_open()) {
            file << j.dump()<< endl;
            file.close();
        }
    } catch (const exception& e) {
        cerr  << e.what() << endl;
    }
}

void run_gui(location *loc){
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow(
        "Server", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, SDL_WINDOW_OPENGL);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    while (run) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                run = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_None);

        {
            ImGui::Begin("Location Data");
            ImGui::Text("Latitude:  %.6f", loc->latitude);
            ImGui::Text("Longitude: %.6f", loc->longitude);
            ImGui::Text("Altitude:  %.2f", loc->altitude);
            ImGui::Text("Time: %s", loc->timeStr.c_str());
            ImGui::Text("Messages: %d", loc->count);
            
            ImGui::Separator();
            if (ImGui::Button("Save to JSON")) {
                save_to_json(*loc);
            }
            
            ImGui::End();
        }

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void run_server(location *loc){
    zmq::context_t ctx(1);
    zmq::socket_t sock(ctx, zmq::socket_type::rep);
    sock.bind("tcp://*:5050");
    cout << "Server connect port: 5050\n";
    
    int count = 0;
    
    while (run) {
        zmq::message_t request;
        
        if (sock.recv(request, zmq::recv_flags::none)) {
            string received_data(static_cast<char*>(request.data()), request.size());
            
            try {
                json j = json::parse(received_data);
                
                if (j.value("type", "") == "ping_client") {
                    sock.send(zmq::buffer(R"({"type":"ping_server"})"), zmq::send_flags::none);
                    continue;
                }
                
                loc->latitude = j["latitude"];
                loc->longitude = j["longitude"]; 
                loc->altitude = j["altitude"];
                loc->timeStr = j["current_time"];
                loc->count = ++count;
                
                save_to_json(*loc);
                
                json response;
                response["status"] = "ok";
                response["count"] = count;
                sock.send(zmq::buffer(response.dump()), zmq::send_flags::none);
                
                cout << "Data received #" << count << "\n";
                
            } catch (...) {
                sock.send(zmq::buffer(R"({"status":"error"})"), zmq::send_flags::none);
            }
        }
    }
}

int main() {
    location locationInfo;

    thread gui_thread(run_gui, &locationInfo);
    thread server_thread(run_server, &locationInfo);

    gui_thread.join();
    server_thread.join();
    return 0;
}