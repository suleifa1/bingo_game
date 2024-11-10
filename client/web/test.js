import Fireworks from 'https://cdn.jsdelivr.net/npm/fireworks-js@2.x/dist/index.umd.js';

// Инициализация
const container = document.querySelector('.fireworks');
const fireworks = new Fireworks(container);
fireworks.start();
