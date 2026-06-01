<h1 align="center">𝐻𝒾 𝓉𝒽𝑒𝓇𝑒, 𝐼'𝓂 <a href="https://t.me/Cocosik1558" target="_blank">Нгуен Зуй-Ань Куеевич</a> 
<img src="https://github.com/blackcater/blackcater/raw/main/images/Hi.gif" height="32"/></h1>
<h3 align="center">A second-year student of the Faculty of Computer Science at SibSUTIS.</h3>
<h3 align="center">My group IKS-433</h3>
<img src="https://readme-typing-svg.demolab.com?font=Fira+Code&pause=1000&width=435&lines=We+are+making+the+future+better." alt="Typing SVG" />

![GIF](https://github.com/Darkness1853/Pictures/blob/main/R%20(1).gif)

---

https://github.com/user-attachments/assets/bf4a3228-51e0-46c1-9552-383498603dbd

---

### Backend-сервер на C++

  ***Описание***

  - Сервер на С++ с двумя потоками: run_gui и run_server
  - Реализация gui: отображение информации и взаимодействие с пользователем.
  - Работа с ZMQ: приема данных от устройства 
  - Сохранение данных в файл .json
  - Связь между двумя потоками осуществялется при помощи одной общей структуры.
  - Работа с базой данных PostgreSQL
  - Вывод графиков (графики сигнала RSRP,RSSI,SINR)
--

# Структура

```bash
LocationServer/
├── CMakeLists.txt
├── DB/
│   ├──docker-compose.yml          # PostgreSQL контейнер
│   └──init.sql                    # Инициализация БД
├── include/
│   ├── location.hpp            # Структуры (LocationData, MobileNetworkData, PerPCIData, Location)
│   ├── server.hpp              # Объявление run_server
│   ├── gui.hpp                 # Объявление run_gui
│   └── db_client.hpp           # Класс DBClient (PostgreSQL)
├── src/
│   ├── main.cpp                # main, запуск потоков
│   ├── server.cpp              # ZeroMQ сервер, приём данных, фильтрация, JSON
│   ├── gui.cpp                 # ImGui интерфейс (Location, Networks, Graphs, Log, Stats)
│   └── db_client.cpp           # Подключение к БД, сохранение данных
│
└── third_party/
    ├── imgui/                  # ImGui (GUI)
    └── implot/                 # ImPlot (графики)
```

# Принцип работы (потоки)

ПОТОК GUI (run_gui)
1. Инициализация GLFW и создание окна
2. Инициализация GLEW, ImGui, ImPlot
3. Вход в главный цикл while(!glfwWindowShouldClose())
4. glfwPollEvents() - обработка событий окна
5. ImGui_ImplOpenGL3_NewFrame() - начало нового кадра
6. Отрисовка всех вкладок (Location, Networks, Graphs, Log, Stats)
7. ImGui::Render() - рендер GUI

ПОТОК СЕРВЕРА (run_server)
1. Создание DBClient и подключение к PostgreSQL
2. Создание ZeroMQ контекста и сокета
3. socket.bind("tcp://*:5050") - ожидание подключений
4. Вход в главный цикл while(loc->running)
5. zmq::poll() - ожидание входящих сообщений
6. ЕСЛИ есть входящее сообщение: 
7. json::parse(json_str) - парсинг JSON
8. Чтение send_id и location из JSON
9. Чтение mobile_networks из JSON
10. ЕСЛИ loc->recording == true:
11. ЕСЛИ БД подключена:
12. loc->message_count++
13. Формирование JSON ответа
14. socket.send() - отправка ответа клиенту

Мой главный проект в [Репозитории](https://github.com/Darkness1853/Android-Project)

Мой клиент в [Репозитории](https://github.com/Darkness1853/Android-Project/tree/client_C%2B%2B)
