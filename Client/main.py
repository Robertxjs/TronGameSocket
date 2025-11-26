import pygame
import socket
import sys
import threading

SERVER_IP = '127.0.0.1'
TCP_PORT = 54000
UDP_PORT = 54001
BUFFER_SIZE = 1024

WIDTH, HEIGHT = 800, 600
PLAYER_SIZE = 20
BLACK = (0, 0, 0)
GRID_COLOR = (20, 20, 20)
P1_COLOR = (0, 255, 255)
P2_COLOR = (255, 0, 0)
WHITE = (255, 255, 255)

# Variabila protejata de un lock simplu (deși GIL ajuta in Python, e bine sa fim siguri)
current_score = "0 - 0"

def udp_listener():
    global current_score
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Timeout mic pentru a nu bloca la iesire
        sock.settimeout(2.0)

        # Abonare
        msg = "SUBSCRIBE".encode('utf-8')
        sock.sendto(msg, (SERVER_IP, UDP_PORT))
        print("[Client UDP] Abonat la scor.")

        while True:
            try:
                data, _ = sock.recvfrom(BUFFER_SIZE)
                text = data.decode('utf-8')
                if text.startswith("SCORE:"):
                    # Format: "SCORE:3 - 1"
                    # Luam tot ce e dupa "SCORE:"
                    current_score = text[6:]
            except socket.timeout:
                continue
            except Exception as e:
                print(f"[UDP Error] {e}")
                break
    except Exception as e:
        print(f"[UDP Init Error] {e}")

def draw_grid(screen):
    for x in range(0, WIDTH, 40):
        pygame.draw.line(screen, GRID_COLOR, (x, 0), (x, HEIGHT))
    for y in range(0, HEIGHT, 40):
        pygame.draw.line(screen, GRID_COLOR, (0, y), (WIDTH, y))

def main():
    global current_score
    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Tron Multiplayer")
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("Arial", 40, bold=True)

    try:
        tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_sock.connect((SERVER_IP, TCP_PORT))
    except Exception as e:
        print(f"Server TCP Offline: {e}")
        return

    t_udp = threading.Thread(target=udp_listener, daemon=True)
    t_udp.start()

    trail_p1 = []
    trail_p2 = []
    p1_x, p1_y = 100, 300
    p2_x, p2_y = 600, 300

    running = True
    while running:
        cmd = "N"
        for event in pygame.event.get():
            if event.type == pygame.QUIT: running = False
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_w or event.key == pygame.K_UP: cmd = "S"
                if event.key == pygame.K_s or event.key == pygame.K_DOWN: cmd = "J"
                if event.key == pygame.K_a or event.key == pygame.K_LEFT: cmd = "A"
                if event.key == pygame.K_d or event.key == pygame.K_RIGHT: cmd = "D"

        try:
            tcp_sock.sendall(cmd.encode('utf-8'))
            data = tcp_sock.recv(BUFFER_SIZE)
            if data:
                parts = data.decode('utf-8').split(',')
                if len(parts) >= 5:
                    nx1, ny1 = int(parts[0]), int(parts[1])
                    nx2, ny2 = int(parts[2]), int(parts[3])
                    reset_flag = int(parts[4])

                    if reset_flag == 1:
                        trail_p1.clear()
                        trail_p2.clear()

                    if nx1 != p1_x or ny1 != p1_y:
                        trail_p1.append((nx1 + PLAYER_SIZE//2, ny1 + PLAYER_SIZE//2))
                    if nx2 != p2_x or ny2 != p2_y:
                        trail_p2.append((nx2 + PLAYER_SIZE//2, ny2 + PLAYER_SIZE//2))

                    p1_x, p1_y = nx1, ny1
                    p2_x, p2_y = nx2, ny2
            else:
                break
        except:
            break

        screen.fill(BLACK)
        draw_grid(screen)

        if len(trail_p1) > 1: pygame.draw.lines(screen, P1_COLOR, False, trail_p1, 3)
        if len(trail_p2) > 1: pygame.draw.lines(screen, P2_COLOR, False, trail_p2, 3)

        pygame.draw.rect(screen, P1_COLOR, (p1_x, p1_y, PLAYER_SIZE, PLAYER_SIZE))
        pygame.draw.rect(screen, P2_COLOR, (p2_x, p2_y, PLAYER_SIZE, PLAYER_SIZE))

        # Afisare SCOR
        txt = font.render(f"SCORE: {current_score}", True, WHITE)
        screen.blit(txt, (WIDTH // 2 - 100, 10))

        pygame.display.flip()
        clock.tick(60)

    tcp_sock.close()
    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()