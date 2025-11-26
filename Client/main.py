import pygame
import socket
import sys
import time

# --- CONFIGURARE ---
SERVER_IP = '127.0.0.1' # Schimba cu IP-ul tau de LAN daca joci cu un coleg
SERVER_PORT = 54000
BUFFER_SIZE = 1024

WIDTH, HEIGHT = 800, 600
PLAYER_SIZE = 10

# Culori Neon
BLACK = (10, 10, 20)      # Fundal usor albastrui
NEON_CYAN = (0, 255, 255) # P1
NEON_PINK = (255, 20, 147)# P2
WHITE = (255, 255, 255)
RED = (255, 0, 0)

def draw_text_center(screen, text, color, y_offset=0):
    font = pygame.font.SysFont("consolas", 40, bold=True)
    render = font.render(text, True, color)
    rect = render.get_rect(center=(WIDTH//2, HEIGHT//2 + y_offset))
    screen.blit(render, rect)

def start_game():
    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Tron Multiplayer")

    # Init Socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_IP, SERVER_PORT))
        sock.setblocking(False)
    except Exception as e:
        print(f"Eroare conectare: {e}")
        return

    # Umplem ecranul cu negru la inceput
    screen.fill(BLACK)

    running = True
    game_over_state = False

    while running:
        # 1. INPUT
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            if event.type == pygame.KEYDOWN:
                cmd = None
                if event.key == pygame.K_w: cmd = "W"
                if event.key == pygame.K_s: cmd = "S"
                if event.key == pygame.K_a: cmd = "A"
                if event.key == pygame.K_d: cmd = "D"
                if event.key == pygame.K_r: cmd = "R" # Restart

                if cmd:
                    try:
                        sock.sendall(cmd.encode())
                    except:
                        pass

        # 2. NETWORK UPDATE
        try:
            data = sock.recv(BUFFER_SIZE)
            if data:
                # Format: "p1x,p1y,p2x,p2y,STATUS"
                msg = data.decode().split(',')
                if len(msg) >= 5:
                    p1x, p1y = int(msg[0]), int(msg[1])
                    p2x, p2y = int(msg[2]), int(msg[3])
                    status = msg[4]

                    # Logic: Daca status e RUNNING, desenam patratele
                    if status == "RUNNING":
                        if game_over_state:
                            # Daca tocmai am iesit din game over (Restart), stergem ecranul
                            screen.fill(BLACK)
                            game_over_state = False

                        pygame.draw.rect(screen, NEON_CYAN, (p1x, p1y, PLAYER_SIZE, PLAYER_SIZE))
                        pygame.draw.rect(screen, NEON_PINK, (p2x, p2y, PLAYER_SIZE, PLAYER_SIZE))

                    elif "WIN" in status or "DRAW" in status:
                        game_over_state = True
                        if status == "P1_WINS":
                            draw_text_center(screen, "PLAYER 1 WINS!", NEON_CYAN)
                        elif status == "P2_WINS":
                            draw_text_center(screen, "PLAYER 2 WINS!", NEON_PINK)
                        elif status == "DRAW":
                            draw_text_center(screen, "DRAW!", WHITE)

                        draw_text_center(screen, "Press 'R' to Restart", WHITE, 50)

                    elif status == "WAITING":
                        draw_text_center(screen, "Waiting for Player 2...", WHITE)

        except BlockingIOError:
            pass
        except Exception as e:
            print(f"Server closed: {e}")
            running = False

        pygame.display.flip()
        # Nu limitam FPS cu clock.tick prea drastic pentru ca socket-ul dicteaza ritmul
        time.sleep(0.01)

    sock.close()
    pygame.quit()

if __name__ == "__main__":
    start_game()