import eel
from client import Client  # Предполагается, что класс Client импортирован из client.py

# Инициализация Eel
eel.init('web')  # Замените 'web' на путь к папке с index.html

client = Client("127.0.0.1", 4242)  # Экземпляр класса Client


@eel.expose
def register(nickname):
    client.register(nickname)
    eel.update_status("Connected")  # Обновление статуса


@eel.expose
def find_game():
    client.find_game()
    eel.log_message("Game search started")  # Логирование в интерфейсе


@eel.expose
def mark_number(number):
    client.mark_number(number)
    eel.log_message(f"Marked number: {number}")


@eel.expose
def bingo_check():
    client.bingo_check()
    eel.log_message("Bingo check sent")


@eel.expose
def ask_ticket():
    client.ask_ticket()
    eel.log_message("Requested ticket")


@eel.expose
def disconnect():
    client.disconnect()
    eel.update_status("Disconnected")
    eel.log_message("Disconnected from server")


client.start()
# Запуск Eel с указанием главной страницы
eel.start('index.html', size=(300, 300))
