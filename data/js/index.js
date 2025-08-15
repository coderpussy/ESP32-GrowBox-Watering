// Initialization
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);
var action;

// Dashboard control elements
var auto_switch = document.getElementById("auto_switch");
var valve_switch_1 = document.getElementById("valve_switch_1");
var valve_switch_2 = document.getElementById("valve_switch_2");
var valve_switch_3 = document.getElementById("valve_switch_3");
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

// Checkbox valve switches event listener
auto_switch.addEventListener('change', e => {
    websocket.send(JSON.stringify({"action":"auto_switch","auto_switch":e.target.checked}));
});
valve_switch_1.addEventListener('change', e => {
    websocket.send(JSON.stringify({"action":"valve_switch_1","valve_switch_1":e.target.checked}));
});
valve_switch_2.addEventListener('change', e => {
    websocket.send(JSON.stringify({"action":"valve_switch_2","valve_switch_2":e.target.checked}));
});
valve_switch_3.addEventListener('change', e => {
    websocket.send(JSON.stringify({"action":"valve_switch_3","valve_switch_3":e.target.checked}));
});
pump_switch.addEventListener('change', e => {
    websocket.send(JSON.stringify({"action":"pump_switch","pump_switch":e.target.checked}));
});

// Reset soil flow volume event listener
const resetCounterBtn = document.getElementById("resetCounter");
resetCounterBtn.addEventListener('click', resetCounter);

// Save settings event listener
const saveSettingsBtn = document.getElementById("savesettings");
saveSettingsBtn.addEventListener('click', saveSettings);

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
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onload(event) {
    initWebSocket();
    initToggleOverlay();
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

function onClose(event) {
    console.log('Verbindung geschlossen');
    setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
    let data = JSON.parse(event.data);

    action = data.action;
    console.log('message:',data);
    
    if (action == "getsettings" || action == "savesettings") {
        // Update settings checkboxes
        use_webserial.checked = data.use_webserial;
        use_flowsensor.checked = data.use_flowsensor;
        use_moisturesensor.checked = data.use_moisturesensor;

        // Show/hide webserial settings based on use_webserial
        if (use_webserial.checked) {
            document.querySelector(".topnav .webserial").style.display = "normal";
            document.querySelector("#overlay-webserial").style.display = "normal";
        } else {
            document.querySelector(".topnav .webserial").style.display = "none";
            document.querySelector("#overlay-webserial").style.display = "none";
        }
        // Show/hide flow sensor settings based on use_flowsensor
        if (use_flowsensor.checked) {
            document.querySelector("#soilFlowVolumeWrapper").style.display = "normal";
        } else {
            document.querySelector("#soilFlowVolumeWrapper").style.display = "none";
        }
        // Show/hide moisture sensor settings based on use_moisturesensor
        if (use_moisturesensor.checked) {
            document.querySelector("#moistureSensorsWrapper").style.display = "normal";
        } else {
            document.querySelector("#moistureSensorsWrapper").style.display = "none";
        }
    } else if (action == "setjoblist") {
        // Set job list from received joblist data
        setJobList(data.joblist);
    } else {
        // Update dashboard control elements
        auto_switch.checked = data.auto_switch;
        valve_switch_1.checked = data.valve_switch_1;
        valve_switch_2.checked = data.valve_switch_2;
        valve_switch_3.checked = data.valve_switch_3;
        // Update pump control
        pump_switch.checked = data.pump_switch;
        // Update pump run time
        pumpRunTime.innerText = data.pumpRunTime;
        // Update soil flow volume
        soilFlowVolume.innerText = data.soilFlowVolume;

        /*led_level.value = (data.test *1).toFixed(1);*/ // example of updating a control element
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
    websocket.send(JSON.stringify({"action":"savesettings","use_webserial":use_webserial.checked,"use_flowsensor":use_flowsensor.checked,"use_moisturesensor":use_moisturesensor.checked}));

    toggleOverlay("settings");
}

function toggleOverlay(mode) {
    document.getElementById("overlay-" + mode).classList.toggle("active");
}