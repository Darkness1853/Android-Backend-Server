import zmq
import time

def start_zmq_server():

    context = zmq.Context()
    socket = context.socket(zmq.REP)
    socket.bind("tcp://0.0.0.0:5050")
    print("Сервер слушает порт: 5050 ")
    data_filename = f"сommunication.txt"
    print(f"Данные записываются в файл: {data_filename}")

    packet_count = 0

    try:
        while True:
            message = socket.recv_string()
            print(f"Получено: {message}")

            packet_count += 1

            timestamp = time.strftime('%H:%M:%S')
            with open(data_filename, 'w', encoding='utf-8') as file:
                file.write("Диалог между Сервером и Клиентом\n")
                file.write(f"{timestamp}  #{packet_count}: {message}\n")

            response = f"Packet #{packet_count} Server receive: {message}"
            socket.send_string(response)
            print(f"Отправлено: {response}")

    except KeyboardInterrupt:
        print("\nСервер остановлен")
    finally:
        socket.close()
        context.term()
        print(f"Всего пакетов: {packet_count}")
        print(f"Файл: {data_filename}")


if __name__ == "__main__":
    start_zmq_server()


