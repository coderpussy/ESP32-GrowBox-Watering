// Initialization
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);
var action;

var auto_switch = document.getElementById("auto_switch");
var valve_switch_1 = document.getElementById("valve_switch_1");
var valve_switch_2 = document.getElementById("valve_switch_2");
var valve_switch_3 = document.getElementById("valve_switch_3");

var pump_switch = document.getElementById("pump_switch");
var pumpRunTime = document.getElementById("pumpRunTime");

var soilFlowVolume = document.getElementById("soilFlowVolume");

/*var oc_azimut = document.getElementById("oc_azimut");
var oc_elevation = document.getElementById("oc_elevation");
var oc_el_offset = document.getElementById("oc_el_offset");
var oc_az_offset = document.getElementById("oc_az_offset");
var oc_motor_speed = document.getElementById("oc_motor_speed");

var om_time = document.getElementById("om_time");
var om_speed = document.getElementById("om_speed");
var om_steps = document.getElementById("om_steps");
var om_azimut = document.getElementById("om_azimut");
var om_elevation = document.getElementById("om_elevation");

var led_level = document.getElementById("led_level");
var state = document.getElementById("state");
var azimut = document.getElementById("azimut");
var elevation = document.getElementById("elevation");

var s_azimut = document.getElementById("s_azimut");
var s_elevation = document.getElementById("s_elevation");

var slider1 = document.getElementById("myAzRange");
var output1 = document.getElementById("d_azimut");

var slider2 = document.getElementById("myElRange");
var output2 = document.getElementById("d_elevation");

var slider3 = document.getElementById("myRotorRange");
var output3 = document.getElementById("rotor");
*/

// Slider on change functions
/*slider1.onchange = function() {
    output1.innerHTML = (this.value *1).toFixed(1);
    websocket.send(JSON.stringify({"action":"slider1","level":this.value}));
}

slider2.onchange = function() {
    output2.innerHTML = (this.value *1).toFixed(1);
    websocket.send(JSON.stringify({"action":"slider2","level":this.value}));
}

slider3.onchange = function() {
    output3.innerHTML = (this.value *1).toFixed(1);
    websocket.send(JSON.stringify({"action":"slider3","level":this.value}));
}*/

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
        /*oc_azimut.value = data.azimut;
        oc_elevation.value = data.elevation;
        oc_el_offset.value = data.el_offset;
        oc_az_offset.value = data.az_offset;
        oc_motor_speed.value = data.motor_speed;*/
    } else {
        auto_switch.checked = data.auto_switch;
        valve_switch_1.checked = data.valve_switch_1;
        valve_switch_2.checked = data.valve_switch_2;
        valve_switch_3.checked = data.valve_switch_3;

        pump_switch.checked = data.pump_switch;
        pumpRunTime.innerText = data.pumpRunTime;

        soilFlowVolume.innerText = data.soilFlowVolume;

        /*led_level.value = (data.led_level *1).toFixed(1);
        state.innerText = data.state;
        
        azimut.innerText = (data.azimut *1).toFixed(1);
        elevation.innerText = (data.elevation *1).toFixed(1);
        om_azimut.innerText = (data.azimut *1).toFixed(1);
        om_elevation.innerText = (data.elevation *1).toFixed(1);
    
        s_azimut.innerText = (data.s_azimut *1).toFixed(1);
        s_elevation.innerText = (data.s_elevation *1).toFixed(1);
        
        output1.innerHTML = (data.d_azimut*1).toFixed(1);
        slider1.value = data.d_azimut;
        
        output2.innerHTML = (data.d_elevation*1).toFixed(1);
        slider2.value = data.d_elevation;
    
        output3.innerHTML = (data.rotor*1).toFixed(1);
        slider3.value = data.rotor;*/
    }
}

/*function button_clicked(action) {
    if (action == "om_el_up" || action == "om_el_down") {
        websocket.send(JSON.stringify({"time":om_time.value,"speed":om_speed.value,"action":action}));
    }
    else if (action == "om_az_up" || action == "om_az_down") {
        if (action == "om_az_up") {
            if (om_steps.value == "short") {
                websocket.send(JSON.stringify({"action":"rotor_up_step"}));
            } else {
                websocket.send(JSON.stringify({"action":"rotor_up"}));
            }
        } else {
            if (om_steps.value == "short") {
                websocket.send(JSON.stringify({"action":"rotor_down_step"}));
            } else {
                websocket.send(JSON.stringify({"action":"rotor_down"}));
            }
        }
    }
    else {
        websocket.send(JSON.stringify({"action":action}));
    }
}*/

function getValues() {
    websocket.send(JSON.stringify({"action":"getvalues"}));
}

function getSettings() {
    websocket.send(JSON.stringify({"action":"getsettings"}));
}

function resetCounter() {
    websocket.send(JSON.stringify({"action":"resetcounter"}));
}

function saveSettings() {    
    //websocket.send(JSON.stringify({"azimut":oc_azimut.value,"elevation":oc_elevation.value,"az_offset":oc_az_offset.value,"el_offset":oc_el_offset.value,"motor_speed":oc_motor_speed.value,"action":"savesettings"}));

    toggleOverlay("settings");
}

function toggleOverlay(mode) {
    document.getElementById("overlay-" + mode).classList.toggle("active");
}