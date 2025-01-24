var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var timeData = [];
var distanceData = [];
var updateInterval = 1000; // Updates every 1 second
var lastUpdate = Date.now();

window.addEventListener('load', onLoad);

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('WebSocket connection opened');
    document.getElementById('status').textContent = "Connected";
    document.getElementById('status').style.color = "green";
}

function onClose(event) {
    console.log('WebSocket connection closed');
    document.getElementById('status').textContent = "Disconnected";
    document.getElementById('status').style.color = "red";
    setTimeout(initWebSocket, 2000); // Retry connection after 2 seconds
}

function onMessage(event) {
    console.log('WebSocket Data:', event.data);

    var dataArray = event.data.split(" ");

    // Parse time
    if (dataArray.includes("temps")) {
        var timeIndex = dataArray.indexOf("temps") + 1;
        var timeValue = parseFloat(dataArray[timeIndex]);
        document.getElementById('temps').innerHTML = `${timeValue.toFixed(2)} seconds`;
        timeData.push(timeValue);
    }

    // Parse distance
    if (dataArray.includes("distance")) {
        var distanceIndex = dataArray.indexOf("distance") + 1;
        var distanceValue = parseFloat(dataArray[distanceIndex]);
        if (distanceValue === -1) {
            document.getElementById('distance').innerHTML = `Out of Range`;
            distanceData.push(null); // Push null for "Out of Range"
        } else {
            document.getElementById('distance').innerHTML = `${distanceValue.toFixed(2)} mm`;
            distanceData.push(distanceValue);
        }
    }

    // Limit the number of data points
    if (timeData.length > 100) {
        timeData.shift();
        distanceData.shift();
    }

    // Update metrics
    updateMetrics();
}

function onLoad(event) {
    initWebSocket();
}

function initChart() {
    const ctx = document.getElementById('distanceChart').getContext('2d');
    window.distanceChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [], // Time labels
            datasets: [{
                label: 'Distance',
                data: [], // Distance data
                borderColor: 'rgba(75, 192, 192, 1)',
                borderWidth: 1,
                fill: false
            }]
        },
        options: {
            scales: {
                x: {
                    type: 'time',
                    time: {
                        unit: 'second'
                    },
                    title: {
                        display: true,
                        text: 'Time'
                    }
                },
                y: {
                    title: {
                        display: true,
                        text: 'Distance (cm)'
                    }
                }
            }
        }
    });
}

function updateChart(time, distance) {
    window.distanceChart.data.labels.push(time);
    window.distanceChart.data.datasets[0].data.push(distance);
    window.distanceChart.update();
}

function updateMetrics() {
    const validDistances = distanceData.filter(d => d !== null);
    if (validDistances.length > 0) {
        const maxDistance = Math.max(...validDistances);
        const minDistance = Math.min(...validDistances);
        const avgDistance = validDistances.reduce((a, b) => a + b, 0) / validDistances.length;

        document.getElementById('maxDistance').innerHTML = `${maxDistance.toFixed(2)} mm`;
        document.getElementById('minDistance').innerHTML = `${minDistance.toFixed(2)} mm`;
        document.getElementById('avgDistance').innerHTML = `${avgDistance.toFixed(2)} mm`;
    } else {
        document.getElementById('maxDistance').innerHTML = `N/A`;
        document.getElementById('minDistance').innerHTML = `N/A`;
        document.getElementById('avgDistance').innerHTML = `N/A`;
    }
}