import pygame
import socket
import sys

# --- CONFIGURARE ---
SERVER_IP = '127.0.0.1'
SERVER_PORT = 54000
BUFFER_SIZE = 1024

# Culori și Dimensiuni
WIDTH, HEIGHT = 800, 600
BLACK = (0, 0, 0)
NEON_BLUE = (0, 255, 255) # Player 1 (Tron Style)
NEON_RED = (255, 0, 0)    # Player 2 (Enemy Style)
PLAYER_SIZE = 20

def start_visual_game():
    # 1. Init Pygame
    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Tron Multiplayer - C++ Server")
    clock = pygame.time.Clock()

    # 2. Conectare
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_IP, SERVER_PORT))
        sock.setblocking(False) # Non-blocant ca să meargă animația fluid
        print("[Client] Conectat! Folosește WASD.")
    except Exception as e:
        print(f"[Eroare] Nu mă pot conecta la server: {e}")
        return

    # Variabile locale pentru poziții (start default)
    p1_x, p1_y = 100, 300
    p2_x, p2_y = 500, 300

    running = True
    while running:
        # --- A. INPUT ---
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            if event.type == pygame.KEYDOWN:
                msg = None
                if event.key == pygame.K_w: msg = "S" # Sus
                if event.key == pygame.K_s: msg = "J" # Jos
                if event.key == pygame.K_a: msg = "A" # Stanga (Left)
                if event.key == pygame.K_d: msg = "D" # Dreapta (Right)

                # Trimitem comanda la server
                if msg:
                    try:
                        sock.sendall(msg.encode('utf-8'))
                    except:
                        pass

        # --- B. UPDATE DE LA SERVER ---
        try:
            data = sock.recv(BUFFER_SIZE)
            if data:
                mesaj = data.decode('utf-8')
                # Serverul trimite: "x1,y1,x2,y2" (ex: "100,290,500,300")
                try:
                    coords = mesaj.split(',') # Spargem textul la virgule
                    if len(coords) == 4:
                        p1_x = int(coords[0])
                        p1_y = int(coords[1])
                        p2_x = int(coords[2])
                        p2_y = int(coords[3])
                except ValueError:
                    pass # Dacă pachetul e corupt, ignorăm frame-ul ăsta
        except BlockingIOError:
            pass # Nu am primit nimic nou, continuăm desenarea
        except Exception:
            print("[Info] Deconectat.")
            running = False

        # --- C. DESENARE ---
        screen.fill(BLACK) # 1. Ștergem ecranul

        # 2. Desenăm Player 1 (Albastru)
        pygame.draw.rect(screen, NEON_BLUE, (p1_x, p1_y, PLAYER_SIZE, PLAYER_SIZE))

        # 3. Desenăm Player 2 (Roșu)
        pygame.draw.rect(screen, NEON_RED, (p2_x, p2_y, PLAYER_SIZE, PLAYER_SIZE))

        pygame.display.flip() # 4. Afișăm
        clock.tick(60) # 60 FPS

    sock.close()
    pygame.quit()

if __name__ == "__main__":
    start_visual_game()