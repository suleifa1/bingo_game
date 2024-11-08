import socket
import threading
import struct
import time
from enum import Enum


RECONNECT_DELAY = 5

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
    INFO = 15
    CMD_FINDGAME = 16

class Client:
    def __init__(self, server_ip, server_port):
        self.server_ip = server_ip
        self.server_port = server_port
        self.socket = None
        self.connected = False
        self.lock = threading.Lock()
        self.last_pong_time = time.time()

    def connect_to_server(self):
        while not self.connected:
            try:
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.connect((self.server_ip, self.server_port))
                self.connected = True
            except socket.error:
                time.sleep(RECONNECT_DELAY)

    def start(self):
        threading.Thread(target=self.connect_to_server, daemon=True).start()
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
            ticket_numbers = struct.unpack(f"{len(payload) // 4}I", payload)
            print(f"Ticket received: {ticket_numbers}")
        elif command == Command.START_GAME:
            print("Game started.")
        elif command == Command.SEND_NUMBER:
            number = int.from_bytes(payload, 'little')
            print(f"Number received: {number}")
        elif command == Command.OK:
            print("Received OK from server.")
        elif command == Command.END_GAME:
            print("Game ended.")
        elif command == Command.INFO:
            info_text = payload.decode()
            print(f"Info: {info_text}")
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
                print("Disconnected")
                time.sleep(RECONNECT_DELAY)
