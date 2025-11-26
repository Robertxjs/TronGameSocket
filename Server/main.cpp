#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#include <chrono>

// --- CONFIGURARE ---
#define TCP_PORT 54000
#define UDP_PORT 54001
#define BUFFER_SIZE 1024

const int WIDTH = 800;
const int HEIGHT = 600;
const int SPEED = 5;
const int PLAYER_SIZE = 20;

bool game_map[WIDTH][HEIGHT]; 

enum Direction { STOP = 0, UP, DOWN, LEFT, RIGHT };

// --- STARE JOC ---
int p1_x = 100, p1_y = 300; Direction dir_p1 = RIGHT;
int p2_x = 600, p2_y = 300; Direction dir_p2 = LEFT;

int score_p1 = 0;
int score_p2 = 0;
bool game_reset_flag = false;

std::mutex game_mutex;
std::vector<struct sockaddr_in> udp_clients;

// --- FUNCTII ---

// Trimite scorul prin UDP
void broadcast_score() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    std::string msg = "SCORE:" + std::to_string(score_p1) + " - " + std::to_string(score_p2);
    
    // Trimitem la toti clientii abonati
    for (const auto& client : udp_clients) {
        sendto(sock, msg.c_str(), msg.size(), 0, (struct sockaddr*)&client, sizeof(client));
    }
    close(sock);
}

void reset_game_state() {
    p1_x = 100; p1_y = 300; dir_p1 = RIGHT;
    p2_x = 600; p2_y = 300; dir_p2 = LEFT;
    memset(game_map, 0, sizeof(game_map));
    game_reset_flag = true;
}

bool check_collision(int x, int y) {
    if (x < 0 || x + PLAYER_SIZE > WIDTH || y < 0 || y + PLAYER_SIZE > HEIGHT) return true;
    int check_x = x + PLAYER_SIZE / 2;
    int check_y = y + PLAYER_SIZE / 2;
    if (game_map[check_x][check_y]) return true;
    return false;
}

void mark_trail(int x, int y) {
    for (int i = x + 5; i < x + PLAYER_SIZE - 5; ++i) {
        for (int j = y + 5; j < y + PLAYER_SIZE - 5; ++j) {
            if (i >= 0 && i < WIDTH && j >= 0 && j < HEIGHT) {
                game_map[i][j] = true;
            }
        }
    }
}

// --- FIZICA JOCULUI (ENGINE) ---
void game_logic_thread() {
    std::cout << "[Server] Engine Pornit." << std::endl;
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); 

        game_mutex.lock();

        // Calcul pozitii viitoare
        int next_p1_x = p1_x, next_p1_y = p1_y;
        int next_p2_x = p2_x, next_p2_y = p2_y;

        if (dir_p1 == UP) next_p1_y -= SPEED;
        if (dir_p1 == DOWN) next_p1_y += SPEED;
        if (dir_p1 == LEFT) next_p1_x -= SPEED;
        if (dir_p1 == RIGHT) next_p1_x += SPEED;

        if (dir_p2 == UP) next_p2_y -= SPEED;
        if (dir_p2 == DOWN) next_p2_y += SPEED;
        if (dir_p2 == LEFT) next_p2_x -= SPEED;
        if (dir_p2 == RIGHT) next_p2_x += SPEED;

        bool p1_crashed = check_collision(next_p1_x, next_p1_y);
        bool p2_crashed = check_collision(next_p2_x, next_p2_y);

        if (abs(next_p1_x - next_p2_x) < PLAYER_SIZE && abs(next_p1_y - next_p2_y) < PLAYER_SIZE) {
            p1_crashed = true; p2_crashed = true;
        }

        if (p1_crashed || p2_crashed) {
            // Logica Scori
            if (p1_crashed && !p2_crashed) score_p2++;
            else if (!p1_crashed && p2_crashed) score_p1++;
            
            // 1. Resetam pozitiile intern
            reset_game_state();

            // 2. Anuntam Scorul (Doar o singura data)
            broadcast_score();
            
            // 3. Deblocam mutexul ca sa trimitem datele de reset si la clienti prin TCP
            game_mutex.unlock();

            // 4. PAUZA (COOLDOWN) - Aici era problema!
            // Asteptam 2 secunde inainte sa inceapa runda urmatoare
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } 
        else {
            mark_trail(p1_x, p1_y);
            mark_trail(p2_x, p2_y);
            p1_x = next_p1_x; p1_y = next_p1_y;
            p2_x = next_p2_x; p2_y = next_p2_y;
            game_mutex.unlock();
        }
    }
}

// --- UDP LISTENER ---
void udp_listener_thread() {
    int udp_sock;
    struct sockaddr_in server_addr, client_addr;
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return;
    int opt = 1;
    setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_PORT);
    if (bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) return;

    char buffer[BUFFER_SIZE];
    while(true) {
        socklen_t len = sizeof(client_addr);
        int n = recvfrom(udp_sock, buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &len);
        buffer[n] = '\0';
        if (strstr(buffer, "SUBSCRIBE")) {
            std::lock_guard<std::mutex> lock(game_mutex);
            bool exists = false;
            for(auto& c : udp_clients) {
                if(c.sin_addr.s_addr == client_addr.sin_addr.s_addr && c.sin_port == client_addr.sin_port) exists = true;
            }
            if (!exists) udp_clients.push_back(client_addr);
            
            // Trimitem scorul curent imediat la conectare
            std::string score_msg = "SCORE:" + std::to_string(score_p1) + " - " + std::to_string(score_p2);
            sendto(udp_sock, score_msg.c_str(), score_msg.size(), 0, (struct sockaddr*)&client_addr, len);
        }
    }
}

// --- TCP CLIENT ---
void handle_tcp_client(int client_socket, int player_id) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) break;
        char cmd = buffer[0];
        game_mutex.lock();
        if (player_id == 1) {
            if (cmd == 'S' && dir_p1 != DOWN) dir_p1 = UP;
            if (cmd == 'J' && dir_p1 != UP) dir_p1 = DOWN;
            if (cmd == 'A' && dir_p1 != RIGHT) dir_p1 = LEFT;
            if (cmd == 'D' && dir_p1 != LEFT) dir_p1 = RIGHT;
        } else if (player_id == 2) {
            if (cmd == 'S' && dir_p2 != DOWN) dir_p2 = UP;
            if (cmd == 'J' && dir_p2 != UP) dir_p2 = DOWN;
            if (cmd == 'A' && dir_p2 != RIGHT) dir_p2 = LEFT;
            if (cmd == 'D' && dir_p2 != LEFT) dir_p2 = RIGHT;
        }
        std::string raspuns = std::to_string(p1_x) + "," + std::to_string(p1_y) + "," +
                              std::to_string(p2_x) + "," + std::to_string(p2_y) + "," +
                              (game_reset_flag ? "1" : "0");
        if (game_reset_flag) game_reset_flag = false;
        game_mutex.unlock();
        send(client_socket, raspuns.c_str(), raspuns.size(), 0);
    }
    close(client_socket);
}

int main() {
    memset(game_map, 0, sizeof(game_map));
    std::thread t_udp(udp_listener_thread); t_udp.detach();
    std::thread t_physics(game_logic_thread); t_physics.detach();
    int server_fd, new_socket;
    struct sockaddr_in address; int addrlen = sizeof(address);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(EXIT_FAILURE);
    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(TCP_PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) exit(EXIT_FAILURE);
    if (listen(server_fd, 3) < 0) exit(EXIT_FAILURE);
    std::cout << "[Server] Gata de joc pe portul " << TCP_PORT << std::endl;
    int player_count = 0;
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) continue;
        player_count++;
        if (player_count > 2) { close(new_socket); player_count--; continue; }
        std::thread t(handle_tcp_client, new_socket, player_count); t.detach();
    }
    return 0;
}