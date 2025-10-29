import { setLanguage, currentLanguage } from "./language.js";

function showWiFiMessage(message, type) {
    const messageDiv = document.getElementById('wifi-message');
    messageDiv.textContent = message;
    messageDiv.className = 'message ' + type;
    messageDiv.style.display = 'block';
    
    setTimeout(() => {
        messageDiv.style.display = 'none';
    }, 5000);
}

function showWiFiLoading(show) {
    document.getElementById('wifi-loading').style.display = show ? 'block' : 'none';
    document.getElementById('wifi-form-container').style.display = show ? 'none' : 'block';
}

async function scanNetworks() {
    showWiFiMessage('Scanning for networks...', 'info');
    
    try {
        // Start the scan
        const startResponse = await fetch('/scan');
        
        if (startResponse.status === 409) {
            showWiFiMessage('Scan already in progress, waiting...', 'info');
        }
        
        // Poll for results
        let scanning = true;
        let attempts = 0;
        const maxAttempts = 30; // 15 seconds max
        
        while (scanning && attempts < maxAttempts) {
            await new Promise(resolve => setTimeout(resolve, 500)); // Wait 500ms
            
            const resultsResponse = await fetch('/scan-results');
            const data = await resultsResponse.json();
            
            if (data.status === 'complete') {
                scanning = false;
                
                const select = document.getElementById('wifi-ssid');
                select.innerHTML = '<option value="">Select a network</option>';
                
                // Sort networks by signal strength
                data.networks.sort((a, b) => b.rssi - a.rssi);
                
                data.networks.forEach(network => {
                    const option = document.createElement('option');
                    option.value = network.ssid;
                    
                    // Add signal strength indicator
                    let signalBars = '';            
                    if (network.rssi > -50) signalBars = '||||';
                    else if (network.rssi > -60) signalBars = '|||_';
                    else if (network.rssi > -70) signalBars = '||__';
                    else signalBars = '|___';
                    
                    option.textContent = `${signalBars} ${network.ssid} (${network.rssi} dBm)`;
                    select.appendChild(option);
                });
                
                showWiFiMessage(`Found ${data.networks.length} network(s)`, 'success');
            } else if (data.error) {
                scanning = false;
                showWiFiMessage('Scan failed: ' + data.error, 'error');
            }
            
            attempts++;
        }
        
        if (attempts >= maxAttempts) {
            showWiFiMessage('Scan timeout', 'error');
        }
    } catch (error) {
        showWiFiMessage('Failed to scan networks', 'error');
        console.error('Scan error:', error);
    }
}

async function connectToWiFi() {
    const ssidSelect = document.getElementById('wifi-ssid').value;
    const ssidManual = document.getElementById('wifi-ssid-manual').value;
    const password = document.getElementById('wifi-password').value;
    
    const ssid = ssidManual || ssidSelect;
    
    if (!ssid) {
        showWiFiMessage('Please select or enter a WiFi network', 'error');
        return;
    }
    
    if (!password) {
        showWiFiMessage('Please enter WiFi password', 'error');
        return;
    }
    
    showWiFiLoading(true);
    
    try {
        const response = await fetch('/connect', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`
        });
        
        const result = await response.text();
        
        if (response.ok) {
            showWiFiLoading(false);
            showWiFiMessage('Connected successfully! Device is restarting...', 'success');
            
            // Close overlay after delay
            setTimeout(() => {
                toggleOverlay('wifi');
                // Clear form
                document.getElementById('wifi-ssid').value = '';
                document.getElementById('wifi-ssid-manual').value = '';
                document.getElementById('wifi-password').value = '';
            }, 3000);
        } else {
            showWiFiLoading(false);
            showWiFiMessage('Failed to connect: ' + result, 'error');
        }
    } catch (error) {
        showWiFiLoading(false);
        showWiFiMessage('Connection error. Please try again.', 'error');
        console.error('Connection error:', error);
    }
}

function resetWiFi() {
    if (confirm('This will reset WiFi settings and restart the device. Continue?')) {
        showWiFiLoading(true);
        
        fetch('/reset-wifi')
            .then(response => response.text())
            .then(data => {
                showWiFiLoading(false);
                showWiFiMessage(data, 'success');
                
                setTimeout(() => {
                    toggleOverlay('wifi');
                }, 2000);
            })
            .catch(error => {
                showWiFiLoading(false);
                showWiFiMessage('Error resetting WiFi', 'error');
                console.error('Reset error:', error);
            });
    }
}

export { scanNetworks, connectToWiFi, resetWiFi };