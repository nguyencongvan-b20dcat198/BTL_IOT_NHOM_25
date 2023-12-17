const express = require('express');
const app = express();
const cors = require('cors');
const mysql2 = require('mysql2');
const mqtt = require('mqtt');
const WebSocket = require('ws');

const db = mysql2.createConnection({
    host: 'localhost',
    user: 'root',
    password: 'root123',
    database: 'demo',
    dateStrings: ['DATETIME']
});

const mqttClient = mqtt.connect('mqtt://localhost:1883');
const server = require('http').createServer(app);
const wss = new WebSocket.Server({ server: server });
const bodyParser = require('body-parser');

db.connect((error) => error ? console.error('Connect error:', error) : console.log('Connected MySQL'));

app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json());
app.use(cors());

wss.on('connection', (ws) => {
    console.log('Client Connected')
    ws.on('message', (message) => {
        const json = (message.toString()).split('|');
        db.query(`INSERT INTO action (device_id,status,time) VALUES('${json[0]}','${json[1]}','${timeNow()}')`, (err) => { });
        mqttClient.publish('button', json[0] + "|" + json[1]);
    });
    ws.on('close', () => console.log('Client Disconnected'));
});

mqttClient.on('message', (topic, message) => {
    const json = (message.toString()).split('|');
    if (topic === "sensor") db.query(`INSERT INTO sensor (device_id, humidity, temperature, mq2_value, time) VALUES ('${json[0]}', ${json[1]}, ${json[2]}, ${json[3]},'${timeNow()}')`, (err) => { });
    wss.clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(message.toString());
        }
    });
});

app.get('/sensor', (req, res) => db.query("SELECT * FROM sensor", (err, data) => (err) ? console.error(err) : res.send(JSON.stringify(data))));
app.get('/action', (req, res) => db.query("SELECT * FROM action", (err, data) => (err) ? console.error(err) : res.send(JSON.stringify(data))));

mqttClient.on('connect', () => {
    mqttClient.subscribe('sensor');
    mqttClient.subscribe('action');
});

server.listen(3000, () => { });

function timeNow() {
    const dateTime = new Date();
    time = dateTime.toTimeString().split(' ')[0];
    [month, day, year] = dateTime.toLocaleDateString().split('/');
    return year + '-' + month + '-' + day + ' ' + time;
}