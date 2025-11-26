#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <mutex>
#include <chrono>
#include <string>

#define PORT 54000
#define BUFFER_SIZE 1024
#define WIDTH 800
#define HEIGHT 600
#define PLAYER_SIZE 10

struct Player {
    int x, y;
    int dir_x, dir_y;
    bool active;
    bool alive;
};

bool game_map[WIDTH][HEIGHT];

Player p1 = {100, 300, 1, 0, false, true};
Player p2 = {700, 300, -1, 0, false, true};

// SCORUL (Global)
int score1 = 0;
int score2 = 0;

int speed = 10;
bool game_running = false;
std::string status_message = "WAITING";

std::mutex game_mutex;

// Resetam doar pozitiile, NU si scorul
void reset_round() {
    memset(game_map, 0, sizeof(game_map));
    p1 = {100, 300, 1, 0, true, true};
    p2 = {700, 300, -1, 0, true, true};
    game_running = true;
    status_message = "RUNNING";
    std::cout << "[Game] Runda noua! Scor: " << score1 << " - " << score2 << std::endl;
}

void game_logic_loop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::lock_guard<std::mutex> lock(game_mutex);
        if (!game_running) continue;

        // 1. Miscare
        p1.x += p1.dir_x * speed; p1.y += p1.dir_y * speed;
        p2.x += p2.dir_x * speed; p2.y += p2.dir_y * speed;

        // 2. Coliziuni Pereti
        if (p1.x < 0 || p1.x >= WIDTH || p1.y < 0 || p1.y >= HEIGHT) p1.alive = false;
        if (p2.x < 0 || p2.x >= WIDTH || p2.y < 0 || p2.y >= HEIGHT) p2.alive = false;

        // 3. Coliziuni Urme
        if (p1.alive && game_map[p1.x][p1.y]) p1.alive = false;
        if (p2.alive && game_map[p2.x][p2.y]) p2.alive = false;

        // 4. Marcare Urme
        if (p1.alive) {
            for(int i=0; i<PLAYER_SIZE; i++) for(int j=0; j<PLAYER_SIZE; j++)
                if(p1.x+i < WIDTH && p1.y+j < HEIGHT) game_map[p1.x+i][p1.y+j] = true;
        }
        if (p2.alive) {
            for(int i=0; i<PLAYER_SIZE; i++) for(int j=0; j<PLAYER_SIZE; j++)
                if(p2.x+i < WIDTH && p2.y+j < HEIGHT) game_map[p2.x+i][p2.y+j] = true;
        }

        // 5. Verificare Castigator & Actualizare Scor
        if (!p1.alive && !p2.alive) {
            status_message = "DRAW";
            game_running = false;
        }
        else if (!p1.alive) {
            status_message = "P2_WINS";
            score2++; // Punct pentru Player 2
            game_running = false;
        }
        else if (!p2.alive) {
            status_message = "P1_WINS";
            score1++; // Punct pentru Player 1
            game_running = false;
        }
    }
}

void handle_client(int client_socket, int player_id) {
    struct timeval tv;
    tv.tv_sec = 0; tv.tv_usec = 100000;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    char buffer[BUFFER_SIZE];

    {
        std::lock_guard<std::mutex> lock(game_mutex);
        if (player_id == 1) p1.active = true;
        if (player_id == 2) p2.active = true;
        if (p1.active && p2.active && !game_running) reset_round();
    }

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);

        if (bytes_received <= 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cout << "[Server] Player " << player_id << " deconectat." << std::endl;
            break;
        }
        else if (bytes_received > 0) {
            char cmd = buffer[0];
            std::lock_guard<std::mutex> lock(game_mutex);

            if (game_running) {
                if (player_id == 1) {
                    if (cmd == 'W' && p1.dir_y == 0) { p1.dir_x = 0; p1.dir_y = -1; }
                    if (cmd == 'S' && p1.dir_y == 0) { p1.dir_x = 0; p1.dir_y = 1; }
                    if (cmd == 'A' && p1.dir_x == 0) { p1.dir_x = -1; p1.dir_y = 0; }
                    if (cmd == 'D' && p1.dir_x == 0) { p1.dir_x = 1; p1.dir_y = 0; }
                }
                else if (player_id == 2) {
                    if (cmd == 'W' && p2.dir_y == 0) { p2.dir_x = 0; p2.dir_y = -1; }
                    if (cmd == 'S' && p2.dir_y == 0) { p2.dir_x = 0; p2.dir_y = 1; }
                    if (cmd == 'A' && p2.dir_x == 0) { p2.dir_x = -1; p2.dir_y = 0; }
                    if (cmd == 'D' && p2.dir_x == 0) { p2.dir_x = 1; p2.dir_y = 0; }
                }
            }
            if (cmd == 'R' && !game_running) reset_round();
        }

        // --- TRIMITEM TOTUL PE ACELASI CANAL ---
        std::string msg;
        {
            std::lock_guard<std::mutex> lock(game_mutex);
            msg = std::to_string(p1.x) + "," + std::to_string(p1.y) + "," +
                  std::to_string(p2.x) + "," + std::to_string(p2.y) + "," +
                  status_message + "," +
                  std::to_string(score1) + "," + std::to_string(score2);
        }
        send(client_socket, msg.c_str(), msg.size(), 0);
    }

    // Cleanup
    {
        std::lock_guard<std::mutex> lock(game_mutex);
        if(player_id==1) p1.active=false;
        if(player_id==2) p2.active=false;
        game_running=false;
        status_message = "WAITING";
        score1 = 0; score2 = 0; // Resetam scorul cand pleaca jucatorii
    }
    close(client_socket);
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { perror("Socket failed"); exit(EXIT_FAILURE); }
    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("Bind failed"); exit(EXIT_FAILURE); }
    if (listen(server_fd, 3) < 0) { perror("Listen failed"); exit(EXIT_FAILURE); }

    std::cout << "[Server] Server Pornit. Astept conexiuni..." << std::endl;

    std::thread(game_logic_loop).detach();

    int player_count = 0;
    while (true) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) continue;
        player_count++;
        int id = (player_count % 2 == 0) ? 2 : 1;
        std::thread(handle_client, new_socket, id).detach();
    }
    return 0;
}