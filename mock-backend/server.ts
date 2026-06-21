import express, { Request, Response } from 'express';
import cors from 'cors';
import path from 'path';

const app = express();
const PORT = 3000;

app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Interface for Spa status
interface SpaStatus {
  online: boolean;
  act_temp: number;
  set_temp: number;
  power: boolean;
  filter: boolean;
  heater: boolean;
  bubble: boolean;
  time: string;
  free_heap: number;
  min_free_heap: number;
  uptime: number;
  wifi_rssi: number;
}

// Interface for ScheduledEvent
interface ScheduledEvent {
  id: number;
  enabled: boolean;
  recurring: boolean;
  dow: number; // day of week bitmask (1 << day)
  year: number;
  month: number;
  day: number;
  hour: number;
  minute: number;
  setPower: boolean;
  powerValue: boolean;
  setFilter: boolean;
  filterValue: boolean;
  setHeater: boolean;
  heaterValue: boolean;
  setBubble: boolean;
  bubbleValue: boolean;
  setTemp: boolean;
  tempValue: number;
}

interface AuditEvent {
  timestamp: number;
  source: string;
  feature: string;
  state: boolean;
}

let mockAuditLogs: AuditEvent[] = [
  { timestamp: Math.floor(Date.now() / 1000) - 7200, source: 'Schedule #1', feature: 'Filter', state: true },
  { timestamp: Math.floor(Date.now() / 1000) - 7200, source: 'Schedule #1', feature: 'Heater', state: true },
  { timestamp: Math.floor(Date.now() / 1000) - 3600, source: 'Web UI', feature: 'Bubbles', state: true },
  { timestamp: Math.floor(Date.now() / 1000) - 1800, source: 'Web UI', feature: 'Bubbles', state: false },
];
let retentionDays = 7;

let startTime = Date.now();

// In-memory mock state
let state: SpaStatus = {
  online: true,
  act_temp: 34.5,
  set_temp: 38,
  power: false,
  filter: false,
  heater: false,
  bubble: false,
  time: new Date().toISOString().replace('T', ' ').substring(0, 19),
  free_heap: 198000,
  min_free_heap: 172000,
  uptime: 0,
  wifi_rssi: -58,
};

let scheduleList: ScheduledEvent[] = [
  {
    id: 1,
    enabled: true,
    recurring: true,
    dow: 62, // Mon-Fri (2 + 4 + 8 + 16 + 32)
    year: 0,
    month: 0,
    day: 0,
    hour: 17,
    minute: 30,
    setPower: true,
    powerValue: true,
    setFilter: true,
    filterValue: true,
    setHeater: true,
    heaterValue: true,
    setBubble: false,
    bubbleValue: false,
    setTemp: true,
    tempValue: 39,
  },
  {
    id: 2,
    enabled: false,
    recurring: false,
    dow: 0,
    year: 2026,
    month: 6,
    day: 21,
    hour: 8,
    minute: 0,
    setPower: true,
    powerValue: false,
    setFilter: false,
    filterValue: false,
    setHeater: false,
    heaterValue: false,
    setBubble: false,
    bubbleValue: false,
    setTemp: false,
    tempValue: 38,
  }
];

let nextEventId = 3;

// Background task: Update simulated time and water temperature
setInterval(() => {
  const now = new Date();
  state.time = now.toISOString().replace('T', ' ').substring(0, 19);
  state.uptime = Math.floor((Date.now() - startTime) / 1000);

  // If power is ON and heater is ON, actual temp approaches target temp
  if (state.power && state.heater) {
    if (state.act_temp < state.set_temp) {
      // Rise temperature by 0.1 degree
      state.act_temp = Math.min(state.set_temp, Math.round((state.act_temp + 0.1) * 10) / 10);
      console.log(`[Sim] Heating... Actual Temp: ${state.act_temp}°C`);
    } else {
      // Target reached, heater turns off dynamically or cycles
      console.log(`[Sim] Target temperature reached. Cycling.`);
    }
  } else {
    // If heater or power is off, cool down slowly to ambient (e.g., 20°C)
    const ambient = 20.0;
    if (state.act_temp > ambient) {
      state.act_temp = Math.max(ambient, Math.round((state.act_temp - 0.05) * 100) / 100);
    }
  }
}, 3000);

// Serve the index.html at root
app.get('/', (req: Request, res: Response) => {
  res.sendFile(path.join(__dirname, '../main/index.html'));
});
app.get('/index.html', (req: Request, res: Response) => {
  res.sendFile(path.join(__dirname, '../main/index.html'));
});

app.get('/favicon.ico', (req: Request, res: Response) => {
  res.sendFile(path.join(__dirname, '../main/favicon.png'));
});

// Serve the config.html at /config
app.get('/config', (req: Request, res: Response) => {
  res.sendFile(path.join(__dirname, '../main/config.html'));
});
app.get('/config.html', (req: Request, res: Response) => {
  res.sendFile(path.join(__dirname, '../main/config.html'));
});

app.post('/config', (req: Request, res: Response) => {
  const { ssid, password } = req.body;
  console.log(`[WiFi Config API] Received SSID: ${ssid}, Password: ${password}`);
  res.send('Credentials saved. Restarting...');
});

// APIs
app.get('/api/status', (req: Request, res: Response) => {
  res.json(state);
});

app.post('/api/control', (req: Request, res: Response) => {
  const { cmd, value } = req.body;
  console.log(`[Control API] cmd: ${cmd}, value: ${value}`);

  if (cmd === 'power') {
    if (state.power !== value) {
      mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Power', state: value });
      state.power = value;
      if (!state.power) {
        if (state.filter) mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Filter', state: false });
        if (state.heater) mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Heater', state: false });
        if (state.bubble) mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Bubbles', state: false });
        state.filter = false;
        state.heater = false;
        state.bubble = false;
      }
    }
  } else if (state.power) {
    if (cmd === 'filter') {
      if (state.filter !== value) {
        mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Filter', state: value });
        state.filter = value;
        if (!state.filter) {
          if (state.heater) mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Heater', state: false });
          state.heater = false;
        }
      }
    } else if (cmd === 'heater') {
      if (state.heater !== value) {
        if (value && !state.filter) {
          mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Filter', state: true });
          state.filter = true;
        }
        mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Heater', state: value });
        state.heater = value;
      }
    } else if (cmd === 'bubble') {
      if (state.bubble !== value) {
        mockAuditLogs.push({ timestamp: Math.floor(Date.now() / 1000), source: 'Web UI', feature: 'Bubbles', state: value });
        state.bubble = value;
      }
    } else if (cmd === 'temp') {
      state.set_temp = value;
    }
  }

  res.json({ status: 'ok' });
});

app.get('/api/schedule', (req: Request, res: Response) => {
  res.json(scheduleList);
});

app.post('/api/schedule/add', (req: Request, res: Response) => {
  const ev = req.body;
  const newEvent: ScheduledEvent = {
    id: nextEventId++,
    enabled: true,
    recurring: ev.recurring,
    dow: ev.dow || 0,
    year: ev.year || 0,
    month: ev.month || 0,
    day: ev.day || 0,
    hour: ev.hour || 0,
    minute: ev.minute || 0,
    setPower: ev.setPower || false,
    powerValue: ev.powerValue || false,
    setFilter: ev.setFilter || false,
    filterValue: ev.filterValue || false,
    setHeater: ev.setHeater || false,
    heaterValue: ev.heaterValue || false,
    setBubble: ev.setBubble || false,
    bubbleValue: ev.bubbleValue || false,
    setTemp: ev.setTemp || false,
    tempValue: ev.tempValue || 38,
  };

  scheduleList.push(newEvent);
  console.log(`[Schedule API] Added event:`, newEvent);
  res.json({ status: 'ok' });
});

app.post('/api/schedule/update', (req: Request, res: Response) => {
  const ev = req.body;
  const id = Number(ev.id);
  console.log(`[Schedule API] Natively updating event ID: ${id}`);
  
  const index = scheduleList.findIndex(e => e.id === id);
  if (index !== -1) {
    scheduleList[index] = {
      id: id,
      enabled: ev.enabled !== undefined ? ev.enabled : scheduleList[index].enabled,
      recurring: ev.recurring,
      dow: ev.dow || 0,
      year: ev.year || 0,
      month: ev.month || 0,
      day: ev.day || 0,
      hour: ev.hour || 0,
      minute: ev.minute || 0,
      setPower: ev.setPower || false,
      powerValue: ev.powerValue || false,
      setFilter: ev.setFilter || false,
      filterValue: ev.filterValue || false,
      setHeater: ev.setHeater || false,
      heaterValue: ev.heaterValue || false,
      setBubble: ev.setBubble || false,
      bubbleValue: ev.bubbleValue || false,
      setTemp: ev.setTemp || false,
      tempValue: ev.tempValue || 38,
    };
    console.log(`[Schedule API] Natively updated event:`, scheduleList[index]);
  } else {
    console.log(`[Schedule API] Failed to update event ID ${id}: Not found`);
  }
  res.json({ status: 'ok' });
});

app.post('/api/schedule/delete', (req: Request, res: Response) => {
  const { id } = req.body;
  console.log(`[Schedule API] Deleting event ID: ${id}`);
  scheduleList = scheduleList.filter(ev => ev.id !== id);
  res.json({ status: 'ok' });
});

app.post('/api/schedule/toggle', (req: Request, res: Response) => {
  const { id, enabled } = req.body;
  console.log(`[Schedule API] Toggling event ID: ${id} to ${enabled}`);
  const ev = scheduleList.find(e => e.id === id);
  if (ev) {
    ev.enabled = enabled;
  }
  res.json({ status: 'ok' });
});

app.post('/api/admin/time', (req: Request, res: Response) => {
  const { timestamp } = req.body;
  const syncDate = new Date(timestamp * 1000);
  console.log(`[Admin API] Syncing time to: ${syncDate.toISOString()}`);
  res.json({ status: 'ok' });
});

app.post('/api/admin/reboot', (req: Request, res: Response) => {
  console.log('[Admin API] Reboot requested... Resetting uptime.');
  startTime = Date.now();
  res.json({ status: 'ok' });
});

app.post('/api/admin/reset/wifi', (req: Request, res: Response) => {
  console.log('[Admin API] Reset WiFi requested... Redirecting to config page.');
  res.json({ status: 'ok' });
});

app.post('/api/admin/reset/schedule', (req: Request, res: Response) => {
  console.log('[Admin API] Reset schedule requested... Clearing events list.');
  scheduleList = [];
  res.json({ status: 'ok' });
});

app.post('/api/admin/reset/all', (req: Request, res: Response) => {
  console.log('[Admin API] Factory Reset requested... Clearing everything.');
  scheduleList = [];
  startTime = Date.now();
  res.json({ status: 'ok' });
});

app.post('/api/admin/ota', express.raw({ type: 'application/octet-stream', limit: '10mb' }), (req: Request, res: Response) => {
  console.log(`[Admin API] OTA firmware upload started...`);
  const data = req.body as Buffer;
  console.log(`[Admin API] OTA upload completed. Received ${data ? data.length : 0} bytes.`);
  res.json({ status: 'ok' });
});

app.get('/api/admin/audit', (req: Request, res: Response) => {
  console.log('[Admin API] Fetching mock audit trail events');
  const cutoff = Math.floor(Date.now() / 1000) - (retentionDays * 24 * 3600);
  mockAuditLogs = mockAuditLogs.filter(e => e.timestamp >= cutoff);
  res.json(mockAuditLogs);
});

app.get('/api/admin/audit/config', (req: Request, res: Response) => {
  console.log('[Admin API] Fetching audit trail config');
  res.json({ retentionDays });
});

app.post('/api/admin/audit/config', (req: Request, res: Response) => {
  const { retentionDays: days } = req.body;
  console.log(`[Admin API] Setting audit trail retention to ${days} days`);
  if (days >= 1 && days <= 7) {
    retentionDays = days;
  }
  res.json({ status: 'ok' });
});

app.post('/api/admin/audit/clear', (req: Request, res: Response) => {
  console.log('[Admin API] Clearing mock audit trail events');
  mockAuditLogs = [];
  res.json({ status: 'ok' });
});

// Run server
app.listen(PORT, () => {
  console.log(`==================================================`);
  console.log(`ESP32 PureSpa Mock Backend running!`);
  console.log(`Access the UI at: http://localhost:${PORT}`);
  console.log(`==================================================`);
});
