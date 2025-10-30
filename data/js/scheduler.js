import { setLanguage, currentLanguage } from "./language.js";
import { websocket } from "./index.js";

const jobNameInput = document.getElementById("job-name");
const jobselectInput = document.getElementById("job-select");
const moistureMinInput = document.getElementById("moisture-min");
const moistureMaxInput = document.getElementById("moisture-max");
const plantselectInput = document.getElementById("plant-select");
const jobVolumeInput = document.getElementById("job-volume");
const jobDurationInput = document.getElementById("job-duration");
const starttimeInput = document.getElementById("starttime");
const everydayInput = document.getElementById("everyday");
const addJobButton = document.getElementById("add-job");
const saveJobsButton = document.getElementById("save-jobs");
const jobList = document.getElementById("job-list");
const deleteJoblistButton = document.getElementById("delete-joblist");

deleteJoblistButton.addEventListener("click", deleteJoblist);
addJobButton.addEventListener("click", addJob);
saveJobsButton.addEventListener("click", saveJobs);
jobList.addEventListener("click", doJobList);
jobselectInput.addEventListener('change', toggleJobTriggerFields);

// Edit index (-1 = not editing)
let editIndex = -1;

// Initial button state
updateSaveButtonState();
// Initial job trigger fields state
toggleJobTriggerFields();

// Function to update save button state
function updateSaveButtonState() {
    const hasJobs = jobList.children.length > 0;
    saveJobsButton.disabled = !hasJobs;
    saveJobsButton.style.opacity = hasJobs ? "1" : "0.5";
    saveJobsButton.style.cursor = hasJobs ? "pointer" : "not-allowed";
}

function clearFormAndEditState() {
    editIndex = -1;
    jobNameInput.value = "";
    jobselectInput.value = 0;
    moistureMinInput.value = "";
    moistureMaxInput.value = "";
    plantselectInput.value = 0;
    jobVolumeInput.value = "";
    jobDurationInput.value = "";
    starttimeInput.value = "";
    everydayInput.checked = false;
    addJobButton.value = "Add Job";

    // Update job trigger fields based on default selection
    toggleJobTriggerFields();
}

function populateFormFromJobItem(jobItem) {
    const name = jobItem.querySelector('.itemjobname').innerText || "";
    const job = jobItem.querySelector('.itemjobselect').innerText || 0;
    const moistureMin = jobItem.querySelector('.itemmoisturemin').innerText || "";
    const moistureMax = jobItem.querySelector('.itemmoisturemax').innerText || "";
    const plant = jobItem.querySelector('.itemplantselect').innerText || 0;
    const jobvolumeText = jobItem.querySelector('.itemjobvolume').innerText || "";
    const durationText = jobItem.querySelector('.itemjobduration').innerText || "";
    const starttime = jobItem.querySelector('.itemstarttime').innerText || "";
    const everydayText = jobItem.querySelector('.itemeveryday').innerText || "false";

    // Add activate button state check
    const activateBtn = jobItem.querySelector('button');
    const isActive = activateBtn.classList.contains('active');
    
    // Store the active state in a data attribute on the form
    addJobButton.dataset.previousState = isActive ? 'active' : 'inactive';

    jobNameInput.value = name;
    jobselectInput.value = job;
    moistureMinInput.value = moistureMin;
    moistureMaxInput.value = moistureMax;
    plantselectInput.value = plant;
    jobVolumeInput.value = jobvolumeText;
    jobDurationInput.value = durationText;
    starttimeInput.value = starttime;
    everydayInput.checked = (everydayText === "true" || everydayText === "True");
}

function updatePlantSelect(plantCount) {
    const plantSelect = document.getElementById('plant-select');
    plantSelect.innerHTML = `
        <option value="0" data-translate="all_plants">All Plants</option>
        ${Array.from({length: plantCount}, (_, i) => `
            <option value="${i+1}"><span data-translate="plant_">Plant</span> ${i+1}</option>
        `).join('')}
    `;
}

function startEdit(index) {
    const children = Array.from(jobList.children);
    if (index < 0 || index >= children.length) return;

    const jobItem = children[index];
    populateFormFromJobItem(jobItem);
    // Update job trigger fields based on current selection
    toggleJobTriggerFields();
    editIndex = index;
    addJobButton.value = "Save Changes";
    
    // scroll overlay to top for visibility (optional)
    const overlay = document.getElementById('overlay-calendar');
    if (overlay) overlay.scrollTop = 0;
}

function addJob() {
    const jobname = jobNameInput.value;
    const jobselect = jobselectInput.value;
    const moistureMin = moistureMinInput.value;
    const moistureMax = moistureMaxInput.value;
    const plantselect = plantselectInput.value;
    const jobvolume = jobVolumeInput.value;
    const jobduration = jobDurationInput.value;
    const starttime = starttimeInput.value;
    const everyday = everydayInput.checked;
    let deadline = "";

    if (jobname.trim() === "") {
        alert("Job name cannot be empty.");
        return; // Don't add task if task or deadline is empty
    }

    if (!jobselect == 1) { // If job type is time-based or both
        if (starttime === "" && !everyday) {
            alert("Please select an upcoming date for the start time.");
            return;
        }

        if (!everyday) {
            const selectedDate = new Date(starttime);
            const currentDate = new Date();
            if ((selectedDate <= currentDate)) {
                alert("Please select an upcoming date in the future for the start time.");
                return;
            }
        }
    }

    // If we are editing an existing job, update it instead of creating new
    if (editIndex >= 0) {
        const children = Array.from(jobList.children);
        if (editIndex < children.length) {
            const jobItem = children[editIndex];
            const activateBtn = jobItem.querySelector('button');
            
            // Preserve the active state from before editing
            if (addJobButton.dataset.previousState === 'active') {
                activateBtn.classList.remove('activate');
                activateBtn.classList.add('active');
                activateBtn.innerText = "Active";
                jobItem.style.backgroundColor = "#737373";
            } else {
                activateBtn.classList.remove('active');
                activateBtn.classList.add('activate');
                activateBtn.innerText = "Activate";
                jobItem.style.backgroundColor = "initial";
            }
            // Update job item details
            //jobItem.querySelector('.activate').innerText = "Activate";
            jobItem.querySelector('.itemjobname').innerText = jobname;
            jobItem.querySelector('.itemjobselect').innerText = jobselect;
            jobItem.querySelector('.itemmoisturemin').innerText = moistureMin;
            jobItem.querySelector('.itemmoisturemax').innerText = moistureMax;
            jobItem.querySelector('.itemplantselect').innerText = plantselect;
            jobItem.querySelector('.itemjobvolume').innerText = jobvolume;
            jobItem.querySelector('.itemjobduration').innerText = jobduration;
            jobItem.querySelector('.itemstarttime').innerText = starttime;
            jobItem.querySelector('.itemeveryday').innerText = everyday ? "true" : "false";
            // Clean up the data attribute
            delete addJobButton.dataset.previousState;
            // Reset form and edit state
            clearFormAndEditState();
            return;
        } else {
            // index out of range - reset
            clearFormAndEditState();
        }
    }
    
    // Create new job item
    const jobItem = document.createElement("div");
    jobItem.classList.add("job");
    // Set translated job selection text
    let jobselectText = "";
    switch(parseInt(jobselect)) {
        case 0: jobselectText = "timebased"; break;
        case 1: jobselectText = "moisturesensor"; break;
        case 2: jobselectText = "time_moisture"; break;
    }
    // Set "--" to separated text variables in case of any of 
    // jobvolume or jobduration or starttime or every day or moisture min or moisture max is empty
    let jobvolumeText = jobvolume ? "" : "--";
    let jobdurationText = jobduration ? "" : "--";
    let starttimeText = starttime ? new Date(starttime).toLocaleString() : "--";
    let everydayText = jobselect == 0 ? everyday ? "yes" : "no" : "--";
    let moistureMinText = moistureMin ? "" : "--";
    let moistureMaxText = moistureMax ? "" : "--";

    jobItem.innerHTML = `
        <button class="activate">Activate</button>
        <p><span data-translate="job_name">Name:</span><span class="itemjobname">${jobname}</span></p>
        <p><span data-translate="job_type">Job:</span><span><span class="itemjobselect">${jobselect}</span> - <span data-translate="${jobselectText}"></span></span></p>
        <p><span data-translate="moisture_min">Moisture Min:</span><span><span class="itemmoisturemin">${moistureMin}</span><span>${moistureMinText}</span><span>%</span></span></p>
        <p><span data-translate="moisture_max">Moisture Max:</span><span><span class="itemmoisturemax">${moistureMax}</span><span>${moistureMaxText}</span><span>%</span></span></p>
        <p><span data-translate="plant">Plant:</span><span class="itemplantselect">${plantselect}</span></p>
        <p><span data-translate="volume">Volume:</span><span><span class="itemjobvolume">${jobvolume}</span><span>${jobvolumeText}</span><span>ml.</span></span></p>
        <p><span data-translate="duration">Duration:</span><span><span class="itemjobduration">${jobduration}</span><span>${jobdurationText}</span><span>sec.</span></span></p>
        <p><span data-translate="start_time">Start:</span><span><span class="itemstarttime">${starttime}</span><span>${starttimeText}</span></span></p>
        <p><span data-translate="every_day_label">Every Day:</span><span><span class="itemeveryday">${everyday}</span><span data-translate="${everydayText}"></span></span></p>
        <p>
            <span class="itemedit" title="Edit Job">&#9998;</span>
            <span class="itemdelete" title="Delete Job">&#128465;</span>
        </p>
    `;

    jobList.appendChild(jobItem);

    // Re-apply translations to new elements
    setLanguage(currentLanguage);
    // Update save button state
    updateSaveButtonState();
    // Reset form
    clearFormAndEditState();
}

function deleteJoblist() {
    websocket.send(JSON.stringify({"action":"deletejoblist"}));
}

function setJobList(data) {
    console.log(data);
    
    // Check if data is an array
    if (!Array.isArray(data)) {
        console.error("Invalid job list data received:", data);
        return;
    }
    
    // Clear existing job list
    jobList.innerHTML = "";

    data.forEach(job => {
        const jobItem = document.createElement("div");
        jobItem.classList.add("job");
        // Set translated job selection text
        let jobTypeText = "";
        switch(parseInt(job.type)) {
            case 0: jobTypeText = "timebased"; break;
            case 1: jobTypeText = "moisturesensor"; break;
            case 2: jobTypeText = "time_moisture"; break;
        }
        // Set "--" to separated text variables in case of any of 
        // jobvolume or jobduration or starttime or every day or moisture min or moisture max is empty
        let jobvolumeText = job.volume ? "" : "--";
        let jobdurationText = job.duration ? "" : "--";
        let everydayText = jobselect == 0 ? job.everyday ? "yes" : "no" : "--";
        let starttimeText = job.starttime ? new Date(job.starttime).toLocaleString() : "--";
        let moistureMinText = job.moistureMin ? "" : "--";
        let moistureMaxText = job.moistureMax ? "" : "--";

        jobItem.innerHTML = `
            <button class="activate">${job.active ? "Active" : "Activate"}</button>
            <p><span data-translate="job_name">Name:</span><span class="itemjobname">${job.name}</span></p>
            <p><span data-translate="job_type">Job:</span><span><span class="itemjobselect">${job.type}</span> - <span data-translate="${jobTypeText}"></span></span></p>
            <p><span data-translate="moisture_min">Moisture Min:</span><span><span class="itemmoisturemin">${job.moistureMin}</span><span>${moistureMinText}</span><span>%</span></span></p>
            <p><span data-translate="moisture_max">Moisture Max:</span><span><span class="itemmoisturemax">${job.moistureMax}</span><span>${moistureMaxText}</span><span>%</span></span></p>
            <p><span data-translate="plant">Plant:</span><span class="itemplantselect">${job.plant}</span></p>
            <p><span data-translate="volume">Volume:</span><span><span class="itemjobvolume">${job.volume}</span><span>${jobvolumeText}</span><span>ml.</span></span></p>
            <p><span data-translate="duration">Duration:</span><span><span class="itemjobduration">${job.duration}</span><span>${jobdurationText}</span><span>sec.</span></span></p>
            <p><span data-translate="start_time">Start:</span><span><span class="itemstarttime">${job.starttime}</span><span>${starttimeText}</span></span></p>
            <p><span data-translate="every_day_label">Every Day:</span><span><span class="itemeveryday">${job.everyday}</span><span data-translate="${everydayText}"></span></span></p>
            <p>
                <span class="itemedit" title="Edit Job">&#9998;</span>
                <span class="itemdelete" title="Delete Job">&#128465;</span>
            </p>
        `;
        jobList.appendChild(jobItem);
    });
    // Re-apply translations to new elements
    setLanguage(currentLanguage);
    // Update save button state
    updateSaveButtonState();
}

async function saveJobs() {
    const collection = jobList.children;

    for (let i = 0; i < collection.length; i++) {
        let id = i;
        let active = collection[i].querySelector('button').innerText || "Active";
        let name = collection[i].querySelector('.itemjobname').innerText || "";
        let type = parseInt(collection[i].querySelector('.itemjobselect').innerText) || 0;
        let moistureMin = parseInt(collection[i].querySelector('.itemmoisturemin').innerText) || 0;
        let moistureMax = parseInt(collection[i].querySelector('.itemmoisturemax').innerText) || 0;
        let plant = parseInt(collection[i].querySelector('.itemplantselect').innerText) || 0;
        let volume = parseInt(collection[i].querySelector('.itemjobvolume').innerText) || 0;
        let duration = parseInt(collection[i].querySelector('.itemjobduration').innerText) || 0;
        let starttime = collection[i].querySelector('.itemstarttime').innerText || "";
        let everyday = collection[i].querySelector('.itemeveryday').innerText === "true";

        active = active === "Active" ? true : false;

        console.log(JSON.stringify({"action":"addjobtolist","id":id,"active":active,"name":name,"type":type,"moistureMin":moistureMin,"moistureMax":moistureMax,"plant":plant,"volume":volume,"duration":duration,"starttime":starttime,"everyday":everyday}));

        websocket.send(JSON.stringify({"action":"addjobtolist","id":id,"active":active,"name":name,"type":type,"moistureMin":moistureMin,"moistureMax":moistureMax,"plant":plant,"volume":volume,"duration":duration,"starttime":starttime,"everyday":everyday}));
        await sleep(200);
    }

    websocket.send(JSON.stringify({"action":"savejoblist"}));
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function doJobList(event) {
    if (event.target.classList.contains("activate")) {
        const jobParentItem = event.target.parentElement;
        jobParentItem.style.backgroundColor = "#737373";
        event.target.classList.remove('activate');
        event.target.classList.add('active');
        event.target.innerHTML = "Active";
        //event.target.disabled = true;
        return;
    }
    if (event.target.classList.contains("active")) {
        const jobParentItem = event.target.parentElement;
        jobParentItem.style.backgroundColor = "initial";
        event.target.classList.remove('active');
        event.target.classList.add('activate');
        event.target.innerHTML = "Activate";
        return;
    }
    if (event.target.classList.contains("itemdelete")) {
        const jobParentItem = event.target.parentElement.parentElement;
        jobParentItem.remove();
        // If we removed the item currently being edited, cancel edit state
        const children = Array.from(jobList.children);
        if (editIndex >= children.length) clearFormAndEditState();
        // Update save button state
        updateSaveButtonState();
        return;
    }
    if (event.target.classList.contains("itemedit")) {
        const jobParentItem = event.target.parentElement.parentElement;
        const index = Array.from(jobList.children).indexOf(jobParentItem);
        startEdit(index);
        return;
    }
}

function toggleJobTriggerFields() {
    const triggerType = parseInt(document.getElementById('job-select').value);
    const timeField = document.getElementById('starttime');
    const everyDayField = document.getElementById('everyday');
    const jobvolume = document.getElementById('job-volume');
    const jobduration = document.getElementById('job-duration');
    const moistureMin = document.getElementById('moisture-min');
    const moistureMax = document.getElementById('moisture-max');

    switch(triggerType) {
        case 0: // Time only
            moistureMin.disabled = true;
            moistureMax.disabled = true;
            jobvolume.disabled = false;
            jobduration.disabled = false;
            timeField.disabled = false;
            everyDayField.disabled = false;
            break;
        case 1: // Moisture only
            moistureMin.disabled = false;
            moistureMax.disabled = false;
            jobvolume.disabled = true;
            jobduration.disabled = true;
            timeField.disabled = true;
            everyDayField.disabled = true;
            break;
        case 2: // Both
            moistureMin.disabled = false;
            moistureMax.disabled = true;
            jobvolume.disabled = false;
            jobduration.disabled = false;
            timeField.disabled = true;
            everyDayField.disabled = true;
            break;
    }
}

export { setJobList, updatePlantSelect, toggleJobTriggerFields };