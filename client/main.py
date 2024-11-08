import random
import threading
import time

import eel
from client import Client  # Предполагается, что класс Client импортирован из client.py

# Инициализация Eel
eel.init('web')  # Замените 'web' на путь к папке с index.html

client = Client("127.0.0.1", 4242)  # Экземпляр класса Client


# Функция для динамической проверки состояния подключения
def check_connection_status():
    while True:
        time.sleep(1)  # Проверка статуса каждую секунду


@eel.expose
def register(nickname):
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
def disconnect():
    client.disconnect()


threading.Thread(target=check_connection_status, daemon=True).start()

client.start()
# Запуск Eel с указанием главной страницы
eel.start('index.html', size=(800, 800), port=random.randint(1024, 65535))
