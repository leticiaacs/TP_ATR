import json
import math
import sys
import pygame
import paho.mqtt.client as mqtt

BROKER_HOST = "localhost"
BROKER_PORT = 1884

# Tópicos MQTT
TOPIC_SUB_ESTADO = "atr/caminhao/+/estado"     
TOPIC_PUB_SETPOINT_BASE = "atr/caminhao/{}/setpoint" 

# Cores da Interface
COLOR_BG = (30, 30, 30)
COLOR_GRID = (60, 60, 60)
COLOR_TEXT = (200, 200, 200)
COLOR_CAM_OK = (50, 200, 50)      # Verde
COLOR_CAM_ERR = (220, 50, 50)     # Vermelho
COLOR_CAM_MAN = (240, 200, 50)    # Amarelo
COLOR_SELECAO = (255, 255, 255)   # Branco

# Dimensões da Tela e Mapa
SCREEN_W, SCREEN_H = 900, 650
MAP_MARGIN = 20
MAP_W = 600
MAP_H = SCREEN_H - 2 * MAP_MARGIN
SIDEBAR_X = MAP_MARGIN + MAP_W + 10

# Coordenadas Lógicas do Mundo 
WORLD_W, WORLD_H = 200.0, 200.0 

# Estado Global
caminhoes = {}       # Dicionário para guardar o estado recebido via MQTT
selecionado_id = 0   # ID do caminhão selecionado pelo operador
running = True       # Controle do loop principal

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Conectado com sucesso! (rc={rc})")
        # Assina o tópico de estado para ver todos os caminhões
        client.subscribe(TOPIC_SUB_ESTADO)
    else:
        print(f"[MQTT] Falha na conexão (rc={rc})")

def on_message(client, userdata, msg):
    """
    Chamado automaticamente quando chega mensagem do C++ ou Simulação.
    Atualiza o dicionário 'caminhoes' para a interface desenhar.
    """
    global caminhoes
    try:
        # Esperado: atr/caminhao/<ID>/estado
        parts = msg.topic.split('/')
        if len(parts) < 4: return
        
        cam_id = int(parts[2])
        payload_str = msg.payload.decode("utf-8")
        data = json.loads(payload_str)
        
        # Salva os dados recebidos
        caminhoes[cam_id] = data
        
    except Exception as e:
        print(f"[MQTT] Erro ao processar mensagem: {e}")

def world_to_screen(wx, wy):
    """ Converte metros (Mundo) para pixels (Tela) """
    sx = MAP_MARGIN + (wx / WORLD_W) * MAP_W
    # Inverte Y 
    sy = (MAP_MARGIN + MAP_H) - (wy / WORLD_H) * MAP_H
    return int(sx), int(sy)

def envia_setpoint(client, cam_id, wx, wy, vel=60.0):
    """ 
    Empacota o comando em JSON e envia via MQTT.
    """
    topic = TOPIC_PUB_SETPOINT_BASE.format(cam_id)
    
    # Payload padronizado para o C++ ler
    payload = {
        "x": round(wx, 2),
        "y": round(wy, 2),
        "vel": round(vel, 1),
        "cmd": "setpoint"
    }
    
    msg_json = json.dumps(payload)
    client.publish(topic, msg_json, qos=0)
    print(f"[CMD] Setpoint enviado p/ Caminhão {cam_id}: {msg_json}")

def verificar_clique_caminhao(mx, my):
    """ Verifica se clicou em cima de algum caminhão para selecioná-lo """
    global selecionado_id
    
    for cid, dados in caminhoes.items():
        x = dados.get('x', 0)
        y = dados.get('y', 0)
        sx, sy = world_to_screen(x, y)
        
        dist = math.hypot(mx - sx, my - sy)
        if dist < 15:
            selecionado_id = cid
            print(f"[UI] Selecionado Caminhão ID: {selecionado_id} (via clique)")
            return True # Clicou em um caminhão
            
    return False 

def main():
    global running, selecionado_id
    
    # Inicializa PyGame
    pygame.init()
    screen = pygame.display.set_mode((SCREEN_W, SCREEN_H))
    pygame.display.set_caption("Gestão da Mina - Operador")
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("Arial", 14)
    font_title = pygame.font.SysFont("Arial", 16, bold=True)
    
    # Configura MQTT
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    print("[Gestão] Tentando conectar ao Broker MQTT...")
    try:
        client.connect(BROKER_HOST, BROKER_PORT, 60)
        # Inicia thread de rede do MQTT
        client.loop_start() 
    except:
        print("[ERRO] Não foi possível conectar ao Broker.")
        print("Verifique se o 'mosquitto' está rodando.")
        sys.exit(1)

    # --- Loop de Eventos e Desenho ---
    while running:
        # 1. Processar Eventos (Teclado/Mouse)
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            
            elif event.type == pygame.KEYDOWN:
                # Teclas 0-9 selecionam o caminhão (atalho rápido)
                if event.key >= pygame.K_0 and event.key <= pygame.K_9:
                    selecionado_id = event.key - pygame.K_0
                    print(f"[UI] Selecionado Caminhão ID: {selecionado_id}")

            elif event.type == pygame.MOUSEBUTTONDOWN:
                # Clique esquerdo (botão 1)
                if event.button == 1:
                    mx, my = pygame.mouse.get_pos()
                    
                    # Primeiro verifica se clicou em um caminhão para selecionar
                    clicou_caminhao = verificar_clique_caminhao(mx, my)
                    
                    # Se NÃO clicou em caminhão, e clicou no mapa -> Envia Setpoint (Rota)
                    if not clicou_caminhao:
                        if (MAP_MARGIN <= mx <= MAP_MARGIN + MAP_W) and \
                           (MAP_MARGIN <= my <= MAP_MARGIN + MAP_H):
                            
                            rel_x = (mx - MAP_MARGIN)
                            rel_y = (my - (MAP_MARGIN + MAP_H)) * -1
                            
                            wx = (rel_x / MAP_W) * WORLD_W
                            wy = (rel_y / MAP_H) * WORLD_H
                            
                            wx = max(0.0, min(WORLD_W, wx))
                            wy = max(0.0, min(WORLD_H, wy))
                            
                            # Envia rota para o caminhão ATUALMENTE selecionado
                            envia_setpoint(client, selecionado_id, wx, wy)

        # 2. Desenhar Fundo e Mapa
        screen.fill(COLOR_BG)
        
        # Borda do Mapa
        pygame.draw.rect(screen, (0,0,0), (MAP_MARGIN, MAP_MARGIN, MAP_W, MAP_H))
        pygame.draw.rect(screen, COLOR_GRID, (MAP_MARGIN, MAP_MARGIN, MAP_W, MAP_H), 2)
        
        for i in range(1, 10):
            pos = MAP_MARGIN + (i * MAP_W / 10)
            pygame.draw.line(screen, (40,40,40), (pos, MAP_MARGIN), (pos, MAP_MARGIN+MAP_H))
            pos_y = MAP_MARGIN + (i * MAP_H / 10)
            pygame.draw.line(screen, (40,40,40), (MAP_MARGIN, pos_y), (MAP_MARGIN+MAP_W, pos_y))

        # 3. Desenhar Caminhões
        for cid, dados in caminhoes.items():
            # Pega dados com valores padrão caso falte algo no JSON
            x = dados.get('x', 0)
            y = dados.get('y', 0)
            auto = dados.get('automatico', False)
            defeito = dados.get('defeito', False)
            
            sx, sy = world_to_screen(x, y)
            
            # Define cor baseada no status
            cor = COLOR_CAM_MAN
            if auto: cor = COLOR_CAM_OK
            if defeito: cor = COLOR_CAM_ERR
            
            if cid == selecionado_id:
                pygame.draw.circle(screen, COLOR_SELECAO, (sx, sy), 12, 2)
            
            pygame.draw.circle(screen, cor, (sx, sy), 8)
            
            lbl = font.render(str(cid), True, COLOR_SELECAO)
            screen.blit(lbl, (sx+10, sy-10))

        # Desenhar Barra Lateral
        pygame.draw.line(screen, COLOR_GRID, (SIDEBAR_X - 5, 0), (SIDEBAR_X - 5, SCREEN_H), 2)
        
        y_off = 30
        title_surf = font_title.render(f"CAMINHÃO SELECIONADO: {selecionado_id}", True, (255, 255, 0))
        screen.blit(title_surf, (SIDEBAR_X, y_off))
        
        if selecionado_id in caminhoes:
            c = caminhoes[selecionado_id]
            # Lista de informações para exibir
            infos = [
                f"Pos X: {c.get('x',0):.2f}",
                f"Pos Y: {c.get('y',0):.2f}",
                f"Vel:   {c.get('vel',0):.1f} m/s",
                f"Ang:   {c.get('ang',0):.1f}°",
                f"Modo:  {'AUTO' if c.get('automatico') else 'MANUAL'}",
                f"Status:{'FALHA' if c.get('defeito') else 'OK'}",
                f"Temp:  {c.get('temp',0):.1f}°C"
            ]
            
            for line in infos:
                y_off += 25
                surf = font.render(line, True, COLOR_TEXT)
                screen.blit(surf, (SIDEBAR_X, y_off))
        else:
            y_off += 25
            screen.blit(font.render("Aguardando dados...", True, (100,100,100)), (SIDEBAR_X, y_off))
            
        # Instruções
        y_off = SCREEN_H - 100
        instr = [
            "COMO USAR:",
            "Clique no caminhao p/ selecionar",
            "Clique no Mapa p/ enviar Rota",
            "0-9: Atalho de seleção"
        ]
        for line in instr:
            surf = font.render(line, True, (150,150,150))
            screen.blit(surf, (SIDEBAR_X, y_off))
            y_off += 20

        pygame.display.flip()
        clock.tick(30) 

    print("[Gestão] Encerrando conexões...")
    client.loop_stop()  
    client.disconnect() 
    pygame.quit()       
    print("[Gestão] Encerrado com sucesso.")

if __name__ == "__main__":
    main()