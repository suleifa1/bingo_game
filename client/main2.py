import argparse
import random
import threading
import time

import eel
from modules.client import Client


parser = argparse.ArgumentParser(description="Client Application")
parser.add_argument("--ip", type=str, required=True, help="IP address of the server")
parser.add_argument("--port", type=int, required=True, help="Port of the server")
args = parser.parse_args()

eel.init('web')

client = Client(args.ip, args.port)


def check_connection_status():
    while True:
        print("Time from last ping" + str(time.time() - client.last_pong_time))
        if (time.time() - client.last_pong_time) >= 10:
            client.connect_to_server()
        time.sleep(1)


@eel.expose
def register(nickname):
    client.nickname = nickname
    client.register(nickname)


@eel.expose
def find_game():
    client.find_game()


@eel.expose
def mark_number(number):
    client.mark_number(number)


@eel.expose
def bingo_check():
    client.bingo_check()


@eel.expose
def ask_ticket():
    client.ask_ticket()


@eel.expose
def leave_room():
    client.exit_room()
    eel.restoreWaitRoom()


@eel.expose
def disconnect():
    client.disconnect()
    eel.close_browser_window()
    exit()


threading.Thread(target=check_connection_status, daemon=True).start()

client.start()

eel.start('index.html', size=(600, 720), port=random.randint(1024, 65535))
