import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { WebSocketServer } from 'ws';
import { validateSnapshot } from './validate.js';
import { ensureMapAsset } from './map-assets.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../');
const webRoot = path.join(root, 'web');
const httpPort = process.env.WEB_PORT || 8787; // Web interface port
const collectorPort = process.env.COLLECTOR_PORT || 3000; // Collector data port

let state = { 
  connection: 'waiting', 
  source: 'external', 
  error: null, 
  snapshot: null,
  lastUpdate: null
};

// HTTP server for web interface
const webServer = http.createServer(async (req, res) => {
  // CORS headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    res.writeHead(200);
    return res.end();
  }

  // API endpoint for state
  if (req.url === '/api/state') { 
    res.writeHead(200, { 'content-type': 'application/json' }); 
    return res.end(JSON.stringify(state)); 
  }

  // Serve static files
  const file = req.url === '/' ? 'index.html' : path.basename(decodeURIComponent(req.url.split('?')[0]));
  
  // Check if it's a map file
  if (file.endsWith('.png')) {
    const mapPath = path.join(webRoot, 'maps', file);
    try {
      const body = await readFile(mapPath);
      res.writeHead(200, { 'content-type': 'image/png' });
      return res.end(body);
    } catch {
      // Generate missing map radar from the installed CS2 DDS overview.
      const map = file.slice(0, -4);
      const generated = await ensureMapAsset(map, webRoot);
      if (generated) {
        const body = await readFile(generated);
        res.writeHead(200, { 'content-type': 'image/png', 'cache-control': 'public, max-age=31536000, immutable' });
        return res.end(body);
      }
    }
  }

  try { 
    const body = await readFile(path.join(webRoot, file)); 
    const contentType = file.endsWith('.js') ? 'text/javascript' : 
                       file.endsWith('.css') ? 'text/css' : 
                       file.endsWith('.png') ? 'image/png' :
                       'text/html';
    res.writeHead(200, { 'content-type': contentType }); 
    res.end(body); 
  } catch { 
    res.writeHead(404); 
    res.end('Not found'); 
  }
});

// WebSocket server for broadcasting to web clients
const wss = new WebSocketServer({ server: webServer, path: '/ws' });
wss.on('connection', ws => {
  console.log('[WebSocket] Client connected');
  ws.send(JSON.stringify(state));
  
  ws.on('close', () => {
    console.log('[WebSocket] Client disconnected');
  });
});

function publish() { 
  const msg = JSON.stringify(state); 
  let clientCount = 0;
  for (const ws of wss.clients) {
    if (ws.readyState === 1) {
      ws.send(msg);
      clientCount++;
    }
  }
  if (clientCount > 0) {
    // console.log(`[WebSocket] Broadcast to ${clientCount} clients`);
  }
}

function updateState(snapshot) {
  state = { 
    connection: 'connected', 
    source: 'external', 
    error: null, 
    snapshot: snapshot,
    lastUpdate: Date.now()
  };
  publish();
}

function fail(message) { 
  console.error(`[Error] ${message}`);
  state = { ...state, connection: 'error', error: message }; 
  publish(); 
}

// HTTP server for receiving data from C++ collector
const collectorServer = http.createServer((req, res) => {
  // CORS headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    res.writeHead(200);
    return res.end();
  }

  if (req.method === 'POST' && req.url === '/api/radar') {
    let body = '';
    
    req.on('data', chunk => {
      body += chunk.toString();
      // Prevent memory overflow
      if (body.length > 1e6) {
        req.destroy();
        res.writeHead(413, { 'Content-Type': 'text/plain' });
        res.end('Payload too large');
      }
    });

    req.on('end', () => {
      try {
        const snapshot = JSON.parse(body);
        
        // Validate snapshot
        const validationError = validateSnapshot(snapshot);
        if (validationError) {
          console.error('[Validation] Failed:', validationError);
          res.writeHead(400, { 'Content-Type': 'application/json' });
          return res.end(JSON.stringify({ error: validationError }));
        }

        // Update state and broadcast
        updateState(snapshot);
        
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ success: true }));
      } catch (error) {
        console.error('[Parse Error]', error.message);
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Invalid JSON' }));
      }
    });

    req.on('error', (error) => {
      console.error('[Request Error]', error);
      res.writeHead(500);
      res.end();
    });
    return;
  }

  res.writeHead(404);
  res.end('Not found');
});

// Start servers
webServer.listen(httpPort, '0.0.0.0', () => {
  console.log('=== Folk Valley Radar Backend ===');
  console.log(`Web interface: http://localhost:${httpPort}`);
  console.log(`WebSocket: ws://localhost:${httpPort}/ws`);
});

collectorServer.listen(collectorPort, '127.0.0.1', () => {
  console.log(`Collector endpoint: http://127.0.0.1:${collectorPort}/api/radar`);
  console.log('Waiting for collector data...');
  console.log('==================================');
});

// Connection timeout check
setInterval(() => {
  if (state.lastUpdate && Date.now() - state.lastUpdate > 5000) {
    if (state.connection === 'connected') {
      console.warn('[Timeout] No data received for 5 seconds');
      state.connection = 'timeout';
      state.error = 'No data from collector';
      publish();
    }
  }
}, 2000);
