#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <mutex> // NECESAR PENTRU CONCURENTA SIGURA

#define PORT 54000
#define BUFFER_SIZE 1024

// --- STAREA JOCULUI (GLOBAL) ---
// Pentru simplitate, folosim coordonate hardcodate pentru start
int p1_x = 100, p1_y = 300; // Jucatorul 1 (Stanga)
int p2_x = 500, p2_y = 300; // Jucatorul 2 (Dreapta)
int viteza = 10;            // Cati pixeli se misca la fiecare apasare

// Mutex-ul protejeaza variabilele globale cand scriu mai multi deodata
std::mutex game_mutex;

void handle_client(int client_socket, int player_id) {
    std::cout << "[Server] Player " << player_id << " conectat." << std::endl;
    char buffer[BUFFER_SIZE];

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);

        if (bytes_received <= 0) {
            std::cout << "[Server] Player " << player_id << " a iesit." << std::endl;
            break;
        }

        // Interpretam comanda (doar prima litera conteaza: S, J, D, A)
        char comanda = buffer[0];

        // --- ZONA CRITICA (Blocam accesul altora cat timp modificam noi) ---
        game_mutex.lock();

        if (player_id == 1) {
            if (comanda == 'S') p1_y -= viteza; // Sus (Y scade)
            if (comanda == 'J') p1_y += viteza; // Jos
            if (comanda == 'A') p1_x -= viteza; // Stanga
            if (comanda == 'D') p1_x += viteza; // Dreapta
        }
        else if (player_id == 2) {
            if (comanda == 'S') p2_y -= viteza;
            if (comanda == 'J') p2_y += viteza;
            if (comanda == 'A') p2_x -= viteza;
            if (comanda == 'D') p2_x += viteza;
        }

        // Construim raspunsul cu AMBELE pozitii: "x1,y1,x2,y2"
        // Exemplu: "100,290,500,300"
        std::string raspuns = std::to_string(p1_x) + "," + std::to_string(p1_y) + "," +
                              std::to_string(p2_x) + "," + std::to_string(p2_y);

        game_mutex.unlock();
        // --- FINAL ZONA CRITICA ---

        // Trimitem noua stare inapoi la client
        send(client_socket, raspuns.c_str(), raspuns.size(), 0);
    }

    close(client_socket);
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Setam optiunea SO_REUSEADDR ca sa putem reporni serverul rapid fara eroare "Address in use"
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[Server] TRON Game Server pornit pe portul " << PORT << "..." << std::endl;

    int player_count = 0;
    std::vector<std::thread> threads;

    while (true) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept error");
            continue;
        }

        player_count++;
        // Limitare simpla la 2 jucatori pentru moment
        if (player_count > 2) {
            std::cout << "[Info] Server plin. Resping conexiunea." << std::endl;
            close(new_socket);
            player_count--; // Decrementam inapoi
            continue;
        }

        threads.emplace_back(std::thread(handle_client, new_socket, player_count));
        threads.back().detach();
    }
    return 0;
}