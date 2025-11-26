import pygame
import socket
import sys
import time

SERVER_IP = '127.0.0.1'
SERVER_PORT = 54000
BUFFER_SIZE = 1024

WIDTH, HEIGHT = 800, 600
PLAYER_SIZE = 10

BLACK = (10, 10, 20)
NEON_CYAN = (0, 255, 255) # P1
NEON_PINK = (255, 20, 147)# P2
WHITE = (255, 255, 255)

def draw_text_center(screen, text, color, y_offset=0):
    try: font = pygame.font.SysFont("consolas", 40, bold=True)
    except: font = pygame.font.SysFont(None, 40)
    render = font.render(text, True, color)
    rect = render.get_rect(center=(WIDTH//2, HEIGHT//2 + y_offset))
    screen.blit(render, rect)

def draw_score(screen, s1, s2):
    # Desenam o bara sus pentru scor
    pygame.draw.rect(screen, BLACK, (0, 0, WIDTH, 40))
    try: font = pygame.font.SysFont("consolas", 30, bold=True)
    except: font = pygame.font.SysFont(None, 30)

    text = f"CYAN: {s1}   vs   PINK: {s2}"
    render = font.render(text, True, WHITE)
    rect = render.get_rect(center=(WIDTH//2, 20))
    screen.blit(render, rect)

def start_game():
    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Tron Multiplayer")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_IP, SERVER_PORT))
        sock.setblocking(False)
    except Exception as e:
        print(f"Eroare conectare: {e}")
        return

    screen.fill(BLACK)
    running = True
    game_over_state = False

    # Variabile scor
    score1, score2 = 0, 0

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
                if event.key == pygame.K_r: cmd = "R"

                if cmd:
                    try: sock.sendall(cmd.encode())
                    except: pass

        # 2. NETWORK UPDATE
        try:
            data = sock.recv(BUFFER_SIZE)
            if data:
                msg_str = data.decode()
                msg = msg_str.split(',')

                if len(msg) >= 7: # Acum asteptam 7 valori
                    p1x, p1y = int(msg[0]), int(msg[1])
                    p2x, p2y = int(msg[2]), int(msg[3])
                    status = msg[4]
                    score1 = int(msg[5]) # Citim scorul
                    score2 = int(msg[6])

                    # Logic
                    if status == "RUNNING":
                        if game_over_state:
                            screen.fill(BLACK)
                            game_over_state = False

                        pygame.draw.rect(screen, NEON_CYAN, (p1x, p1y, PLAYER_SIZE, PLAYER_SIZE))
                        pygame.draw.rect(screen, NEON_PINK, (p2x, p2y, PLAYER_SIZE, PLAYER_SIZE))

                    elif "WIN" in status or "DRAW" in status:
                        game_over_state = True
                        if status == "P1_WINS": txt = "PLAYER 1 WINS!"
                        elif status == "P2_WINS": txt = "PLAYER 2 WINS!"
                        else: txt = "DRAW!"

                        draw_text_center(screen, txt, WHITE, 0)
                        draw_text_center(screen, "Press 'R' to Restart", WHITE, 50)

                    elif status == "WAITING":
                        draw_text_center(screen, "Waiting for Player 2...", WHITE)

        except BlockingIOError:
            pass
        except Exception as e:
            print(f"Network warning: {e}")

        # 3. DESENARE SCOR
        draw_score(screen, score1, score2)

        pygame.display.flip()
        time.sleep(0.01)

    sock.close()
    pygame.quit()

if __name__ == "__main__":
    start_game()