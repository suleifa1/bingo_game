import socket
import threading
import struct
import time
from enum import Enum
import eel

RECONNECT_DELAY = 5

MAX_NICKNAME_LEN = 20   # Максимальная длина ника (определите точное значение)
TICKET_SIZE = 5        # Размер билета (определите точное значение)

previous_player_nicknames = []

def handle_room_update(room_info):
    global previous_player_nicknames

    current_player_nicknames = room_info['playerNicknames']

    # Если это первый вызов, инициализируем список и выходим
    if not previous_player_nicknames:
        previous_player_nicknames = current_player_nicknames.copy()
        return

    # Преобразуем списки никнеймов в множества для удобства сравнения
    previous_players = set(previous_player_nicknames)
    current_players = set(current_player_nicknames)

    # Определяем новых подключившихся игроков
    connected_players = current_players - previous_players
    for nickname in connected_players:
        eel.showSystemMessage(f"Игрок {nickname} подключился.")

    # Определяем игроков, которые отключились
    disconnected_players = previous_players - current_players
    for nickname in disconnected_players:
        eel.showSystemMessage(f"Игрок {nickname} отключился.")

    # Обновляем предыдущий список игроков
    previous_player_nicknames = current_player_nicknames.copy()


def unpack_client(data):
    # Формат строки для struct.unpack
    # s - строка байтов фиксированной длины, 32s - строка длиной 32 байта (например, для никнейма)
    # i - целое число 4 байта
    # I - беззнаковое целое число 4 байта
    format_str = f'{MAX_NICKNAME_LEN}s i i i {TICKET_SIZE}i {TICKET_SIZE}i i i I I I'

    # Распаковка данных согласно формату
    unpacked_data = struct.unpack(format_str, data)

    # Создаем словарь для удобного доступа к полям клиента
    client_info = {
        'nickname': unpacked_data[0].decode('utf-8', errors='ignore').split('\x00', 1)[0],
        'socket': unpacked_data[1],
        'state': unpacked_data[2],
        'prev_state': unpacked_data[3],
        'ticket': unpacked_data[4:4 + TICKET_SIZE],
        'marked': unpacked_data[4 + TICKET_SIZE:4 + 2 * TICKET_SIZE],
        'ticket_count': unpacked_data[4 + 2 * TICKET_SIZE],
        'room_id': unpacked_data[4 + 2 * TICKET_SIZE + 1],
        'last_ping': unpacked_data[4 + 2 * TICKET_SIZE + 2],
        'last_pong': unpacked_data[4 + 2 * TICKET_SIZE + 3],
        'disconnect_time': unpacked_data[4 + 2 * TICKET_SIZE + 4]
    }

    return client_info

def unpack_room_info(data):
    MAX_NICKNAME_LEN = 20
    MAX_PLAYERS_IN_ROOM = 3
    MAX_NUMBERS = 40

    format_str = f'<iii{MAX_PLAYERS_IN_ROOM * MAX_NICKNAME_LEN}s{MAX_NUMBERS}iii'

    expected_size = struct.calcsize(format_str)
    if len(data) != expected_size:
        raise ValueError(f"Data size mismatch. Expected {expected_size} bytes, got {len(data)} bytes.")

    unpacked_data = struct.unpack(format_str, data)

    room_id = unpacked_data[0]
    game_state = unpacked_data[1]
    player_count = unpacked_data[2]

    nicknames_data = unpacked_data[3]
    player_nicknames = []
    for i in range(MAX_PLAYERS_IN_ROOM):
        start = i * MAX_NICKNAME_LEN
        end = start + MAX_NICKNAME_LEN
        nickname_bytes = nicknames_data[start:end]
        nickname = nickname_bytes.decode('utf-8', errors='ignore').split('\x00', 1)[0]
        if nickname:
            player_nicknames.append(nickname)

    called_numbers = list(unpacked_data[4 : 4 + MAX_NUMBERS])

    total_numbers_called = unpacked_data[4 + MAX_NUMBERS]
    unpause = unpacked_data[5 + MAX_NUMBERS]

    room_info = {
        'roomId': room_id,
        'gameState': game_state,
        'playerCount': player_count,
        'playerNicknames': player_nicknames[:player_count],
        'calledNumbers': called_numbers[:total_numbers_called],
        'totalNumbersCalled': total_numbers_called,
        'unpause': unpause
    }

    return room_info

class Command(Enum):
    REGISTER = 0
    READY = 1
    MARK_NUMBER = 2
    BINGO = 3
    RECONNECT = 4
    PONG = 5
    ASK_TICKET = 6
    SEND_TICKET = 8
    START_GAME = 9
    SEND_NUMBER = 10
    BINGO_CHECK = 11
    PING = 12
    OK = 13
    END_GAME = 14
    USER_INFO = 15
    CMD_FINDGAME = 16
    ROOM_INFO = 17

class State(Enum):
    STATE_REGISTER = 0
    STATE_LOBBY = 1
    STATE_RECONNECTING = 2
    STATE_DISCONNECTED = 3

    STATE_WAITING_FOR_GAME = 4

    STATE_GOT_TICKET = 5
    STATE_GET_NUMBER = 6
    STATE_MARK_NUMBER = 7

    STATE_BINGO_CHECK = 8


class Client:
    def __init__(self, server_ip, server_port):
        self.server_ip = server_ip
        self.server_port = server_port
        self.socket = None
        self.connected = False
        self.lock = threading.Lock()
        self.last_pong_time = time.time()
        self.ticket = None
        self.data = None

    def connect_to_server(self):
        while not self.connected:
            try:
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.connect((self.server_ip, self.server_port))
                self.connected = True
            except socket.error:
                time.sleep(RECONNECT_DELAY)

    def start(self):
        threading.Thread(target=self.receive_messages, daemon=True).start()

    def send_command(self, command, data=b''):
        header = b'SP' + command.value.to_bytes(4, 'little') + len(data).to_bytes(4, 'little')
        with self.lock:
            try:
                self.socket.sendall(header + data)
            except socket.error:
                self.connected = False

    def register(self, nickname):
        self.send_command(Command.REGISTER, nickname.encode())

    def find_game(self):
        self.send_command(Command.CMD_FINDGAME)

    def mark_number(self, number):
        number_data = struct.pack("I", number)
        self.send_command(Command.MARK_NUMBER, number_data)

    def bingo_check(self):
        self.send_command(Command.BINGO)

    def ask_ticket(self):
        print("ask tikect")
        self.send_command(Command.ASK_TICKET)

    def send_pong(self):
        """Отправляет PONG сообщение серверу для ответа на PING."""
        self.send_command(Command.PONG)
        print("Sent PONG in response to PING")

    def disconnect(self):
        self.connected = False
        if self.socket:
            self.socket.close()
            print("Disconnected")

    def process_payload(self, command, payload):
        if command == Command.PING:
            self.send_pong()  # Отправка PONG в ответ на PING
            self.last_pong_time = time.time()
        elif command == Command.SEND_TICKET:
            self.ticket = struct.unpack(f"{len(payload) // 4}I", payload)
            print(self.ticket)
            eel.displayTicket(self.ticket, 0)

        elif command == Command.START_GAME:
            eel.showPage('game-page')
            print("denis " + str(self.ticket))
            eel.displayTicket(self.ticket, 1)
            print("Game started.")
        elif command == Command.SEND_NUMBER:
            number = int.from_bytes(payload, 'little')
            eel.updateCurrentNumber(number)
            print(f"Number received: {number}")
        elif command == Command.OK:
            print("Received OK from server.")
        elif command == Command.END_GAME:
            eel.clearTicket()
            eel.showPage('lobby')
            self.ticket = None
            self.data = None
            print("Game ended.")
        elif command == Command.USER_INFO:
            self.data = unpack_client(payload)
            print(self.data.get('state'))
            if State(self.data.get('state')) == State.STATE_GOT_TICKET:
                eel.showPage('waiting-room')
                eel.displayTicket(self.data.get('ticket'))

            elif (State(self.data.get('state')) == State.STATE_GET_NUMBER or
                  State(self.data.get('state')) == State.STATE_MARK_NUMBER or
                  State(self.data.get('state')) == State.STATE_BINGO_CHECK):
                eel.showPage('game-page')
                eel.displayTicket(self.data.get('ticket'), 1)

            elif State(self.data.get('state')) == State.STATE_REGISTER:
                eel.showPage('registration')

            elif State(self.data.get('state')) == State.STATE_LOBBY:
                eel.showPage("lobby")

            print(f"Info: {self.data}")
        elif command == Command.ROOM_INFO:
            room_info = unpack_room_info(payload)
            handle_room_update(room_info)
            if room_info.get('gameState') != 0 or room_info.get('gameState') != 2:
                eel.updateRoomInfo(room_info)

        else:
            print("Unknown command received.")

    def receive_messages(self):
        while True:
            if not self.connected:
                self.connect_to_server()
                continue

            try:
                header = self.socket.recv(10)
                if not header:
                    raise socket.error("Server disconnected.")

                prefix = header[0:2].decode()
                command = int.from_bytes(header[2:6], 'little')
                length = int.from_bytes(header[6:10], 'little')

                if prefix == 'SP':
                    payload = self.socket.recv(length) if length > 0 else b''
                    self.process_payload(Command(command), payload)
                else:
                    print("Unknown prefix")
            except socket.error:
                self.connected = False
                time.sleep(RECONNECT_DELAY)
