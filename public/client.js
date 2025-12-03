const socket = io();

const canvas = document.getElementById('mapCanvas');
const ctx = canvas.getContext('2d');

const TILE_SIZE = 32;
const MAP_SIZE = 20;

let map = [];
let assets = {};
let selectedTool = {
    type: 'terrain',
    value: 'grass'
};

// Asset loading
const unitNames = [
    'drone', 'hatchery', 'hydrarisk', 'larva', 'marine',
    'multalrisk', 'overloard', 'ultrarisk', 'zealot', 'zergling'
];

function loadAssets() {
    unitNames.forEach(name => {
        const img = new Image();
        img.src = `assets/${name}.png`;
        img.onload = () => {
            assets[name] = img;
            drawMap(); // Redraw when asset loads
        };
    });
}

loadAssets();

// Socket events
socket.on('init_map', (data) => {
    map = data;
    drawMap();
});

socket.on('update_map', (data) => {
    const { x, y, cell } = data;
    map[y][x] = cell;
    drawMap(); // In efficient but simple: redraw whole map on update. Can optimize later.
});

// Input handling
const buttons = document.querySelectorAll('.tool-btn');
buttons.forEach(btn => {
    btn.addEventListener('click', () => {
        // Deselect all
        buttons.forEach(b => b.classList.remove('selected'));
        // Select clicked
        btn.classList.add('selected');

        selectedTool.type = btn.dataset.type;
        selectedTool.value = btn.dataset.value;
    });
});

canvas.addEventListener('mousedown', handleMapClick);
canvas.addEventListener('mousemove', (e) => {
    if (e.buttons === 1) { // Left click held down
        handleMapClick(e);
    }
});

function handleMapClick(e) {
    const rect = canvas.getBoundingClientRect();
    const x = Math.floor((e.clientX - rect.left) / TILE_SIZE);
    const y = Math.floor((e.clientY - rect.top) / TILE_SIZE);

    if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE) {
        // Send update to server
        socket.emit('edit_map', {
            x,
            y,
            changeType: selectedTool.type,
            value: selectedTool.value
        });
    }
}

// Rendering
function drawMap() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    if (!map || map.length === 0) return;

    for (let y = 0; y < map.length; y++) {
        for (let x = 0; x < map[y].length; x++) {
            const cell = map[y][x];
            const px = x * TILE_SIZE;
            const py = y * TILE_SIZE;

            // Draw Terrain
            if (cell.type === 'grass') {
                ctx.fillStyle = '#4CAF50';
            } else if (cell.type === 'dirt') {
                ctx.fillStyle = '#8D6E63';
            } else if (cell.type === 'water') {
                ctx.fillStyle = '#2196F3';
            } else {
                ctx.fillStyle = '#000';
            }
            ctx.fillRect(px, py, TILE_SIZE, TILE_SIZE);

            // Draw Grid Lines
            ctx.strokeStyle = '#333';
            ctx.lineWidth = 1;
            ctx.strokeRect(px, py, TILE_SIZE, TILE_SIZE);

            // Draw Unit
            if (cell.unit && cell.unit !== 'none' && assets[cell.unit]) {
                ctx.drawImage(assets[cell.unit], px, py, TILE_SIZE, TILE_SIZE);
            }
        }
    }
}
