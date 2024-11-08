import eel
from client import Client

# Инициализация папки с веб-файлами
eel.init("web")

# Создаём экземпляр клиента
client = Client("127.0.0.1", 4242)

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

@eel.expose
def log_message(message):
    eel.log_message(message)

# Запуск eel
client.start()  # Запускаем клиент в отдельном потоке
eel.start("index.html", size=(800, 600))
