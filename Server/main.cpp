#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#inlcude <sys/socket.h>

using namespace std;

#define PORT 54000
#define BUFFER_SIZE 1024

void handle_client(int client_socket, int player_id) {
    cout << "[Server] Player " << player_id << " connected on " << client_socket << endl;
    char buffer[BUFFER_SIZE];

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);    //memset umple bufferul cu 0-uri
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);   //nu trece mai departe pana nu primeste ceva de la client
        if (bytes_received <= 0) {    //0-clientul a inchis conexiunea, <0(-1)-a aparut o eroare de retea
            cout << "[Server] Player " << player_id << " disconnected" << endl;
            break;
        }
        cout << "[Player " << player_id << "] received: " << buffer << endl;
        string answer = "[Server] received answer: " + string(buffer);
        send(client_socket, answer.c_str(), answer.size(), 0);
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

    cout << "[Server] waiting for connection on port " << PORT << "..." << endl;

    int player_count = 0;
    vector<thread> threads;

    while (true) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept error");
            continue;
        }
        player_count++;
        threads.emplace_back(thread(handle_client, new_socket, player_count));
        threads.back().detach();
    }
    return 0;
}