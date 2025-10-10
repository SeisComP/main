/**
 * Event-based time selection functionality for FDSNWS DataSelect builder
 */

// Simple debounce utility
function debounce(fn, delay) {
  let timeout;
  return function(...args) {
    clearTimeout(timeout);
    timeout = setTimeout(() => fn.apply(this, args), delay);
  };
}

async function checkEventService() {
  try {
    const response = await fetch('../../event/1/version');
    return response.ok;
  } catch (error) {
    return false;
  }
}

// Event functionality
let currentOriginTime = null;
let currentController = null; // Track current request for cancellation

async function fetchEventOriginTime(eventId, signal = null) {
  const params = new URLSearchParams({
    eventid: eventId,
    format: 'text',
    nodata: '404'
  });
  
  const response = await fetch(`../../event/1/query?${params}`, { signal });
  
  if (!response.ok) {
    throw new Error(response.status === 404 ? 'Event not found' : 'Failed to fetch event');
  }

  const text = await response.text();
  const lines = text.trim().split('\n');
  if (lines.length < 2) throw new Error('Invalid response format');

  const originTimeStr = lines[1].split('|')[1]?.trim();
  if (!originTimeStr) throw new Error('No origin time found');

  const originTime = originTimeStr.endsWith('Z') 
    ? new Date(originTimeStr)
    : new Date(originTimeStr + 'Z');

  if (isNaN(originTime.getTime())) throw new Error('Invalid origin time');
  
  return originTime;
}

function updateTimeFields() {
  if (!currentOriginTime) return;

  const before = parseFloat(document.getElementById('before-time').value) || 0;
  const after = parseFloat(document.getElementById('after-time').value) || 0;

  const startTime = new Date(currentOriginTime.getTime() - before * 60000);
  const endTime = new Date(currentOriginTime.getTime() + after * 60000);

  document.querySelector('input[name="starttime"]').value = startTime.toISOString().replace(/\.\d{3}Z$/, '');
  document.querySelector('input[name="endtime"]').value = endTime.toISOString().replace(/\.\d{3}Z$/, '');

  // Trigger URL update via existing form event system
  const form = document.getElementById('query-form');
  form.dispatchEvent(new Event('input', { bubbles: true }));
}

const debouncedEventLookup = debounce(async function(eventId) {
  const statusDiv = document.getElementById('event-status');
  
  // Cancel previous request if still running
  if (currentController) {
    currentController.abort();
  }
  
  if (!eventId.trim()) {
    currentController = null;
    currentOriginTime = null;
    statusDiv.textContent = '';
    statusDiv.className = '';
    return;
  }

  if (eventId.length < 3) {
    statusDiv.textContent = 'Enter complete Event ID...';
    statusDiv.className = 'status loading';
    return;
  }

  // Create new controller - this becomes the "current" request
  currentController = new AbortController();
  const signal = currentController.signal;

  statusDiv.textContent = 'Fetching event...';
  statusDiv.className = 'status loading';

  try {
    currentOriginTime = await fetchEventOriginTime(eventId, signal);
    
    // Only update UI if this request wasn't cancelled
    if (!signal.aborted) {
      statusDiv.textContent = `Origin Time: ${currentOriginTime.toISOString().replace(/\.\d{3}Z$/, '')}`;
      statusDiv.className = 'status success';
      updateTimeFields();
    }
  } catch (error) {
    // Don't show error if request was just cancelled
    if (!signal.aborted && error.name !== 'AbortError') {
      statusDiv.textContent = error.message;
      statusDiv.className = 'status error';
      currentOriginTime = null;
    }
  }
}, 800);

function clearEventData() {
  // Cancel any pending request
  if (currentController) {
    currentController.abort();
    currentController = null;
  }
  
  document.getElementById('event-id').value = '';
  document.getElementById('before-time').value = '';
  document.getElementById('after-time').value = '';
  document.querySelector('input[name="starttime"]').value = '';
  document.querySelector('input[name="endtime"]').value = '';
  document.getElementById('event-status').textContent = '';
  document.getElementById('event-status').className = '';
  currentOriginTime = null;
  
  const form = document.getElementById('query-form');
  form.dispatchEvent(new Event('input', { bubbles: true }));
}

async function initializeEventTimeSelection() {
  // Check event service on load and configure UI
  const serviceAvailable = await checkEventService();
  const eventSection = document.querySelector('.event-section');
  const eventLegend = eventSection.querySelector('legend');
  
  if (serviceAvailable) {
    eventLegend.textContent = 'Event-based Time Selection ✓';
    eventLegend.style.color = '#28a745';
    
    // Set up event listeners
    document.getElementById('event-id').addEventListener('input', (e) => {
      debouncedEventLookup(e.target.value);
    });
    
    document.getElementById('before-time').addEventListener('input', updateTimeFields);
    document.getElementById('after-time').addEventListener('input', updateTimeFields);
    document.getElementById('clear-event').addEventListener('click', clearEventData);
  } else {
    eventLegend.textContent = 'Event-based Time Selection (service unavailable)';
    eventLegend.style.color = '#dc3545';
    eventSection.disabled = true;
  }
}