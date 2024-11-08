import struct
from enum import Enum
import socket
import threading
import tkinter as tk
import time

SERVER_IP = "127.0.0.1"
SERVER_PORT = 4242
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
    def __init__(self, server_ip, server_port, ui):
        self.server_ip = server_ip
        self.server_port = server_port
        self.ui = ui
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
                self.ui.update_status("Connected")
                self.ui.log_message("Successfully connected to the server.")
            except socket.error:
                time.sleep(RECONNECT_DELAY)

    def start(self):
        self.connect_to_server()
        threading.Thread(target=self.receive_messages, daemon=True).start()

    def send_command(self, command, data=b''):
        header = b'SP' + command.value.to_bytes(4, 'little') + len(data).to_bytes(4, 'little')
        with self.lock:
            try:
                self.socket.sendall(header + data)
                print(f"Sent command {command.name} with data: {data}")
                self.ui.log_message(f"Sent {command.name} command")
            except socket.error as e:
                self.ui.log_message(f"Failed to send command {command.name}: {e}")
                self.connected = False

    def register(self, nickname):
        self.send_command(Command.REGISTER, nickname.encode())

    def find_game(self):
        self.send_command(Command.CMD_FINDGAME)

    def mark_number(self, number):
        number_data = struct.pack("I", number)
        self.send_command(Command.MARK_NUMBER, number_data)
        self.ui.log_message(f"Attempted to mark number: {number}")

    def bingo_check(self):
        self.send_command(Command.BINGO)
        self.ui.log_message("Attempted BINGO check")

    def send_pong(self):
        self.send_command(Command.PONG)

    def ask_ticket(self):
        self.send_command(Command.ASK_TICKET)

    def disconnect(self):
        self.connected = False
        if self.socket:
            self.socket.close()
            self.ui.update_status("Disconnected")

    def process_payload(self, command, payload):
        if command == Command.PING:
            self.send_pong()
            self.last_pong_time = time.time()
            print(f"Processed command: {command.name}")
        elif command == Command.SEND_TICKET:
            ticket_numbers = struct.unpack(f"{len(payload) // 4}I", payload)
            self.ui.log_message(f"Ticket received: {ticket_numbers}")
        elif command == Command.START_GAME:
            self.ui.log_message("Game started.")
        elif command == Command.SEND_NUMBER:
            number = int.from_bytes(payload, 'little')
            self.ui.log_message(f"Number received: {number}")
        elif command == Command.OK:
            self.ui.log_message("Received OK from server.")
        elif command == Command.END_GAME:
            self.ui.log_message("Game ended.")
        elif command == Command.INFO:
            info_text = payload.decode()
            self.ui.log_message(f"Info: {info_text}")
        else:
            self.ui.log_message("Unknown command received.")
        print(f"Processed command: {command.name} with payload: {payload}")

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
                    print(f"Received command {Command(command).name} with raw payload: {payload}")
                    self.process_payload(Command(command), payload)
                else:
                    self.ui.log_message("Unknown prefix")
            except socket.error:
                self.connected = False
                self.ui.update_status("Disconnected")
                time.sleep(RECONNECT_DELAY)


class ClientApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Client Application")
        self.client = Client(SERVER_IP, SERVER_PORT, self)

        self.status_label = tk.Label(root, text="Status: Disconnected")
        self.status_label.pack()

        self.nickname_label = tk.Label(root, text="Enter Nickname:")
        self.nickname_label.pack()

        self.nickname_entry = tk.Entry(root)
        self.nickname_entry.pack()

        self.message_box = tk.Text(root, width=50, height=15, state='disabled')
        self.message_box.pack()

        self.register_button = tk.Button(root, text="Register", command=self.register)
        self.register_button.pack()

        self.find_game_button = tk.Button(root, text="Find Game", command=self.find_game)
        self.find_game_button.pack()

        self.mark_number_button = tk.Button(root, text="Mark Number", command=self.mark_number_prompt)
        self.mark_number_button.pack()

        self.bingo_check_button = tk.Button(root, text="Bingo Check", command=self.bingo_check)
        self.bingo_check_button.pack()

        self.ticket_button = tk.Button(root, text="Ask for Ticket", command=self.ask_ticket)
        self.ticket_button.pack()

        self.disconnect_button = tk.Button(root, text="Disconnect", command=self.disconnect)
        self.disconnect_button.pack()

        threading.Thread(target=self.client.start, daemon=True).start()

    def register(self):
        nickname = self.nickname_entry.get()
        if nickname:
            self.client.register(nickname)
            self.log_message(f"Sent: Register as {nickname}")
        else:
            self.log_message("Please enter a nickname to register.")

    def find_game(self):
        self.client.find_game()
        self.log_message("Sent: Find Game")

    def mark_number_prompt(self):
        # Открываем небольшое окно для ввода числа
        def submit_number():
            try:
                number = int(number_entry.get())
                self.client.mark_number(number)
                self.log_message(f"Sent: Mark number {number}")
                prompt.destroy()
            except ValueError:
                self.log_message("Invalid number entered")

        prompt = tk.Toplevel(self.root)
        prompt.title("Enter Number to Mark")
        tk.Label(prompt, text="Enter Number:").pack()
        number_entry = tk.Entry(prompt)
        number_entry.pack()
        tk.Button(prompt, text="Submit", command=submit_number).pack()

    def bingo_check(self):
        self.client.bingo_check()
        self.log_message("Sent: Bingo Check")

    def ask_ticket(self):
        self.client.ask_ticket()
        self.log_message("Sent: Ask Ticket")

    def disconnect(self):
        self.client.disconnect()
        self.update_status("Disconnected")

    def update_status(self, status):
        self.status_label.config(text=f"Status: {status}")

    def log_message(self, message):
        self.message_box.config(state='normal')
        self.message_box.insert(tk.END, message + "\n")
        self.message_box.config(state='disabled')


if __name__ == "__main__":
    root = tk.Tk()
    app = ClientApp(root)
    root.mainloop()
