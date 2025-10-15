// Import necessary modules
import { setJobList, updatePlantSelect } from "./scheduler.js";

// Default language
export let currentLanguage = 'en';

// Initialization
var gateway = `ws://${window.location.hostname}/ws`;
export var websocket;

window.addEventListener('load', onload);
var action;

// Dashboard control elements
var auto_switch = document.getElementById("auto_switch");
var plantCountInput = document.getElementById("plant_count");
/*var valve_switch_1 = document.getElementById("valve_switch_1");
var valve_switch_2 = document.getElementById("valve_switch_2");
var valve_switch_3 = document.getElementById("valve_switch_3");*/
// Pump control element
var pump_switch = document.getElementById("pump_switch");
// Pump run time display
var pumpRunTime = document.getElementById("pumpRunTime");
// Soil flow volume display
var soilFlowVolume = document.getElementById("soilFlowVolume");

// Settings control elements
var use_webserial = document.getElementById("use_webserial");
var use_flowsensor = document.getElementById("use_flowsensor");
var use_moisturesensor = document.getElementById("use_moisturesensor");
var autoSwitchEnabled = document.getElementById("auto_switch_enabled");

// Language selection event listener
var language_select = document.getElementById("language-select");
language_select.addEventListener("change", e => {
    setLanguage(e.target.value);
});
// Checkbox valve switches event listener
auto_switch.addEventListener('change', e => {
    websocket.send(JSON.stringify({"action":"auto_switch","auto_switch":e.target.checked}));
});
pump_switch.addEventListener('change', e => {
    websocket.send(JSON.stringify({"action":"pump_switch","pump_switch":e.target.checked}));
});

plantCountInput.addEventListener('change', e => {
    const count = parseInt(e.target.value);
    if (count >= 1 && count <= 8) {
        createValveControls(count);
        updatePlantSelect(count);
    }
});

// Dynamic creation of valve controls
function createValveControls(plantCount) {
    const valveContainer = document.getElementById('valve-controls');
    valveContainer.innerHTML = ''; // Clear existing controls
    
    for(let i = 0; i < plantCount; i++) {
        const valveDiv = document.createElement('td');
        valveDiv.innerHTML = `
            <table><tr>
                <td style="text-align: center;">
                    <span data-translate="magnetic_valve_${i+1}">Magnetic Valve ${i+1}:</span><br /><br />
                    <span id="valve_${i+1}"></span>
                    <label class="valve-switch" for="valve_switch_${i+1}">
                        <input type="checkbox" id="valve_switch_${i+1}" />
                        <div class="valve-slider round"></div>
                    </label>
                </td>
            </tr></table>
        `;
        valveContainer.appendChild(valveDiv);
        
        // Add event listener
        document.getElementById(`valve_switch_${i+1}`).addEventListener('change', e => {
            websocket.send(JSON.stringify({
                "action": "valve_switch",
                "valve_id": i + 1  // Send 1-based index
            }));
        });
    }
}

// Reset soil flow volume event listener
const resetCounterBtn = document.getElementById("resetCounter");
resetCounterBtn.addEventListener('click', resetCounter);

// Save settings event listener
const saveSettingsBtn = document.getElementById("savesettings");
saveSettingsBtn.addEventListener('click', saveSettings);

async function loadLanguage(lang) {
    const response = await fetch(`./lang/${lang}.json`);
    const translations = await response.json();
    return translations;
}

export async function setLanguage(lang) {
    currentLanguage = lang;
    const translations = await loadLanguage(lang);
    applyTranslations(translations);
}

function applyTranslations(translations) {
    document.querySelectorAll('[data-translate]').forEach(element => {
        const key = element.getAttribute('data-translate');
        if (translations[key]) {
            // Check if the element is an input or button
            if (element.tagName === 'INPUT' && element.type === 'button') {
                element.value = translations[key]; // Update button value
            } else if (element.tagName === 'INPUT' && element.hasAttribute('placeholder')) {
                element.placeholder = translations[key]; // Update placeholder text
            } else {
                element.innerText = translations[key]; // Update inner text for other elements
            }
        }
    });
}

function initToggleOverlay() {
    const overlaycalendar = document.querySelector('#overlay-calendar');
    const overlaywebserial = document.querySelector('#overlay-webserial');
    const overlaysettings = document.querySelector('#overlay-settings');

    overlaycalendar.addEventListener('click', e => { e.target.id === overlaycalendar.id ? toggleOverlay("calendar"):null });
    overlaywebserial.addEventListener('click', e => { e.target.id === overlaywebserial.id ? toggleOverlay("webserial"):null });
    overlaysettings.addEventListener('click', e => { e.target.id === overlaysettings.id ? toggleOverlay("settings"):null });

    document.querySelectorAll('.toggle-overlay').forEach(function (elem) {
        //console.log(elem);
        elem.addEventListener('click', function () {
            if (elem.classList.contains("calendar")) {
                toggleOverlay("calendar");
            }
            if (elem.classList.contains("webserial")) {
                toggleOverlay("webserial");
            }
            if (elem.classList.contains("settings")) {
                toggleOverlay("settings");
            }
        });
    });
}

function initWebSocket() {
    console.log('Versuche WebSocket Verbindung herzustellen…');
    let retries = 0;
    const maxRetries = 5;
    
    function connect() {
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = () => {
            if (retries < maxRetries) {
                retries++;
                setTimeout(connect, 2000 * retries);
            }
        };
        websocket.onmessage = onMessage;
    }
    
    connect();
}

function onload(event) {
    initWebSocket();
    initToggleOverlay();
    setLanguage(currentLanguage); // Set default language on load
}

function onOpen(event) {
    console.log('Verbindung hergestellt');
    // get initial values data
    getValues();
    // get initial settings data
    getSettings();
    // get job list data
    getJobList();
}

function onMessage(event) {
    let data = JSON.parse(event.data);

    action = data.action;
    console.log('message:',data);
    
    if (action == "setvalues") {
        // Update dashboard control elements
        auto_switch.checked = data.auto_switch;

        // Handle dynamic valve states
        if (data.valves) {
            data.valves.forEach(valve => {
                const switchElem = document.getElementById(`valve_switch_${valve.id}`);
                if (switchElem) {
                    switchElem.checked = valve.state;
                }
            });
        }

        // Update pump control
        pump_switch.checked = data.pump_switch;
        // Update pump run time
        pumpRunTime.innerText = data.pumpRunTime;
        
        if (data.soilFlowVolume) {
            // Update soil flow volume
            soilFlowVolume.innerText = data.soilFlowVolume;
        }
    } else if (action == "setsettings") {
        // Update settings checkboxes
        use_webserial.checked = data.use_webserial;
        use_flowsensor.checked = data.use_flowsensor;
        use_moisturesensor.checked = data.use_moisturesensor;
        autoSwitchEnabled.checked = data.auto_switch_enabled;
        if (data.auto_switch_enabled) auto_switch.checked = data.auto_switch_enabled;

        plantCountInput.value = data.plant_count || 3;
        createValveControls(data.plant_count || 3);
        updatePlantSelect(data.plant_count || 3);

        // Show/hide webserial settings based on use_webserial
        if (use_webserial.checked) {
            document.querySelector(".topnav .webserial").style.display = "block";
            document.querySelector("#overlay-webserial").style.display = "grid";
        } else {
            document.querySelector(".topnav .webserial").style.display = "none";
            document.querySelector("#overlay-webserial").style.display = "none";
        }
        // Show/hide flow sensor settings based on use_flowsensor
        if (use_flowsensor.checked) {
            document.querySelector("#soilFlowVolumeWrapper").style.display = "inline-block";
        } else {
            document.querySelector("#soilFlowVolumeWrapper").style.display = "none";
        }
        // Show/hide moisture sensor settings based on use_moisturesensor
        if (use_moisturesensor.checked) {
            document.querySelector("#moistureSensorsWrapper").style.display = "block";
        } else {
            document.querySelector("#moistureSensorsWrapper").style.display = "none";
        }
    } else if (action == "setjoblist") {
        // Set job list from received joblist data
        setJobList(data.joblist);
    }
}

function getValues() {
    websocket.send(JSON.stringify({"action":"getvalues"}));
}

function getSettings() {
    websocket.send(JSON.stringify({"action":"getsettings"}));
}

function getJobList() {
    websocket.send(JSON.stringify({"action":"getjoblist"}));
}

function resetCounter() {
    websocket.send(JSON.stringify({"action":"resetcounter"}));
}

function saveSettings() {
    websocket.send(JSON.stringify({
        "action": "savesettings",
        "use_webserial": use_webserial.checked,
        "use_flowsensor": use_flowsensor.checked,
        "use_moisturesensor": use_moisturesensor.checked,
        "auto_switch_enabled": autoSwitchEnabled.checked,
        "plant_count": parseInt(plantCountInput.value)
    }));
    toggleOverlay("settings");
}

function toggleOverlay(mode) {
    document.getElementById("overlay-" + mode).classList.toggle("active");
}