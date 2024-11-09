   // Асинхронная функция для регистрации

   // Экспорт функций для вызова из Python
    eel.expose(clearTicket);
    eel.expose(displayTicket);
    eel.expose(showPage);
    eel.expose(updateCurrentNumber);

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
        showPage("lobby");  // Возвращаемся в лобби
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
        }
