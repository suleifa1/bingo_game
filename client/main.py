import argparse
from modules.UIController import start_application


def main():
    # Парсинг аргументов командной строки
    parser = argparse.ArgumentParser(description="Client Application")
    parser.add_argument("--ip", type=str, required=True, help="IP address of the server")
    parser.add_argument("--port", type=int, required=True, help="Port of the server")
    args = parser.parse_args()

    # Запуск приложения с переданными параметрами
    start_application(args.ip, args.port)


if __name__ == "__main__":
    main()
