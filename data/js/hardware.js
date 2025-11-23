import { setLanguage, currentLanguage } from "./language.js";
import { websocket } from "./index.js";

// Dynamic creation of valve controls
function createValveControls(plantCount) {
    const valveContainer = document.getElementById('valve-controls');
    valveContainer.innerHTML = ''; // Clear existing controls
    
    for(let i = 0; i < plantCount; i++) {
        const valveDiv = document.createElement('div');
        valveDiv.className = 'valve-control';

        valveDiv.innerHTML = `
            <div id="moisture_sensor_${i}" class="moistureSensor" style="display:none;"></div>
            <div>
                <span><span data-translate="magnetic_valve">Magnetic Valve</span> ${i+1}:</span>
                <br /><br />
                <span id="valve_${i}"></span>
                <label class="valve-switch" for="valve_switch_${i}">
                    <input type="checkbox" id="valve_switch_${i}" />
                    <div class="valve-slider round"></div>
                </label>
            </div>
        `;
        valveContainer.appendChild(valveDiv);
        
        // Add event listener
        document.getElementById(`valve_switch_${i}`).addEventListener('change', e => {
            websocket.send(JSON.stringify({
                "action": "valve_switch",
                "valve_id": i  // Send 0-based index
            }));
        });
    }
}

// Update moisture sensor data
function updateMoistureSensors(data) {
    // If moisture sensors are disabled or count is zero, do nothing
    if (!data.enabled || data.count === 0) {
        return;
    }
    
    // Iterate through each sensor and update its display
    data.sensors.forEach((sensor, index) => {
        const sensorId = `moisture_sensor_${index}`;
        const sensorValveDiv = document.getElementById(sensorId);
        
        // check if element exists
        if (!sensorValveDiv) return;

        sensorValveDiv.innerHTML = ''; // Clear existing content
        
        const sensorWrapper = document.createElement('div');
        sensorWrapper.className = 'moisture-sensor-wrapper';
        
        const statusClass = sensor.isDry ? 'dry' : 'ok';
        const statusText = sensor.isDry ? 'DRY' : 'OK';
        
        sensorWrapper.innerHTML = `
            <div class="moisture-sensor-title"><span data-translate="moisturesensor">Moisture Sensor</span> ${sensor.id}:</div>
            <div class="moisture-sensor-item">
                <div class="sensor-header">
                    <span class="sensor-label" data-translate="plant_">Plant</span> <span class="sensor-label">${sensor.id}</span>
                    <span class="sensor-status ${statusClass}" data-translate="${statusText}">${statusText}</span>
                </div>
                <div class="moisture-bar-container">
                    <div class="moisture-bar ${statusClass}" style="width: ${sensor.percent}%"></div>
                </div>
                <div class="sensor-details">
                    <span>${sensor.analog}/${sensor.percent}%</span>
                    <span class="sensor-pin">Pin ${sensor.pin}</span>
                </div>
            </div>
        `;
        
        //container.appendChild(sensorWrapper);
        sensorValveDiv.appendChild(sensorWrapper);
    });

    // Re-apply translations to new elements
    setLanguage(currentLanguage);
}

export { createValveControls, updateMoistureSensors };