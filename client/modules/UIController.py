import random
import threading
import time
import eel
from modules.client import Client

client = None


def init_eel(ip, port):
    """
    Инициализация клиента и eel.
    """
    global client
    client = Client(ip, port)
    eel.init('web')


def check_connection_status():
    """
    Проверка состояния соединения.
    """
    while True:
        print("Time from last ping: " + str(time.time() - client.last_pong_time))
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


def start_application(ip, port):
    """
    Запуск eel и клиента.
    """
    # Инициализация eel и клиента
    init_eel(ip, port)

    # Запуск потока проверки соединения
    threading.Thread(target=check_connection_status, daemon=True).start()

    eel.start('index.html', size=(600, 720), port=random.randint(1024, 65535))

    # Запуск клиента
    client.start()

    # Запуск веб-интерфейса
