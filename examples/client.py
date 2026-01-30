import zmq


def start_zmq_client():
    context = zmq.Context()

    socket = context.socket(zmq.REQ)

    host = '192.168.0.85'
    port = 5050
    socket.connect(f"tcp://{host}:{port}")

    print("Подключение к серверу...")

    message = "Hello from client"
    socket.send_string(message)
    print(f"Отправлено: {message}")

    response = socket.recv()
    print(f"Получен ответ: {response}")

    socket.close()
    context.term()


if __name__ == "__main__":
    start_zmq_client()