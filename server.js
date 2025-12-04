const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

// Serve static files from 'public' directory
app.use(express.static(path.join(__dirname, 'public')));

// Initialize map (10x10 grid for simplicity for now, can be resized)
// Each cell can be an object: { type: 'ground', unit: null }
const MAP_SIZE = 20;
const map = [];

function initMap() {
    for (let y = 0; y < MAP_SIZE; y++) {
        const row = [];
        for (let x = 0; x < MAP_SIZE; x++) {
            row.push({
                x,
                y,
                type: 'grass', // default terrain
                unit: null
            });
        }
        map.push(row);
    }
}

initMap();

io.on('connection', (socket) => {
    console.log('A user connected:', socket.id);

    // Send the current map state to the new client
    socket.emit('init_map', map);

    // Handle map edits
    socket.on('edit_map', (data) => {
        // data: { x, y, changeType, value }
        // changeType could be 'terrain' or 'unit'
        const { x, y, changeType, value } = data;

        if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE) {
            if (changeType === 'terrain') {
                map[y][x].type = value;
            } else if (changeType === 'unit') {
                map[y][x].unit = value;
            }

            // Broadcast the update to all clients (including sender)
            io.emit('update_map', { x, y, cell: map[y][x] });
        }
    });

    socket.on('disconnect', () => {
        console.log('User disconnected:', socket.id);
    });
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`Server running on port ${PORT}`);
});
