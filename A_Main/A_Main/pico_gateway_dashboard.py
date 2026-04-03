"""
Pico W MicroPython Gateway + Dashboard

Features:
- UDP listener on port 5005
- Parses link-state packets in String and JSON formats
- Sequence dedupe with last_seq[node_id]
- Node expiry after 15s
- HTTP:
  GET /      -> Pinnable Physics Graph with Clean Routing Table
  GET /api/nodes -> JSON API
"""

import network
import socket
import select
import ujson
import utime


# ---------------------------
# Config
# ---------------------------
WIFI_SSID = "Jw"
WIFI_PASSWORD = "junwei123123"

UDP_PORT = 5005
HTTP_PORT = 80
NODE_EXPIRE_MS = 15000
SELECT_TIMEOUT_S = 0.1


# ---------------------------
# State
# ---------------------------
nodes = {}
last_seq = {}


# ---------------------------
# Dashboard HTML
# ---------------------------
HTML_PAGE = """<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Mesh Gateway</title>
  <style>
    body { margin: 0; background: #0a0f1c; color: #e5e7eb; font-family: "IBM Plex Sans", "Segoe UI", sans-serif; display: flex; height: 100vh; overflow: hidden; }
    
    /* Sidebar */
    .sidebar { width: 360px; background: #111827; border-right: 1px solid #1f2937; display: flex; flex-direction: column; z-index: 10; box-shadow: 2px 0 10px rgba(0,0,0,0.5); }
    .header { padding: 18px 20px; border-bottom: 1px solid #1f2937; }
    h2 { margin: 0 0 10px 0; font-size: 18px; color: #7dd3fc; letter-spacing: 0.6px; font-weight: 700; }
    .status { display: inline-block; font-size: 11px; color: #9ae6b4; background: rgba(15, 23, 42, 0.8); padding: 5px 10px; border-radius: 4px; font-weight: 600; border: 1px solid #334155; font-family: "IBM Plex Mono", "Consolas", monospace; }
    
    .panel { padding: 20px; flex: 1; overflow-y: auto; }
    
    /* Custom Scrollbar for Sidebar */
    .panel::-webkit-scrollbar { width: 6px; }
    .panel::-webkit-scrollbar-track { background: transparent; }
    .panel::-webkit-scrollbar-thumb { background: #334155; border-radius: 3px; }
    
    .section-title { font-size: 11px; text-transform: uppercase; color: #93a3b8; font-weight: 700; letter-spacing: 1.2px; margin-bottom: 12px; display: flex; align-items: center; gap: 8px; font-family: "IBM Plex Mono", "Consolas", monospace; }
    .section-title::before { content: ''; width: 8px; height: 8px; background: #22d3ee; border-radius: 0; box-shadow: 0 0 0 1px #155e75 inset; }
    
    .form-group { margin-bottom: 15px; }
    label { display: block; font-size: 13px; color: #cbd5e1; margin-bottom: 6px; }
    select { width: 100%; background: #0f172a; color: #f8fafc; border: 1px solid #334155; padding: 10px; border-radius: 4px; font-size: 13px; outline: none; font-family: "IBM Plex Mono", "Consolas", monospace; }
    select:focus { border-color: #3b82f6; }
    
    button { width: 100%; padding: 11px; border-radius: 4px; font-size: 13px; font-weight: 700; border: none; cursor: pointer; margin-bottom: 8px; font-family: "IBM Plex Sans", "Segoe UI", sans-serif; letter-spacing: 0.3px; }
    .btn-primary { background: #3b82f6; color: white; }
    .btn-primary:hover { background: #2563eb; }
    .btn-secondary { background: #1f2937; color: #94a3b8; border: 1px solid #334155; }
    .btn-secondary:hover { background: #334155; color: white; }
    .proto-filter { display: grid; grid-template-columns: repeat(2, 1fr); gap: 6px; margin: 8px 0 12px; }
    .proto-filter label { display: flex; align-items: center; gap: 6px; margin: 0; padding: 6px 8px; border: 1px solid #334155; border-radius: 4px; background: #0b1324; font-size: 11px; font-family: "IBM Plex Mono", "Consolas", monospace; color: #cbd5e1; }
    .proto-filter input { accent-color: #22d3ee; }
    
    #route-result { margin-top: 14px; padding: 12px; border-radius: 4px; font-size: 12px; display: none; line-height: 1.45; margin-bottom: 18px; }
    .res-success { background: rgba(16, 185, 129, 0.1); border: 1px solid #10b981; color: #e2e8f0; }
    .res-error { background: rgba(239, 68, 68, 0.1); border: 1px solid #ef4444; color: #fca5a5; }
    .route-title { color: #6ee7b7; font-weight: 700; margin-bottom: 8px; letter-spacing: 0.3px; }
    .route-path { font-family: "IBM Plex Mono", "Consolas", monospace; color: #7dd3fc; margin: 6px 0 10px; font-size: 12px; }
    .kpi-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px; margin: 10px 0; }
    .kpi { background: rgba(2, 6, 23, 0.5); border: 1px solid #334155; border-radius: 4px; padding: 6px; text-align: center; }
    .kpi-label { font-size: 10px; color: #94a3b8; text-transform: uppercase; letter-spacing: 0.6px; font-family: "IBM Plex Mono", "Consolas", monospace; }
    .kpi-value { font-size: 13px; color: #f8fafc; font-weight: 700; font-family: "IBM Plex Mono", "Consolas", monospace; }
    .hop-table { width: 100%; border-collapse: collapse; font-size: 11px; margin-top: 8px; background: rgba(2, 6, 23, 0.5); border: 1px solid #334155; }
    .hop-table th { text-align: left; padding: 7px 8px; color: #93a3b8; border-bottom: 1px solid #334155; font-family: "IBM Plex Mono", "Consolas", monospace; font-size: 10px; text-transform: uppercase; }
    .hop-table td { padding: 7px 8px; border-bottom: 1px solid #1f2937; font-family: "IBM Plex Mono", "Consolas", monospace; color: #e2e8f0; }
    .hop-table tr:last-child td { border-bottom: none; }
    .hop-inferred { color: #fbbf24; font-weight: 700; }
    .subtle-note { margin-top: 8px; color: #94a3b8; font-size: 10px; font-family: "IBM Plex Mono", "Consolas", monospace; }
    
    /* Routing Table Styles */
    .rt-table { width: 100%; border-collapse: collapse; font-size: 11px; background: rgba(2, 6, 23, 0.6); border-radius: 4px; overflow: hidden; border: 1px solid #334155;}
    .rt-table th { text-align: left; padding: 8px 10px; color: #94a3b8; border-bottom: 1px solid #334155; font-weight: 600; text-transform: uppercase; font-size: 10px; letter-spacing: 0.6px; background: rgba(148, 163, 184, 0.06); font-family: "IBM Plex Mono", "Consolas", monospace;}
    .rt-table td { padding: 8px 10px; border-bottom: 1px solid #1f2937; color: #e2e8f0; }
    .rt-table tr:last-child td { border-bottom: none; }
    .rt-node { font-family: "IBM Plex Mono", "Consolas", monospace; font-weight: 700; color: #7dd3fc; font-size: 12px; }
    .rt-cost { font-family: "IBM Plex Mono", "Consolas", monospace; color: #86efac; font-size: 12px;}
    
    /* Main Canvas Area */
    .main { flex: 1; position: relative; }
    canvas { display: block; width: 100%; height: 100%; cursor: grab; }
    canvas:active { cursor: grabbing; }
    .edge-tooltip { position: absolute; display: none; min-width: 260px; max-width: 380px; background: rgba(2, 6, 23, 0.95); border: 1px solid #334155; border-radius: 4px; padding: 8px; color: #e5e7eb; font-size: 11px; line-height: 1.4; pointer-events: none; z-index: 20; box-shadow: 0 6px 18px rgba(0,0,0,0.35); font-family: "IBM Plex Mono", "Consolas", monospace; }
    .edge-tooltip .tt-title { color: #7dd3fc; font-weight: 700; margin-bottom: 6px; }
    .edge-tooltip .tt-row { border-top: 1px solid #1f2937; padding-top: 5px; margin-top: 5px; }
    .edge-tooltip .tt-proto { color: #93c5fd; font-weight: 700; }
    .edge-tooltip .tt-inferred { color: #fbbf24; font-weight: 700; }
    .edge-tooltip .tt-age { color: #94a3b8; }
    .edge-tooltip .tt-invalid { color: #fca5a5; }
    .edge-tooltip .tt-badge { display: inline-block; margin-left: 6px; padding: 1px 4px; border: 1px solid #ef4444; border-radius: 3px; color: #fecaca; font-size: 10px; font-weight: 700; }
    
    .legend { position: absolute; bottom: 20px; left: 20px; background: rgba(2, 6, 23, 0.92); padding: 12px; border-radius: 4px; font-size: 12px; color: #cbd5e1; pointer-events: none; border: 1px solid #334155; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    .legend-row { display: flex; align-items: center; gap: 8px; margin-top: 5px;}
    .color-box { width: 12px; height: 12px; border-radius: 2px; }
    .green { background: #10b981; } .yellow { background: #fbbf24; } .red { background: #ef4444; } .blue { background: #3b82f6; } .orange { background: #f59e0b; }
  </style>
</head>
<body>

  <div class="sidebar">
    <div class="header">
      <h2>Mesh Gateway</h2>
      <div class="status" id="status">Waiting for telemetry...</div>
    </div>
    
    <div class="panel">
      <div class="section-title">Route Planner</div>
      
      <div class="form-group">
        <label>Source Node</label>
        <select id="route-src"><option value="">Select source...</option></select>
      </div>
      <div class="form-group">
        <label>Destination Node</label>
        <select id="route-dst"><option value="">Select destination...</option></select>
      </div>
      <div class="form-group">
        <label>Routing Metric</label>
        <select id="route-metric">
          <option value="latency">Speed (Lowest Latency)</option>
          <option value="energy">Energy (Lowest EJ)</option>
          <option value="reliability">Reliability (Highest STR)</option>
        </select>
      </div>
      <div class="section-title">Protocol Filters</div>
      <div class="proto-filter">
        <label><input type="checkbox" id="pf-WIFI" checked>WIFI</label>
        <label><input type="checkbox" id="pf-BLE" checked>BLE</label>
        <label><input type="checkbox" id="pf-LORA" checked>LORA</label>
      </div>
      
      <button class="btn-primary" onclick="calculateRoute()">Compute Route</button>
      <button class="btn-secondary" onclick="clearRoute()">Clear Route</button>
      
      <div id="route-result"></div>

      <div class="section-title" style="margin-top: 25px;">Forwarding Table</div>
      <div id="telemetry-list">
        <div style="color:#64748b; font-size:12px; font-style:italic;">Select a Source Node above to view its real-time routing table.</div>
      </div>
    </div>
  </div>
  
  <div class="main">
    <canvas id="c"></canvas>
    <div class="edge-tooltip" id="edge-tooltip"></div>
    <div class="legend">
      <div style="font-weight: 700; color: #f8fafc; margin-bottom: 8px; font-size: 12px; border-bottom: 1px solid #334155; padding-bottom: 5px; font-family: 'IBM Plex Mono', monospace;">Network Legend</div>
      <div style="margin-bottom: 8px; font-size: 11px; color: #94a3b8; font-family: 'IBM Plex Mono', monospace;">
        Drag node = pin position<br>
        Double-click node = unpin
      </div>
      <div class="legend-row"><div class="color-box green"></div> Latency &lt; 50ms</div>
      <div class="legend-row"><div class="color-box yellow"></div> Latency 50 - 150ms</div>
      <div class="legend-row"><div class="color-box red"></div> Latency &gt; 150ms</div>
      <div class="legend-row"><div class="color-box blue"></div> Normal Node</div>
      <div class="legend-row"><div class="color-box orange"></div> Pinned Node</div>
      <div style="margin-top:8px; border-top:1px solid #334155; padding-top:6px; font-family:'IBM Plex Mono', monospace; color:#94a3b8;">
        Protocol Tags: [W]=WIFI [B]=BLE [L]=LORA
      </div>
    </div>
  </div>

  <script>
    const canvas = document.getElementById('c');
    const ctx = canvas.getContext('2d');
    let width, height;

    function resize() {
      const rect = canvas.parentNode.getBoundingClientRect();
      width = canvas.width = rect.width;
      height = canvas.height = rect.height;
    }
    window.addEventListener('resize', resize);
    resize();

    let rawNodes = [];
    let simulationNodes = [];
    let simulationEdges = [];
    let nodeMap = {};
    const edgeTooltip = document.getElementById('edge-tooltip');
    let hoverEdge = null;
    let lastMousePos = { x: 0, y: 0 };
    let invalidLinksByPair = {};
    
    let previousTopologyHash = "";
    let physicsFrames = 150; 

    // Routing State
    let activeRoutePath = [];
    let activeRouteMetric = "";
    let activeRouteHops = [];

    function isProtocolVisible(proto) {
      const p = String(proto || "").toUpperCase();
      if (!["WIFI", "BLE", "LORA"].includes(p)) return false;
      const el = document.getElementById(`pf-${p}`);
      if (!el) return true;
      return el.checked;
    }

    function protocolShort(proto) {
      const p = String(proto || "").toUpperCase();
      if (p === "WIFI") return "W";
      if (p === "BLE") return "B";
      if (p === "LORA") return "L";
      return "";
    }

    function pairKeyFor(a, b) {
      return a < b ? (a + "|" + b) : (b + "|" + a);
    }

    function pointSegmentDistance(px, py, ax, ay, bx, by) {
      const dx = bx - ax;
      const dy = by - ay;
      const len2 = dx * dx + dy * dy;
      if (len2 === 0) return Math.sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay));
      let t = ((px - ax) * dx + (py - ay) * dy) / len2;
      t = Math.max(0, Math.min(1, t));
      const x = ax + t * dx;
      const y = ay + t * dy;
      return Math.sqrt((px - x) * (px - x) + (py - y) * (py - y));
    }

    function hideEdgeTooltip() {
      hoverEdge = null;
      edgeTooltip.style.display = "none";
    }

    function renderEdgeTooltip(mousePos) {
      if (!hoverEdge) { hideEdgeTooltip(); return; }
      const visibleEdges = simulationEdges.filter(e => isProtocolVisible(e.protocol));
      const key = pairKeyFor(hoverEdge.source.id, hoverEdge.target.id);
      const rows = visibleEdges
        .filter(e => pairKeyFor(e.source.id, e.target.id) === key)
        .sort((a, b) => {
          const ka = `${a.source.id}->${a.target.id}|${a.protocol}|${a.inferred ? 1 : 0}`;
          const kb = `${b.source.id}->${b.target.id}|${b.protocol}|${b.inferred ? 1 : 0}`;
          if (ka < kb) return -1;
          if (ka > kb) return 1;
          return 0;
        })
        .map(e => `<div class="tt-row"><span class="tt-proto">${e.protocol}</span>${e.inferred ? ' <span class="tt-inferred">*</span>' : ''} ${e.source.id}→${e.target.id} | ${e.latency.toFixed(1)}ms | ${e.energy.toFixed(2)}ej | ${(e.reliability * 100).toFixed(1)}% <span class="tt-age">| upd ${e.ageSec}s ago</span></div>`)
        .join('');
      const invalidRows = (invalidLinksByPair[key] || [])
        .map(e => `<div class="tt-row tt-invalid"><span class="tt-proto">${e.protocol || "UNKNOWN"}</span> ${e.from}→${e.to} <span class="tt-badge">INVALID</span> ${e.reason}</div>`)
        .join('');
      const content = rows + invalidRows;

      edgeTooltip.innerHTML = `<div class="tt-title">${hoverEdge.source.id} ↔ ${hoverEdge.target.id}</div>${content || '<div class="tt-row">No visible protocol stats</div>'}`;
      edgeTooltip.style.display = "block";

      const tooltipRect = edgeTooltip.getBoundingClientRect();
      let left = mousePos.x + 14;
      let top = mousePos.y + 14;
      if (left + tooltipRect.width > width - 8) left = mousePos.x - tooltipRect.width - 14;
      if (top + tooltipRect.height > height - 8) top = mousePos.y - tooltipRect.height - 14;
      left = Math.max(8, left);
      top = Math.max(8, top);
      edgeTooltip.style.left = left + "px";
      edgeTooltip.style.top = top + "px";
    }

    function updateHover(mousePos) {
      if (draggedNode) { hideEdgeTooltip(); return; }
      const visibleEdges = simulationEdges.filter(e => isProtocolVisible(e.protocol));
      let best = null;
      let bestDist = Infinity;
      visibleEdges.forEach(e => {
        const d = pointSegmentDistance(mousePos.x, mousePos.y, e.source.x, e.source.y, e.target.x, e.target.y);
        if (d < bestDist) { bestDist = d; best = e; }
      });
      if (!best || bestDist > 12) {
        hideEdgeTooltip();
        return;
      }
      hoverEdge = best;
      renderEdgeTooltip(mousePos);
    }

    // Physics Constants
    const REPULSION = 5000;
    const SPRING_LEN = 200;
    const SPRING_K = 0.05;
    const DAMPING = 0.82;

    function syncDropdown(selectId, nodeIds) {
      const sel = document.getElementById(selectId);
      const currentVal = sel.value;
      const existingVals = Array.from(sel.options).map(o => o.value);
      
      nodeIds.forEach(id => {
        if (!existingVals.includes(id) && id !== "") sel.add(new Option(id, id));
      });
      for (let i = sel.options.length - 1; i >= 0; i--) {
        if (sel.options[i].value !== "" && !nodeIds.includes(sel.options[i].value)) sel.remove(i);
      }
      if (nodeIds.includes(currentVal)) sel.value = currentVal;
    }

    function buildDirectedBestGraph(metric) {
      const graph = {};
      const knownIds = simulationNodes.map(n => n.id);
      knownIds.forEach(id => graph[id] = {});
      const bestByPair = {};
      const explicitDir = new Set();
      const rawLinks = [];

      rawNodes.forEach(n => {
        const from = String(n.node_id || "").trim();
        if (!from || !graph[from]) return;

        (n.links || []).forEach(l => {
          const to = String(l.to || "").trim();
          if (!to || !(to in graph)) return;
          explicitDir.add(from + "|" + to);
          rawLinks.push({ from, to, link: l });
        });
      });

      function considerLink(from, to, l, inferred) {
        const latency = Number(l.latency);
        const energy = Number(l.energy);
        const reliability = Number(l.reliability);
        const protocol = String(l.protocol || "").trim().toUpperCase();
        if (!["WIFI", "BLE", "LORA"].includes(protocol)) return;

        let weight = latency;
        if (metric === 'energy') weight = energy;
        if (metric === 'reliability') weight = 1 - reliability;

        if (!Number.isFinite(weight)) return;

        const key = from + "|" + to;
        const candidate = {
          from: from, to: to, protocol: protocol,
          latency: Number.isFinite(latency) ? latency : 0,
          energy: Number.isFinite(energy) ? energy : 0,
          reliability: Number.isFinite(reliability) ? reliability : 0,
          weight: weight,
          inferred: inferred
        };

        const prev = bestByPair[key];
        const shouldReplace =
          !prev ||
          weight < prev.weight ||
          (prev.inferred && !candidate.inferred && weight === prev.weight);

        if (shouldReplace) {
          bestByPair[key] = candidate;
          graph[from][to] = weight;
        }
      }

      // Add explicit directional links first.
      rawLinks.forEach(item => {
        considerLink(item.from, item.to, item.link, false);
      });

      // If reverse direction is missing entirely, add inferred reverse link.
      rawLinks.forEach(item => {
        const reverseKey = item.to + "|" + item.from;
        if (!explicitDir.has(reverseKey)) {
          considerLink(item.to, item.from, item.link, true);
        }
      });

      return { graph, knownIds, bestByPair };
    }

    // Triggers routing table update when source or metric changes
    document.getElementById('route-src').addEventListener('change', updateRoutingTable);
    document.getElementById('route-metric').addEventListener('change', updateRoutingTable);
    ['WIFI', 'BLE', 'LORA'].forEach(p => {
      const el = document.getElementById(`pf-${p}`);
      if (el) el.addEventListener('change', () => { hideEdgeTooltip(); draw(); });
    });

    function updateGraph() {
      const hoveredPairKey = hoverEdge
        ? pairKeyFor(hoverEdge.source.id, hoverEdge.target.id)
        : null;
      const allKnownNodes = new Set();
      
      const currentHash = rawNodes.map(n => {
          n.links.forEach(l => allKnownNodes.add(l.to));
          allKnownNodes.add(n.node_id);
          return n.node_id + ">" + n.links.map(l=>l.to).sort().join(',');
      }).sort().join('|');

      if (currentHash !== previousTopologyHash) {
          physicsFrames = 150; 
          previousTopologyHash = currentHash;
      }

      const nodeIds = Array.from(allKnownNodes);
      syncDropdown('route-src', nodeIds);
      syncDropdown('route-dst', nodeIds);

      const newMap = {};
      
      rawNodes.forEach(n => {
        if(nodeMap[n.node_id]) { newMap[n.node_id] = nodeMap[n.node_id]; } 
        else { newMap[n.node_id] = { id: n.node_id, x: width/2 + Math.random()*50, y: height/2 + Math.random()*50, vx: 0, vy: 0, fixed: false }; }
      });
      
      rawNodes.forEach(n => {
        n.links.forEach(l => {
          if(!newMap[l.to]) { 
            if(nodeMap[l.to]) { newMap[l.to] = nodeMap[l.to]; } 
            else { newMap[l.to] = { id: l.to, x: width/2 + Math.random()*50, y: height/2 + Math.random()*50, vx: 0, vy: 0, fixed: false }; }
          }
        });
      });

      nodeMap = newMap;
      simulationNodes = Object.values(nodeMap);
      
      simulationEdges = [];
      const explicitDir = new Set();
      const rawLinks = [];
      invalidLinksByPair = {};
      rawNodes.forEach(n => {
        const from = String(n.node_id || "").trim();
        const ageSec = Number.isFinite(Number(n.last_seen_sec_ago)) ? Number(n.last_seen_sec_ago) : 0;
        (n.links || []).forEach(l => {
          const to = String(l.to || "").trim();
          if (!from || !to) return;
          explicitDir.add(from + "|" + to);
          rawLinks.push({ from, to, ageSec, link: l });
        });
        (n.invalid_links || []).forEach(l => {
          const to = String(l.to || "").trim();
          if (!from || !to) return;
          const key = pairKeyFor(from, to);
          if (!invalidLinksByPair[key]) invalidLinksByPair[key] = [];
          invalidLinksByPair[key].push({
            from: from,
            to: to,
            protocol: String(l.protocol || "").toUpperCase(),
            reason: String(l.reason || "invalid stats"),
          });
        });
      });

      function pushEdge(from, to, l, inferred, ageSec) {
        const proto = String(l.protocol || "").toUpperCase();
        if (!["WIFI", "BLE", "LORA"].includes(proto)) return;
        const lat = Number.isFinite(Number(l.latency)) ? Number(l.latency) : 0;
        const eng = Number.isFinite(Number(l.energy)) ? Number(l.energy) : 0;
        const rel = Number.isFinite(Number(l.reliability)) ? Number(l.reliability) : 0;
        if(nodeMap[from] && nodeMap[to]) {
          simulationEdges.push({
            source: nodeMap[from],
            target: nodeMap[to],
            latency: lat,
            energy: eng,
            reliability: rel,
            protocol: proto,
            ageSec: ageSec,
            inferred: inferred,
            label: inferred ? (proto + "*") : proto
          });
        }
      }

      rawLinks.forEach(item => pushEdge(item.from, item.to, item.link, false, item.ageSec));
      rawLinks.forEach(item => {
        const reverseKey = item.to + "|" + item.from;
        if (!explicitDir.has(reverseKey)) pushEdge(item.to, item.from, item.link, true, item.ageSec);
      });

      const totalNodes = simulationNodes.length;
      const totalLinks = simulationEdges.length;
      document.getElementById('status').textContent = `TX:${rawNodes.length} NODES:${totalNodes} LINKS:${totalLinks} REFRESH:3s`;
      if (hoveredPairKey) {
        const refreshedHover = simulationEdges.find(e =>
          isProtocolVisible(e.protocol) &&
          pairKeyFor(e.source.id, e.target.id) === hoveredPairKey
        );
        if (refreshedHover) {
          hoverEdge = refreshedHover;
          renderEdgeTooltip(lastMousePos);
        } else {
          hideEdgeTooltip();
        }
      }
      
      updateRoutingTable(); // Refresh the table
      if(activeRoutePath.length > 0) calculateRoute(); // Refresh the isolated path
    }

    // --- Routing Table Logic ---
    function updateRoutingTable() {
      const src = document.getElementById('route-src').value;
      const metric = document.getElementById('route-metric').value;
      const container = document.getElementById('telemetry-list');
      
      if (!src) {
        container.innerHTML = '<div style="color:#64748b; font-size:12px; font-style:italic; padding:10px;">Select a Source Node above to view its real-time routing table.</div>';
        return;
      }

      const { graph, knownIds, bestByPair } = buildDirectedBestGraph(metric);

      // Dijkstra
      const dists = {}; const prev = {}; const q = new Set(knownIds);
      q.forEach(n => { dists[n] = Infinity; prev[n] = null; });
      dists[src] = 0;

      while (q.size > 0) {
        let u = null;
        q.forEach(n => { if (u === null || dists[n] < dists[u]) u = n; });
        if (dists[u] === Infinity) break;
        q.delete(u);

        for (let v in graph[u]) {
          if (q.has(v)) {
            let alt = dists[u] + graph[u][v];
            if (alt < dists[v]) { dists[v] = alt; prev[v] = u; }
          }
        }
      }

      // Build Table HTML
      let html = `<table class="rt-table">
                    <thead>
                      <tr><th>Dest</th><th>Next Hop</th><th>Cost (${metric.substring(0,3)})</th></tr>
                    </thead>
                    <tbody>`;
      
      let hasRoutes = false;
      knownIds.forEach(dst => {
        if (dst !== src && dists[dst] !== Infinity) {
          hasRoutes = true;
          
          let p = []; let c = dst;
          while(c) { p.unshift(c); c = prev[c]; }
          if (p[0] !== src || p.length < 2) return;
          const nextHop = p[1];

          let realLat = 0, realEng = 0, realRel = 1;
          let validRoute = true;
          for(let i = 0; i < p.length - 1; i++){
            const hop = bestByPair[p[i] + "|" + p[i + 1]];
            if (!hop) { validRoute = false; break; }
            realLat += hop.latency;
            realEng += hop.energy;
            realRel *= hop.reliability;
          }
          if (!validRoute) return;
          
          let displayCost = "";
          if (metric === 'latency') displayCost = realLat.toFixed(1) + 'ms';
          else if (metric === 'energy') displayCost = realEng.toFixed(2) + 'ej';
          else if (metric === 'reliability') displayCost = (realRel*100).toFixed(0) + '%';

          html += `<tr>
                     <td class="rt-node">${dst}</td>
                     <td class="rt-node" style="color:#f8fafc">${nextHop}</td>
                     <td class="rt-cost">${displayCost}</td>
                   </tr>`;
        }
      });
      html += `</tbody></table>`;
      
      if (!hasRoutes) {
        html = '<div style="color:#ef4444; font-size:12px; padding:10px;">No reachable destinations found from this node.</div>';
      }
      
      container.innerHTML = html;
    }

    // --- Isolated Path Logic ---
    function clearRoute() {
      activeRoutePath = [];
      activeRouteHops = [];
      document.getElementById('route-result').style.display = 'none';
      hideEdgeTooltip();
      draw();
    }

    function calculateRoute() {
      const src = document.getElementById('route-src').value;
      const dst = document.getElementById('route-dst').value;
      const metric = document.getElementById('route-metric').value;
      const res = document.getElementById('route-result');

      if (!src || !dst) {
        res.style.display = "block"; res.className = "res-error"; res.innerHTML = "Select source & destination."; return;
      }
      if (src === dst) {
        res.style.display = "block"; res.className = "res-error"; res.innerHTML = "Source and destination must differ."; return;
      }

      const { graph, knownIds, bestByPair } = buildDirectedBestGraph(metric);

      const dists = {}; const prev = {}; const q = new Set(knownIds);
      q.forEach(n => { dists[n] = Infinity; prev[n] = null; });
      dists[src] = 0;

      while (q.size > 0) {
        let u = null;
        q.forEach(n => { if (u === null || dists[n] < dists[u]) u = n; });
        if (dists[u] === Infinity || u === dst) break;
        q.delete(u);

        for (let v in graph[u]) {
          if (q.has(v)) {
            let alt = dists[u] + graph[u][v];
            if (alt < dists[v]) { dists[v] = alt; prev[v] = u; }
          }
        }
      }

      let path = []; let curr = dst;
      while (curr) { path.unshift(curr); curr = prev[curr]; }

      if (dists[dst] !== Infinity && path[0] === src) {
        const hops = [];
        let tLat = 0, tEng = 0, rProd = 1;
        let validRoute = true;
        for (let i = 0; i < path.length - 1; i++) {
          const u = path[i];
          const v = path[i + 1];
          const hop = bestByPair[u + "|" + v];
          if (!hop) { validRoute = false; break; }
          hops.push(hop);
          tLat += hop.latency;
          tEng += hop.energy;
          rProd *= hop.reliability;
        }

        if (!validRoute || hops.length === 0) {
          activeRoutePath = [];
          activeRouteHops = [];
          res.style.display = "block"; res.className = "res-error";
          res.innerHTML = "No valid route found between selected nodes.";
          draw();
          return;
        }

        activeRoutePath = path;
        activeRouteMetric = metric;
        activeRouteHops = hops;

        const metricName = metric === "latency" ? "LATENCY" : (metric === "energy" ? "ENERGY" : "RELIABILITY");
        const hopRows = hops.map((h, idx) =>
          `<tr>
            <td>${idx + 1}</td>
            <td>${h.from} ➔ ${h.to}</td>
            <td>${h.protocol}${h.inferred ? ' <span class="hop-inferred">*</span>' : ''}</td>
            <td>${h.latency.toFixed(1)}ms</td>
            <td>${h.energy.toFixed(2)}ej</td>
            <td>${(h.reliability * 100).toFixed(1)}%</td>
          </tr>`
        ).join('');
        
        res.style.display = "block"; res.className = "res-success";
        res.innerHTML = `
          <div class="route-title">ROUTE_COMPUTED : OK</div>
          <div class="route-path">${path.join(' ➔ ')}</div>
          <div class="kpi-grid">
            <div class="kpi"><div class="kpi-label">Metric</div><div class="kpi-value">${metricName}</div></div>
            <div class="kpi"><div class="kpi-label">Hops</div><div class="kpi-value">${hops.length}</div></div>
            <div class="kpi"><div class="kpi-label">Source</div><div class="kpi-value">${src}</div></div>
          </div>
          <table class="hop-table">
            <thead>
              <tr><th>#</th><th>Link</th><th>Proto</th><th>Lat</th><th>Eng</th><th>Rel</th></tr>
            </thead>
            <tbody>${hopRows}</tbody>
          </table>
          <div class="kpi-grid">
            <div class="kpi"><div class="kpi-label">Total Lat</div><div class="kpi-value">${tLat.toFixed(1)}ms</div></div>
            <div class="kpi"><div class="kpi-label">Total Eng</div><div class="kpi-value">${tEng.toFixed(2)}ej</div></div>
            <div class="kpi"><div class="kpi-label">Path Rel</div><div class="kpi-value">${(rProd * 100).toFixed(1)}%</div></div>
          </div>
          <div class="subtle-note">* inferred reverse link when only one direction is reported</div>
        `;
      } else {
        activeRoutePath = [];
        activeRouteHops = [];
        res.style.display = "block"; res.className = "res-error";
        res.innerHTML = "No valid route found between selected nodes.";
      }
      
      draw(); 
    }

    function isEdgeInRoute(edge) {
      if (activeRouteHops.length === 0) return false;
      for (let i = 0; i < activeRouteHops.length; i++) {
        const h = activeRouteHops[i];
        if (h.from === edge.source.id && h.to === edge.target.id && h.protocol === edge.protocol) return true;
      }
      return false;
    }

    // --- Physics Simulation Step ---
    function step() {
      for(let i=0; i<simulationNodes.length; i++) {
        for(let j=i+1; j<simulationNodes.length; j++) {
          const n1 = simulationNodes[i];
          const n2 = simulationNodes[j];
          const dx = n2.x - n1.x;
          const dy = n2.y - n1.y;
          let dist = Math.sqrt(dx*dx + dy*dy);
          if(dist === 0) dist = 0.1;
          const force = REPULSION / (dist * dist);
          const fx = (dx / dist) * force;
          const fy = (dy / dist) * force;
          if(!n1.fixed && n1 !== draggedNode) { n1.vx -= fx; n1.vy -= fy; }
          if(!n2.fixed && n2 !== draggedNode) { n2.vx += fx; n2.vy += fy; }
        }
      }

      simulationEdges.forEach(e => {
        const dx = e.target.x - e.source.x;
        const dy = e.target.y - e.source.y;
        let dist = Math.sqrt(dx*dx + dy*dy);
        if(dist === 0) dist = 0.1;
        const diff = dist - SPRING_LEN;
        const force = diff * SPRING_K;
        const fx = (dx / dist) * force;
        const fy = (dy / dist) * force;
        if(!e.source.fixed && e.source !== draggedNode) { e.source.vx += fx; e.source.vy += fy; }
        if(!e.target.fixed && e.target !== draggedNode) { e.target.vx -= fx; e.target.vy -= fy; }
      });

      simulationNodes.forEach(n => {
        if(!n.fixed && draggedNode !== n) {
          n.vx += (width/2 - n.x) * 0.005;
          n.vy += (height/2 - n.y) * 0.005;
          n.vx *= DAMPING;
          n.vy *= DAMPING;
          n.x += n.vx;
          n.y += n.vy;
        }
      });
    }

    // --- Interaction (Fixed Drag Logic) ---
    let draggedNode = null;
    let lastClickTime = 0;
    let hasMoved = false; 
    let dragStartX = 0;
    let dragStartY = 0;
    
    function getMousePos(e) {
      const rect = canvas.getBoundingClientRect();
      const clientX = e.touches ? e.touches[0].clientX : e.clientX;
      const clientY = e.touches ? e.touches[0].clientY : e.clientY;
      return { x: clientX - rect.left, y: clientY - rect.top };
    }

    function onDown(e) {
      const pos = getMousePos(e);
      const currentTime = new Date().getTime();
      const isDoubleClick = (currentTime - lastClickTime) < 300;
      lastClickTime = currentTime;

      for(let n of simulationNodes) {
        const dx = pos.x - n.x;
        const dy = pos.y - n.y;
        if(dx*dx + dy*dy < 900) { 
          if(isDoubleClick) {
            n.fixed = false;
            n.vx = 0; n.vy = 0;
            physicsFrames = 60;
          } else {
            draggedNode = n;
            hasMoved = false;
            dragStartX = pos.x;
            dragStartY = pos.y;
          }
          break;
        }
      }
    }

    function onMove(e) {
      const pos = getMousePos(e);
      lastMousePos = pos;
      updateHover(pos);

      if(draggedNode) {
        e.preventDefault();
        
        const distMoved = Math.abs(pos.x - dragStartX) + Math.abs(pos.y - dragStartY);
        if (distMoved > 5) {
          hasMoved = true;
        }

        if (hasMoved) {
          draggedNode.x = pos.x;
          draggedNode.y = pos.y;
          draggedNode.vx = 0; draggedNode.vy = 0;
          physicsFrames = 60; 
        }
      }
    }

    function onUp() { 
      if(draggedNode) {
        if (hasMoved) draggedNode.fixed = true;
        draggedNode = null; 
        hasMoved = false;
      }
    }

    canvas.addEventListener('mousedown', onDown);
    canvas.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    canvas.addEventListener('touchstart', onDown, {passive: false});
    canvas.addEventListener('touchmove', onMove, {passive: false});
    window.addEventListener('touchend', onUp);
    canvas.addEventListener('mouseleave', hideEdgeTooltip);

    // --- Rendering ---
    function draw() {
      ctx.clearRect(0, 0, width, height);
      const routingMode = activeRouteHops.length > 0;
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";

      const renderEdges = simulationEdges.filter(e => isProtocolVisible(e.protocol));
      const pairMeta = {};

      // Draw edges only. Protocol details are shown on hover tooltip.
      renderEdges.forEach(e => {
        const k = pairKeyFor(e.source.id, e.target.id);
        if (!pairMeta[k]) {
          pairMeta[k] = {
            source: e.source,
            target: e.target,
            tags: new Set()
          };
        }
        pairMeta[k].tags.add(protocolShort(e.protocol));

        const inRoute = isEdgeInRoute(e);
        
        let lineColor = "#10b981"; // Green
        if (e.latency >= 150) lineColor = "#ef4444"; // Red
        else if (e.latency >= 50) lineColor = "#fbbf24"; // Yellow
        
        if (routingMode && inRoute) lineColor = "#34d399"; 
        
        ctx.lineWidth = routingMode && inRoute ? 4 : 2;
        ctx.strokeStyle = lineColor;
        ctx.globalAlpha = routingMode && !inRoute ? 0.55 : 1.0;
        
        ctx.beginPath();
        ctx.moveTo(e.source.x, e.source.y);
        ctx.lineTo(e.target.x, e.target.y);
        ctx.stroke();
        ctx.globalAlpha = 1.0;
      });

      // Draw compact protocol tags once per connection pair: [B][W][L]
      const protoLabels = [];
      Object.keys(pairMeta).forEach(k => {
        const m = pairMeta[k];
        const dx = m.target.x - m.source.x;
        const dy = m.target.y - m.source.y;
        const dist = Math.sqrt(dx * dx + dy * dy) || 1;
        let nx = -dy / dist;
        let ny = dx / dist;
        if (!Number.isFinite(nx) || !Number.isFinite(ny)) { nx = 0; ny = -1; }
        const text = Array.from(m.tags).sort().map(t => `[${t}]`).join("");
        const x = ((m.source.x + m.target.x) / 2) + nx * 12;
        const y = ((m.source.y + m.target.y) / 2) + ny * 12;
        ctx.font = "bold 10px monospace";
        const w = ctx.measureText(text).width + 10;
        const h = 16;
        protoLabels.push({ x, y, w, h, nx, ny, text });
      });

      // Simple collision pass for protocol tags.
      const placedProto = [];
      protoLabels.forEach(lbl => {
        const c = { ...lbl };
        let t = 0;
        while (t < 16) {
          let clash = false;
          for (let i = 0; i < placedProto.length; i++) {
            const p = placedProto[i];
            const ox = Math.abs(c.x - p.x) < ((c.w + p.w) / 2);
            const oy = Math.abs(c.y - p.y) < ((c.h + p.h) / 2);
            if (ox && oy) { clash = true; break; }
          }
          if (!clash) break;
          c.x += c.nx * 10;
          c.y += c.ny * 10;
          t++;
        }
        c.x = Math.max((c.w / 2) + 6, Math.min(width - (c.w / 2) - 6, c.x));
        c.y = Math.max((c.h / 2) + 6, Math.min(height - (c.h / 2) - 6, c.y));
        placedProto.push(c);
      });

      placedProto.forEach(lbl => {
        ctx.fillStyle = "rgba(2, 6, 23, 0.9)";
        ctx.strokeStyle = "#334155";
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.roundRect(lbl.x - lbl.w / 2, lbl.y - lbl.h / 2, lbl.w, lbl.h, 4);
        ctx.fill();
        ctx.stroke();
        ctx.fillStyle = "#cbd5e1";
        ctx.fillText(lbl.text, lbl.x, lbl.y + 0.5);
      });

      // Draw Nodes
      ctx.font = "bold 13px monospace";
      simulationNodes.forEach(n => {
        const inRoute = routingMode ? activeRoutePath.includes(n.id) : true;
        ctx.globalAlpha = 1.0;
        
        ctx.beginPath();
        ctx.arc(n.x, n.y, 25, 0, Math.PI*2);
        
        ctx.fillStyle = n.fixed ? "#f59e0b" : "#3b82f6";
        if (routingMode && inRoute) ctx.fillStyle = "#34d399";
        
        ctx.fill();
        
        ctx.lineWidth = 3;
        ctx.strokeStyle = n.fixed ? "#b45309" : "#1e3a8a";
        
        if (draggedNode === n && hasMoved) ctx.strokeStyle = "#ffffff";
        
        ctx.stroke();

        ctx.fillStyle = "#ffffff";
        ctx.fillText(n.id, n.x, n.y);
        
        ctx.globalAlpha = 1.0; 
      });
    }

    function loop() {
      if (physicsFrames > 0 || draggedNode) {
        step();
        if (!draggedNode) physicsFrames--;
      }
      draw(); 
      requestAnimationFrame(loop);
    }
    loop();

    async function refreshData() {
      try {
        const r = await fetch("/api/nodes");
        const j = await r.json();
        rawNodes = j.nodes || [];
        updateGraph();
      } catch (e) {}
    }

    refreshData();
    setInterval(refreshData, 3000);
  </script>
</body>
</html>
"""


# ---------------------------
# Helpers
# ---------------------------
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if wlan.isconnected():
        print("WiFi already connected:", wlan.ifconfig())
        return wlan

    if not WIFI_SSID or WIFI_SSID == "YOUR_WIFI_SSID":
        print("WiFi creds not set. Skipping connect.")
        return wlan

    print("Connecting WiFi...")
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    t0 = utime.ticks_ms()
    while not wlan.isconnected() and utime.ticks_diff(utime.ticks_ms(), t0) < 15000:
        utime.sleep_ms(200)
    print("WiFi status:", wlan.isconnected(), wlan.ifconfig() if wlan.isconnected() else "")
    return wlan


def ticks_now():
    return utime.ticks_ms()


def parse_number(s, as_int=False):
    try:
        if as_int:
            return int(s)
        return float(s)
    except Exception:
        return 0 if as_int else 0.0


def get_invalid_link_reason(link):
    protocol = str(link.get("protocol", "")).strip().upper()
    if protocol not in ("WIFI", "BLE", "LORA"):
        return "bad protocol"
    if str(link.get("to", "")).strip() == "":
        return "missing destination"

    rssi = parse_number(link.get("rssi", 0), as_int=True)
    latency = parse_number(link.get("latency", 0.0), as_int=False)
    energy = parse_number(link.get("energy", 0.0), as_int=False)
    reliability = parse_number(link.get("reliability", 0.0), as_int=False)

    reasons = []
    # User rule: if any protocol stat is 0/0.0, treat it as invalid.
    if rssi == 0:
        reasons.append("rssi=0")
    if latency == 0.0:
        reasons.append("latency=0")
    if energy == 0.0:
        reasons.append("energy=0")
    if reliability == 0.0:
        reasons.append("reliability=0")
    if reasons:
        return ", ".join(reasons)

    return ""


def split_links_by_validity(links):
    valid = []
    invalid = []
    i = 0
    while i < len(links):
        l = links[i]
        reason = get_invalid_link_reason(l)
        if reason:
            invalid.append({
                "to": str(l.get("to", "")).strip(),
                "protocol": str(l.get("protocol", "")).strip().upper(),
                "reason": reason,
            })
        else:
            valid.append({
                "to": str(l.get("to", "")).strip(),
                "protocol": str(l.get("protocol", "")).strip().upper(),
                "rssi": parse_number(l.get("rssi", 0), as_int=True),
                "latency": parse_number(l.get("latency", 0.0), as_int=False),
                "energy": parse_number(l.get("energy", 0.0), as_int=False),
                "reliability": parse_number(l.get("reliability", 0.0), as_int=False),
            })
        i += 1
    return valid, invalid


def parse_link_item_csv(item):
    p = item.split(",")
    if len(p) < 6:
        return None
    link = {
        "to": p[0],
        "protocol": p[1],
        "rssi": parse_number(p[2], as_int=True),
        "latency": parse_number(p[3], as_int=False),
        "energy": parse_number(p[4], as_int=False),
        "reliability": parse_number(p[5], as_int=False),
    }
    return link


def parse_ls_string(msg):
    parts = msg.split("|")
    if len(parts) < 2 or parts[0] != "LS":
        return None

    node_id = parts[1]
    links = []
    i = 2
    while i < len(parts):
        link = parse_link_item_csv(parts[i])
        if link:
            links.append(link)
        i += 1

    return {
        "node_id": node_id,
        "seq": None,
        "links": links,
    }


def parse_json_payload(msg):
    try:
        obj = ujson.loads(msg)
    except Exception:
        return None

    node_id = obj.get("node_id")
    if not node_id:
        return None

    seq = obj.get("seq", None)
    links_in = obj.get("links", [])
    links = []
    for l in links_in:
        links.append({
            "to": l.get("to", ""),
            "protocol": l.get("protocol", ""),
            "rssi": parse_number(l.get("rssi", 0), as_int=True),
            "latency": parse_number(l.get("latency", 0.0), as_int=False),
            "energy": parse_number(l.get("energy", 0.0), as_int=False),
            "reliability": parse_number(l.get("reliability", 0.0), as_int=False),
        })

    return {"node_id": node_id, "seq": seq, "links": links}


def parse_wrapped_payload(msg):
    p1 = msg.find("|")
    if p1 <= 0:
        return None
    p2 = msg.find("|", p1 + 1)
    if p2 <= p1 + 1:
        return None

    node_id = msg[:p1]
    seq_s = msg[p1 + 1:p2]
    data = msg[p2 + 1:]
    try:
        seq = int(seq_s)
    except Exception:
        return None

    parsed = parse_payload(data)
    if not parsed:
        last = data.rfind("|")
        if last > 0:
            parsed = parse_payload(data[:last])
    if not parsed:
        return None

    parsed["node_id"] = node_id
    parsed["seq"] = seq
    return parsed


def parse_payload(msg):
    msg = msg.strip()
    if not msg:
        return None

    if msg.startswith("LS|"):
        return parse_ls_string(msg)

    if msg.startswith("{"):
        return parse_json_payload(msg)

    wrapped = parse_wrapped_payload(msg)
    if wrapped:
        return wrapped

    return None


def should_drop_duplicate(node_id, seq):
    if seq is None:
        return False

    prev = last_seq.get(node_id, None)
    if prev is not None and seq <= prev:
        return True

    last_seq[node_id] = seq
    return False


def compute_winners(links):
    if not links:
        return None, None, None

    best_str = None
    best_spd = None
    best_ej = None

    i = 0
    while i < len(links):
        l = links[i]
        if (best_str is None) or (l["reliability"] > best_str["reliability"]):
            best_str = l
        if (best_spd is None) or (l["latency"] < best_spd["latency"]):
            best_spd = l
        if (best_ej is None) or (l["energy"] < best_ej["energy"]):
            best_ej = l
        i += 1

    return best_str, best_spd, best_ej


def upsert_node(parsed):
    node_id = parsed["node_id"]
    seq = parsed["seq"]
    links, invalid_links = split_links_by_validity(parsed["links"])

    if should_drop_duplicate(node_id, seq):
        return

    nodes[node_id] = {
        "node_id": node_id,
        "last_seen": ticks_now(),
        "seq": seq,
        "links": links,
        "invalid_links": invalid_links,
    }


def expire_nodes():
    now = ticks_now()
    to_delete = []
    for node_id, n in nodes.items():
        age = utime.ticks_diff(now, n["last_seen"])
        if age > NODE_EXPIRE_MS:
            to_delete.append(node_id)

    i = 0
    while i < len(to_delete):
        node_id = to_delete[i]
        if node_id in nodes:
            del nodes[node_id]
        if node_id in last_seq:
            del last_seq[node_id]
        i += 1


def build_nodes_api():
    now = ticks_now()
    out = []

    for node_id, n in nodes.items():
        links = n["links"]
        best_str, best_spd, best_ej = compute_winners(links)
        age_ms = utime.ticks_diff(now, n["last_seen"])
        age_s = int(age_ms / 1000)

        out.append({
            "node_id": node_id,
            "seq": n["seq"],
            "last_seen": n["last_seen"],
            "last_seen_sec_ago": age_s,
            "STR": best_str,
            "SPD": best_spd,
            "EJ": best_ej,
            "links": links,
            "invalid_links": n.get("invalid_links", []),
        })

    return {"nodes": out}


def send_http(conn, status, content_type, body):
    if isinstance(body, str):
        body = body.encode("utf-8")

    hdr = (
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n"
    ) % (status, content_type, len(body))

    try:
        conn.settimeout(1.0)
        conn.sendall(hdr.encode("utf-8"))
        chunk_size = 1024
        for i in range(0, len(body), chunk_size):
            conn.sendall(body[i:i+chunk_size])
    except Exception as e:
        print("HTTP Send Error:", e)


def handle_http_client(conn):
    try:
        conn.settimeout(0.2)
        req = conn.recv(512)
        if not req:
            return

        line_end = req.find(b"\r\n")
        if line_end < 0:
            line_end = len(req)
        line = req[:line_end].decode("utf-8")
        parts = line.split(" ")
        if len(parts) < 2:
            send_http(conn, "400 Bad Request", "text/plain", "bad request")
            return

        method = parts[0]
        path = parts[1]
        if method != "GET":
            send_http(conn, "405 Method Not Allowed", "text/plain", "method not allowed")
            return

        if path == "/":
            send_http(conn, "200 OK", "text/html; charset=utf-8", HTML_PAGE)
            return

        if path == "/api/nodes":
            payload = ujson.dumps(build_nodes_api())
            send_http(conn, "200 OK", "application/json", payload)
            return

        send_http(conn, "404 Not Found", "text/plain", "not found")
    except Exception:
        pass
    finally:
        try:
            conn.close()
        except Exception:
            pass


def setup_udp():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", UDP_PORT))
    s.setblocking(False)
    print("UDP listening on", UDP_PORT)
    return s


def setup_http():
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", HTTP_PORT))
    s.listen(2)
    s.setblocking(False)
    print("HTTP listening on", HTTP_PORT)
    return s


def handle_udp(sock):
    try:
        data, addr = sock.recvfrom(1024)
    except Exception:
        return

    try:
        msg = data.decode("utf-8").strip()
    except Exception:
        return

    parsed = parse_payload(msg)
    if not parsed:
        return

    upsert_node(parsed)


def main():
    connect_wifi()
    udp_sock = setup_udp()
    http_sock = setup_http()

    while True:
        expire_nodes()

        rlist, _, _ = select.select([udp_sock, http_sock], [], [], SELECT_TIMEOUT_S)
        i = 0
        while i < len(rlist):
            ready = rlist[i]
            if ready is udp_sock:
                handle_udp(udp_sock)
            elif ready is http_sock:
                try:
                    conn, _ = http_sock.accept()
                    handle_http_client(conn)
                except Exception:
                    pass
            i += 1


if __name__ == "__main__":
    main()
