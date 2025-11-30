import json
import math
import time
from dataclasses import dataclass, field

import pygame
import paho.mqtt.client as mqtt

# ---------------------- Configurações gerais ----------------------

BROKER_HOST = "localhost"
BROKER_PORT = 1883

WORLD_W, WORLD_H = 200.0, 200.0  # "tamanho" lógico da mina
SCREEN_W, SCREEN_H = 900, 600
MAP_MARGIN = 20
MAP_W = SCREEN_W - 2 * MAP_MARGIN
MAP_H = SCREEN_H - 2 * MAP_MARGIN

FPS = 30
DT = 1.0 / FPS   # passo de simulação (seg)

# ---------------------- Estruturas de dados ----------------------


@dataclass
class CaminhaoState:
    cam_id: int
    x: float = 0.0
    y: float = 0.0
    vel: float = 0.0            # 0..100 (%)
    ang: float = 0.0            # graus, 0 = leste, CCW

    accel_cmd: float = 0.0      # comando de aceleração (vem da lógica)
    dir_cmd: float = 0.0        # comando de direção (vem da lógica)

    automatico: bool = False
    defeito: bool = False       # recebido da lógica de comando

    temp: float = 40.0          # temperatura "do motor"
    falha_eletrica: bool = False
    falha_hidraulica: bool = False

    _noise_phase: float = field(default=0.0, repr=False)


# dicionário global de caminhões
caminhoes: dict[int, CaminhaoState] = {}


def get_or_create_truck(cam_id: int) -> CaminhaoState:
    if cam_id not in caminhoes:
        print(f"[sim_mina] Criando caminhão {cam_id} ao receber primeira mensagem MQTT.")
        caminhoes[cam_id] = CaminhaoState(cam_id=cam_id)
    return caminhoes[cam_id]


# ---------------------- Conversões de coordenadas ----------------------


def world_to_screen(x: float, y: float) -> tuple[int, int]:
    sx = int(MAP_MARGIN + (x / WORLD_W) * MAP_W)
    sy = int(MAP_MARGIN + MAP_H - (y / WORLD_H) * MAP_H)
    return sx, sy


def publica_sensores(client: mqtt.Client, st: CaminhaoState) -> None:
    """Publica posição e ângulo atuais (sensores)."""
    payload = {
        "id": st.cam_id,
        "pos_x": st.x,
        "pos_y": st.y,
        "ang_x": st.ang,
    }
    topic = f"atr/caminhao/{st.cam_id}/sensores"
    client.publish(topic, json.dumps(payload), qos=0, retain=False)


def publica_falhas(client: mqtt.Client, st: CaminhaoState) -> None:
    """Publica temperatura atual e falhas elétrica/hidráulica."""
    payload = {
        "id": st.cam_id,
        "temp": st.temp,
        "falha_eletrica": st.falha_eletrica,
        "falha_hidraulica": st.falha_hidraulica,
    }
    topic = f"atr/caminhao/{st.cam_id}/falhas"
    client.publish(topic, json.dumps(payload), qos=0, retain=False)


# ---------------------- Dinâmica da simulação ----------------------


def atualiza_dinamica(st: CaminhaoState, dt: float) -> None:
    # dinâmica baseada em comandos vindos da lógica de comando
    st.vel += st.accel_cmd * dt
    st.ang += st.dir_cmd * dt

    # limita velocidade e normaliza ângulo
    if st.vel < 0.0:
        st.vel = 0.0
    if st.vel > 100.0:
        st.vel = 100.0
    st.ang = (st.ang + 180.0) % 360.0 - 180.0

    # atualiza posição
    rad = math.radians(st.ang)
    st.x += st.vel * math.cos(rad) * dt
    st.y += st.vel * math.sin(rad) * dt

    st.x = max(0.0, min(WORLD_W, st.x))
    st.y = max(0.0, min(WORLD_H, st.y))

    # temperatura simples: sobe com a velocidade + ruído
    base = 40.0 + 0.4 * st.vel
    st._noise_phase += dt
    ruido = 2.0 * math.sin(st._noise_phase * 1.3 + st.cam_id)
    st.temp = base + ruido


# ---------------------- MQTT callbacks ----------------------


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[sim_mina] Conectado ao broker MQTT (rc=0)")
        # recebe comandos de atuadores vindos da logica_comando
        client.subscribe("atr/caminhao/+/atuadores")
        print("[sim_mina] Subscrito em atr/caminhao/+/atuadores")
    else:
        print(f"[sim_mina] Falha ao conectar MQTT, rc={rc}")


def on_message(client, userdata, msg):
    try:
        topic = msg.topic
        payload = msg.payload.decode("utf-8")
        data = json.loads(payload)
    except Exception as e:
        print("[sim_mina] Erro ao decodificar mensagem MQTT:", e)
        return

    parts = topic.split("/")
    if len(parts) < 4:
        return

    # ID vem do tópico: atr/caminhao/<id>/atuadores
    try:
        cam_id_topic = int(parts[2])
    except ValueError:
        print("[sim_mina] ID inválido no tópico:", topic)
        return

    # se também vier "id" no payload, podemos conferir (opcional)
    cam_id_payload = data.get("id")
    if cam_id_payload is not None and cam_id_payload != cam_id_topic:
        print(f"[sim_mina] Aviso: id do payload ({cam_id_payload}) "
              f"difere do id do tópico ({cam_id_topic}). Usando o do tópico.")

    st = get_or_create_truck(cam_id_topic)

    if parts[3] != "atuadores":
        return

    try:
        st.accel_cmd = float(data.get("acel", 0.0))
        st.dir_cmd = float(data.get("dir", 0.0))
        st.automatico = bool(data.get("automatico", st.automatico))
        st.defeito = bool(data.get("defeito", st.defeito))
    except Exception as e:
        print("[sim_mina] Erro ao aplicar comandos de atuadores:", e)


# ---------------------- Desenho da interface ----------------------


def desenha_grid(screen):
    screen.fill((30, 30, 30))
    pygame.draw.rect(screen, (50, 50, 50),
                     (MAP_MARGIN, MAP_MARGIN, MAP_W, MAP_H), 0)

    for i in range(0, 11):
        x = MAP_MARGIN + int(i * MAP_W / 10)
        y = MAP_MARGIN + int(i * MAP_H / 10)
        pygame.draw.line(screen, (60, 60, 60), (x, MAP_MARGIN), (x, MAP_MARGIN + MAP_H), 1)
        pygame.draw.line(screen, (60, 60, 60), (MAP_MARGIN, y), (MAP_MARGIN + MAP_W, y), 1)

    pygame.draw.line(screen, (180, 180, 180),
                     (MAP_MARGIN, MAP_MARGIN + MAP_H),
                     (MAP_MARGIN + MAP_W, MAP_MARGIN + MAP_H), 2)
    pygame.draw.line(screen, (180, 180, 180),
                     (MAP_MARGIN, MAP_MARGIN),
                     (MAP_MARGIN, MAP_MARGIN + MAP_H), 2)


def desenha_caminhao(screen, font, st: CaminhaoState, selecionado_id: int | None):
    sx, sy = world_to_screen(st.x, st.y)

    # cor: verde auto, amarelo manual, vermelho se defeito (só visual)
    if st.defeito:
        color = (220, 60, 60)
    elif st.automatico:
        color = (50, 200, 50)
    else:
        color = (240, 200, 50)

    radius = 10 if st.cam_id == selecionado_id else 8
    pygame.draw.circle(screen, color, (sx, sy), radius)

    ang_rad = math.radians(st.ang)
    tip_x = sx + int(14 * math.cos(ang_rad))
    tip_y = sy - int(14 * math.sin(ang_rad))
    pygame.draw.line(screen, (230, 230, 230), (sx, sy), (tip_x, tip_y), 2)

    label = f"#{st.cam_id} {'AUTO' if st.automatico else 'MAN'} v={st.vel:.0f} T={st.temp:.0f}"
    text_surf = font.render(label, True, (220, 220, 220))
    screen.blit(text_surf, (sx + 12, sy - 10))


def desenha_painel(screen, font_small, selecionado_id):
    pygame.draw.rect(screen, (20, 20, 20),
                     (0, 0, SCREEN_W, MAP_MARGIN - 2))

    linha1 = "Estados: VERDE=Auto, AMARELO=Manual, VERMELHO=Defeito (flag recebida) | Mundo: 0..200 x 0..200"
    linha2 = "Teclas: 1..9 seleciona caminhão | F: toggla falha elétrica | H: toggla falha hidráulica"

    txt1 = font_small.render(linha1, True, (220, 220, 220))
    txt2 = font_small.render(linha2, True, (220, 220, 220))
    screen.blit(txt1, (MAP_MARGIN, SCREEN_H - MAP_MARGIN + 4))
    screen.blit(txt2, (MAP_MARGIN, SCREEN_H - MAP_MARGIN + 20))


# ---------------------- Loop principal ----------------------


def main():
    pygame.init()
    screen = pygame.display.set_mode((SCREEN_W, SCREEN_H))
    pygame.display.set_caption("Simulação da Mina - ATR / Caminhão")
    font = pygame.font.SysFont("consolas", 14)
    font_small = pygame.font.SysFont("consolas", 12)

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER_HOST, BROKER_PORT, 60)
    client.loop_start()

    clock = pygame.time.Clock()
    running = True
    selecionado_id: int | None = None

    last_publish = time.time()

    print("[sim_mina] Simulação iniciada. Caminhões serão criados somente ao receber mensagens MQTT.")

    while running:
        dt = clock.tick(FPS) / 1000.0

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.KEYDOWN:
                # números 1..9 selecionam caminhão (se já existirem)
                if pygame.K_1 <= event.key <= pygame.K_9:
                    idx = event.key - pygame.K_1 + 1
                    if idx in caminhoes:
                        selecionado_id = idx
                        print(f"[sim_mina] Caminhão selecionado: {selecionado_id}")
                    else:
                        print(f"[sim_mina] Caminhão {idx} ainda não existe (nenhuma msg MQTT recebida).")

                elif event.key == pygame.K_f:
                    # toggla falha elétrica no caminhão selecionado
                    if selecionado_id is not None and selecionado_id in caminhoes:
                        st = caminhoes[selecionado_id]
                        st.falha_eletrica = not st.falha_eletrica
                        print(f"[sim_mina] Caminhão {selecionado_id}: falha elétrica = {st.falha_eletrica}")
                    else:
                        print("[sim_mina] Nenhum caminhão selecionado ou ainda criado para falha elétrica.")

                elif event.key == pygame.K_h:
                    # toggla falha hidráulica
                    if selecionado_id is not None and selecionado_id in caminhoes:
                        st = caminhoes[selecionado_id]
                        st.falha_hidraulica = not st.falha_hidraulica
                        print(f"[sim_mina] Caminhão {selecionado_id}: falha hidráulica = {st.falha_hidraulica}")
                    else:
                        print("[sim_mina] Nenhum caminhão selecionado ou ainda criado para falha hidráulica.")

        # Atualiza dinâmica de todos os caminhões existentes
        for st in caminhoes.values():
            atualiza_dinamica(st, dt)

        # desenha
        desenha_grid(screen)
        for st in caminhoes.values():
            desenha_caminhao(screen, font, st, selecionado_id)
        desenha_painel(screen, font_small, selecionado_id)

        pygame.display.flip()

        # publica sensores e falhas a cada ~100 ms
        now = time.time()
        if now - last_publish > 0.1:
            for st in caminhoes.values():
                publica_sensores(client, st)
                publica_falhas(client, st)
            last_publish = now

    print("[sim_mina] Encerrando simulação...")
    client.loop_stop()
    client.disconnect()
    pygame.quit()


if __name__ == "__main__":
    main()
