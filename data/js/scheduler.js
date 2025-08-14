const jobNameInput = document.getElementById("job-name");
const jobselectInput = document.getElementById("job-select");
const plantselectInput = document.getElementById("plant-select");
const jobDurationInput = document.getElementById("job-duration");
const starttimeInput = document.getElementById("starttime");
const everydayInput = document.getElementById("everyday");
const addJobButton = document.getElementById("add-job");
const saveJobsButton = document.getElementById("save-jobs");
const jobList = document.getElementById("job-list");

addJobButton.addEventListener("click", addJob);
saveJobsButton.addEventListener("click", saveJobs);
jobList.addEventListener("click", doJobList);

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

    const selectedDate = new Date(starttime);
    const currentDate = new Date();

    if (starttime === "") {
        alert("Please select an upcoming date for the start time.");
        return; // Don't add task if deadline is not in the future
    }

    if ((selectedDate <= currentDate)) {
        alert("Please select an upcoming date in the future for the start time.");
        return; // Don't add task if deadline is not in the future
    }

    const jobItem = document.createElement("div");
    jobItem.classList.add("job");
    jobItem.innerHTML = `
    <button class="activate">Activate</button>
    <p>Name:<span class="itemjobname">${jobname}</span></p>
    <p>Job:<span class="itemjobselect">${jobselect}</span></p>
    <p>Plant:<span class="itemplantselect">${plantselect}</span></p>
    <p>Duration:<span class="itemjobduration">${jobduration}</span><span>sec.</span></p>
    <p>Start:<span class="itemstarttime">${starttime}</span></p>
    <p>Every Day:<span class="itemeveryday">${everyday}</span></p>
    <p>
        <span class="itemedit" title="Edit Job">&#9998;</span>&nbsp;
        <span class="itemdelete" title="Delete Job">&#128465;</span>
    </p>
  `;

    jobList.appendChild(jobItem);

    jobNameInput.value = "";
    jobselectInput.value = "watering";
    plantselectInput.value = "all-plants";
    jobDurationInput.value = "";
    starttimeInput.value = "";
    everydayInput.checked = false;
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
            <p>Name:<span class="itemjobname">${job.name}</span></p>
            <p>Job:<span class="itemjobselect">${job.job}</span></p>
            <p>Plant:<span class="itemplantselect">${job.plant}</span></p>
            <p>Duration:<span class="itemjobduration">${job.duration}</span><span>sec.</span></p>
            <p>Start:<span class="itemstarttime">${job.starttime}</span></p>
            <p>Every Day:<span class="itemeveryday">${job.everyday}</span></p>
            <p>
                <span class="itemedit" title="Edit Job">&#9998;</span>&nbsp;
                <span class="itemdelete" title="Delete Job">&#128465;</span>
            </p>
        `;
        jobList.appendChild(jobItem);
    });
}

async function saveJobs() {
    const collection = jobList.children;
    //let tempjobs = '';
    //let jobJSON = "";

    for (let i = 0; i < collection.length; i++) {
        let id = i;
        let active = collection[i].children[0].childNodes[0].data;
        let name = collection[i].children[1].children[0].childNodes[0].data;
        let job = collection[i].children[2].children[0].childNodes[0].data;
        let plant = collection[i].children[3].children[0].childNodes[0].data;
        let duration = parseInt(collection[i].children[4].children[0].childNodes[0].data);
        let starttime = collection[i].children[5].children[0].childNodes[0].data;
        let everyday = Boolean(collection[i].children[6].children[0].childNodes[0].data);

        active = active === "Active" ? true : false;

        console.log(JSON.stringify({"action":"addjobtolist","id":id,"active":active,"name":name,"job":job,"plant":plant,"duration":duration,"starttime":starttime,"everyday":everyday}));
        
        websocket.send(JSON.stringify({"action":"addjobtolist","id":id,"active":active,"name":name,"job":job,"plant":plant,"duration":duration,"starttime":starttime,"everyday":everyday}));
        await sleep(2000);
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
        return;
    }
}
