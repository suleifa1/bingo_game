   let chatMessages = [];// Асинхронная функция для регистрации
   let previousCalledNumbers = []; // Предыдущий список вызванных номеров
   let fireworks = null;
   let attemptCount = 0;
   let clientMarkedNumbers = [];
   let numbers = [];
   eel.expose(markMissingNumbers);

        // Глобальные переменные

   // Экспорт функций для вызова из Python
    eel.expose(clearTicket);
    eel.expose(displayTicket);
    eel.expose(showPage);
    eel.expose(updateCurrentNumber);
    eel.expose(showSystemMessage);
    eel.expose(updateRoomInfo);
    eel.expose(restoreWaitRoom);
    eel.expose(close_browser_window);
    eel.expose(showConnectionStatus);
    eel.expose(hideConnectionStatus)

    async function register() {
        let nickname = document.getElementById("nickname").value;
        await eel.register(nickname)();
    }

    async function findGame() {
        await eel.find_game()();
        showPage("waiting-room");
    }

    async function markNumber() {
        let number = prompt("Введите номер для отметки:");
        if (number) {
            await eel.mark_number(parseInt(number))();
        }
    }

    async function bingoCheck() {
        await eel.bingo_check()();
    }

    async function askTicket() {
        await eel.ask_ticket()();
    }

    function displayTicket(numbers, isGame) {
        let ticketElement;
        if (isGame){
            ticketElement = document.getElementsByClassName("ticket")[1];
        }
        else {
            ticketElement = document.getElementsByClassName("ticket")[0];
        }

        const grid = ticketElement.querySelector(".ticket-grid");
        grid.innerHTML = '';

        numbers.forEach(number => {
            const cell = document.createElement("div");
            cell.classList.add("ticket-cell");
            cell.textContent = number;
            if (isGame) cell.addEventListener("click", () => markNumberOnTicket(parseInt(cell.textContent)));
            grid.appendChild(cell);
        });

        document.getElementById("get_button").classList.toggle("hidden");
        ticketElement.classList.remove("hidden");  // Показываем билет
    }
    
    function clearTicket(isGame){
        let ticketElement;
        if (isGame){
            ticketElement = document.getElementsByClassName("ticket")[1];
        }
        else {
            ticketElement = document.getElementsByClassName("ticket")[0];
        }

        const grid = ticketElement.querySelector(".ticket-grid");
        grid.innerHTML = ''; 
    }

    async function disconnect() {
        await eel.disconnect()();
        showPage("registration");
    }
                // Функция для выхода из комнаты ожидания и возвращения в лобби
    async function leaveRoom() {
        await eel.leave_room()();
    }

        // Обновление отображения текущего номера
    function updateCurrentNumber(number) {
        document.getElementById("number-display").textContent = number;
    }

    function markNumber(number) {
        console.log("Marked number:", number);
        // Можно добавить логику для визуального обозначения помеченного числа
    }

    function declareBingo() {
        console.log("Bingo!");
        eel.bingo_check();  // Отправляем проверку на бинго на сервер
    }


    // Привязка кнопок к функциям
    document.getElementById("registerButton").onclick = register;
    document.getElementById("findGameButton").onclick = findGame;
    document.getElementById("markNumberButton").onclick = markNumber;
    document.getElementById("bingoCheckButton").onclick = bingoCheck;
    document.getElementById("askTicketButton").onclick = askTicket;
    document.getElementById("disconnectButton").onclick = disconnect;

    function showPage(pageId) {
            document.querySelectorAll(".page").forEach(page => {
                page.classList.add("hidden");
            });

            document.getElementById(pageId).classList.remove("hidden");
            if (pageId === 'win-game') {
                startFireworks()
            } else {
                if (fireworks !== null){
                    fireworks.stop()
                }

            }
    }

    function showSystemMessage(message) {
        const container = document.getElementById("system-messages");

        // Создаем элемент сообщения
        const messageElement = document.createElement("div");
        messageElement.classList.add("system-message");
        messageElement.textContent = message;

        // Добавляем сообщение в контейнер
        container.appendChild(messageElement);

        // Удаляем сообщение через 5 секунд (время должно совпадать с длительностью анимации)
        setTimeout(() => {
            if (container.contains(messageElement)) {
                container.removeChild(messageElement);
            }
        }, 5000);
    }

    function updateCalledNumbers(calledNumbers) {
        const numbersContainer = document.getElementById("called-numbers-list");
        numbersContainer.innerHTML = ''; // Очищаем контейнер
        numbers = calledNumbers;
        // Определяем новые номера
        const newNumbers = calledNumbers.filter(num => !previousCalledNumbers.includes(num));

        calledNumbers.forEach((number, index) => {
            const numberElement = document.createElement("div");
            numberElement.classList.add("number-item");
            numberElement.textContent = number;

            // Если это последний номер и он новый, добавляем класс для анимации
            if (index === calledNumbers.length - 1 && newNumbers.includes(number)) {
                numberElement.classList.add("new-number");
            }

            numbersContainer.appendChild(numberElement);
        });

        // Обновляем предыдущий список вызванных номеров
        previousCalledNumbers = calledNumbers.slice();
    }

    function markNumberOnTicket(number) {
        if (numbers.includes(number) && !clientMarkedNumbers.includes(number)) {
            clientMarkedNumbers.push(number);

            // Находим элемент на билете и добавляем класс "marked"
            const ticketCells = document.querySelectorAll(".ticket-cell");
            ticketCells.forEach(cell => {
                if (parseInt(cell.textContent) === number) {
                    cell.classList.add("marked");
                }
            });
        } else {
            console.log("Номер не может быть отмечен: либо он не вызван, либо уже отмечен.");
        }
    }

    function updateRoomInfo(roomInfo) {
        // Обновление вызванных номеров
        updateCalledNumbers(roomInfo.calledNumbers);

        // Обновление текущего номера
        const currentNumberElement = document.getElementById("number-display");
        const lastNumber = roomInfo.calledNumbers[roomInfo.calledNumbers.length - 1] || '--';
        currentNumberElement.textContent = lastNumber;

        // Вы можете добавить другие обновления интерфейса на основе roomInfo
    }

    function restoreWaitRoom(){
        let getTicketButton = document.getElementById('get_button');
        let ticket = document.getElementById('ticket');

        getTicketButton.classList.remove('hidden');
        ticket.classList.add('hidden');
        clearTicket(0);

    }



    function close_browser_window() {
        window.close();  // Закрываем окно браузера
    }

        // Функция для запуска фейерверка
   function startFireworks() {
         // Отображаем секцию выигрыша

        const container = document.getElementById('fireworks-container');
        container.innerHTML = '';
        fireworks = new Fireworks.default(container, {

            speed: 2,
            gravity: 1,
            particles: 50,
            explosion: 1,
            traceSpeed: 5,
            delay: { min: 30, max: 60 },
            lineWidth: {
                explosion: {
                      min: 4,
                      max: 8
                }

            }
        });

        fireworks.start();
    }



    // Функция для отображения статуса подключения
    function showConnectionStatus(message, attempts) {
        const statusElement = document.getElementById("connection-status");
        const messageElemenet = document.getElementById("connection-message");

        messageElemenet.textContent = `${message}`;
        statusElement.classList.remove("hidden");

        // Если попыток больше 5, показываем кнопку выхода
        if (attempts > 5) {

            document.getElementById("exit-button").classList.remove("hidden");
        } else {
            document.getElementById("exit-button").classList.add("hidden");
        }
    }

    function hideConnectionStatus(){
        document.getElementById("connection-status").classList.add("hidden");

    }

    function markMissingNumbers(callNumbers) {
        callNumbers.forEach(number => {
            if (!clientMarkedNumbers.includes(number)) {
                clientMarkedNumbers.push(number);
                const cells = document.querySelectorAll(".ticket-cell");
                cells.forEach(cell => {
                    if (parseInt(cell.textContent) === number) {
                        cell.classList.add("marked"); // Добавляем класс marked
                    }
                });
            }
        });
    }






