import { setLanguage } from "./index.js";
import { currentLanguage } from "./index.js";
import { websocket } from "./index.js";

const jobNameInput = document.getElementById("job-name");
const jobselectInput = document.getElementById("job-select");
const plantselectInput = document.getElementById("plant-select");
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

// Edit index (-1 = not editing)
let editIndex = -1;

// Get job list to check if there are jobs to enable/disable save button
const jobListCheck = document.getElementById("job-list");

// Initial button state
updateSaveButtonState();

// Function to update save button state
function updateSaveButtonState() {
    const hasJobs = jobListCheck.children.length > 0;
    saveJobsButton.disabled = !hasJobs;
    saveJobsButton.style.opacity = hasJobs ? "1" : "0.5";
    saveJobsButton.style.cursor = hasJobs ? "pointer" : "not-allowed";
}

function clearFormAndEditState() {
    editIndex = -1;
    jobNameInput.value = "";
    jobselectInput.value = "watering";
    plantselectInput.value = "all-plants";
    jobDurationInput.value = "";
    starttimeInput.value = "";
    everydayInput.checked = false;
    addJobButton.value = "Add Job";
}

function populateFormFromJobItem(jobItem) {
    const name = jobItem.querySelector('.itemjobname').innerText || "";
    const job = jobItem.querySelector('.itemjobselect').innerText || "watering";
    const plant = jobItem.querySelector('.itemplantselect').innerText || "plant-1";
    const durationText = jobItem.querySelector('.itemjobduration').innerText || "0";
    const starttime = jobItem.querySelector('.itemstarttime').innerText || "";
    const everydayText = jobItem.querySelector('.itemeveryday').innerText || "false";

    // Add activate button state check
    const activateBtn = jobItem.querySelector('button');
    const isActive = activateBtn.classList.contains('active');
    
    // Store the active state in a data attribute on the form
    addJobButton.dataset.previousState = isActive ? 'active' : 'inactive';

    jobNameInput.value = name;
    jobselectInput.value = job;
    plantselectInput.value = plant;
    jobDurationInput.value = parseInt(durationText) || "";
    starttimeInput.value = starttime;
    everydayInput.checked = (everydayText === "true" || everydayText === "True");
}

function updatePlantSelect(plantCount) {
    const plantSelect = document.getElementById('plant-select');
    plantSelect.innerHTML = `
        <option value="all-plants" data-translate="all_plants">All Plants</option>
        ${Array.from({length: plantCount}, (_, i) => `
            <option value="plant-${i+1}"><span data-translate="plant_">Plant</span> ${i+1}</option>
        `).join('')}
    `;
}

function startEdit(index) {
    const children = Array.from(jobList.children);
    if (index < 0 || index >= children.length) return;

    const jobItem = children[index];
    populateFormFromJobItem(jobItem);
    
    editIndex = index;
    addJobButton.value = "Save Changes";
    
    // scroll overlay to top for visibility (optional)
    const overlay = document.getElementById('overlay-calendar');
    if (overlay) overlay.scrollTop = 0;
}

function addJob() {
    const jobname = jobNameInput.value;
    const jobselect = jobselectInput.value;
    const plantselect = plantselectInput.value;
    const jobduration = jobDurationInput.value;
    const starttime = starttimeInput.value;
    const everyday = everydayInput.checked;
    let deadline = "";

    if (jobname.trim() === "") {
        alert("Job name cannot be empty.");
        return; // Don't add task if task or deadline is empty
    }

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
            jobItem.querySelector('.itemplantselect').innerText = plantselect;
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
    jobItem.innerHTML = `
    <button class="activate">Activate</button>
    <p><span data-translate="job_name">Name:</span><span class="itemjobname">${jobname}</span></p>
    <p><span data-translate="job_type">Job:</span><span class="itemjobselect">${jobselect}</span></p>
    <p><span data-translate="plant">Plant:</span><span class="itemplantselect">${plantselect}</span></p>
    <p><span data-translate="duration">Duration:</span><span class="itemjobduration">${jobduration}</span><span>sec.</span></p>
    <p><span data-translate="start_time">Start:</span><span class="itemstarttime">${starttime}</span></p>
    <p><span data-translate="every_day_label">Every Day:</span><span class="itemeveryday">${everyday}</span></p>
    <p>
        <span class="itemedit" title="Edit Job">&#9998;</span>&nbsp;
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
        jobItem.innerHTML = `
            <button class="activate">${job.active ? "Active" : "Activate"}</button>
            <p><span data-translate="job_name">Name:</span><span class="itemjobname">${job.name}</span></p>
            <p><span data-translate="job_type">Job:</span><span class="itemjobselect">${job.job}</span></p>
            <p><span data-translate="plant">Plant:</span><span class="itemplantselect">${job.plant}</span></p>
            <p><span data-translate="duration">Duration:</span><span class="itemjobduration">${job.duration}</span><span>sec.</span></p>
            <p><span data-translate="start_time">Start:</span><span class="itemstarttime">${job.starttime}</span></p>
            <p><span data-translate="every_day_label">Every Day:</span><span class="itemeveryday">${job.everyday}</span></p>
            <p>
                <span class="itemedit" title="Edit Job">&#9998;</span>&nbsp;
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
        let job = collection[i].querySelector('.itemjobselect').innerText || "";
        let plant = collection[i].querySelector('.itemplantselect').innerText || "";
        let duration = parseInt(collection[i].querySelector('.itemjobduration').innerText) || 0;
        let starttime = collection[i].querySelector('.itemstarttime').innerText || "";
        let everyday = collection[i].querySelector('.itemeveryday').innerText === "true";

        active = active === "Active" ? true : false;

        console.log(JSON.stringify({"action":"addjobtolist","id":id,"active":active,"name":name,"job":job,"plant":plant,"duration":duration,"starttime":starttime,"everyday":everyday}));
        
        websocket.send(JSON.stringify({"action":"addjobtolist","id":id,"active":active,"name":name,"job":job,"plant":plant,"duration":duration,"starttime":starttime,"everyday":everyday}));
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

export { setJobList, updatePlantSelect };