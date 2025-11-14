/* 機能統合用サーバー */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// mimeパッケージの正しい使用方法
let mime;
try {
    mime = require('mime-types'); // mime-types パッケージを使用
} catch (e) {
    try {
        const mimeLib = require('mime');
        // 新しいバージョンのmimeパッケージの場合
        mime = {
            lookup: mimeLib.lookup || mimeLib.getType,
        };
    } catch (e2) {
        // フォールバック
        mime = {
            lookup: (filePath) => {
                const ext = path.extname(filePath).toLowerCase();
                const mimeTypes = {
                    '.html': 'text/html',
                    '.css': 'text/css',
                    '.js': 'text/javascript',
                    '.json': 'application/json',
                    '.png': 'image/png',
                    '.jpg': 'image/jpeg',
                    '.jpeg': 'image/jpeg',
                    '.gif': 'image/gif',
                    '.svg': 'image/svg+xml',
                    '.txt': 'text/plain',
                    '.csv': 'text/csv',
                    '.geojson': 'application/geo+json',
                };
                return mimeTypes[ext] || 'application/octet-stream';
            },
        };
    }
}

const server = http.createServer(async (req, res) => {
    /* ===== ① すべてのレスポンスに CORS ヘッダーを付与 ===== */
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    /* ===== ② プリフライトなら何もせず返す ===== */
    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        return res.end();
    }

    /* ===== ③ /calc への POST ===== */
    if (req.url === '/calc' && req.method === 'POST') {
        let body = '';
        req.on('data', (chunk) => (body += chunk));
        req.on('end', async () => {
            try {
                const p = new URLSearchParams(body);

                const base_args = [
                    p.get('weight0'),
                    p.get('weight1'),
                    p.get('weight2'),
                    p.get('weight3'),
                    p.get('weight4'),
                    p.get('weight5'),
                    p.get('weight6'),
                    p.get('weight7'),
                    p.get('weight8'),
                    p.get('weight9'),
                    p.get('weight10'),
                    p.get('weight11'),
                    p.get('weight12'),
                    p.get('param1'),
                    p.get('param2'),
                ].join(' ');

                const walkingSpeed = parseFloat(p.get('walkingSpeed')) || 80; // m/min
                const startNode = p.get('param1');
                const endNode = p.get('param2');

                // Run up44 once to generate the cost file
                execSync(`./up44 ${base_args}`, { encoding: 'utf8', cwd: __dirname });

                // Load route and signal data
                const routeInfoCsv = fs.readFileSync('oomiya_route_inf_4.csv', 'utf8');
                const signalInfoCsv = fs.readFileSync('signal_inf.csv', 'utf8');
                const routeDataMap = new Map();
                routeInfoCsv
                    .split('\n')
                    .slice(1)
                    .forEach((line) => {
                        if (line.trim() === '') return;
                        const cols = line.trim().split(',');
                        routeDataMap.set(`${cols[0]}-${cols[1]}`, {
                            distance: parseFloat(cols[2]),
                            gradient: parseFloat(cols[4]),
                            isSignal: cols[8] === '1',
                            isCrosswalk: cols[15] === '1', // 横断歩道フラグ
                        });
                    });

                const signalDataMap = new Map();
                signalInfoCsv
                    .split('\n')
                    .slice(1)
                    .forEach((line) => {
                        if (line.trim() === '') return;
                        const cols = line.trim().split(',');
                        signalDataMap.set(`${cols[0]}-${cols[1]}`, {
                            cycle: parseFloat(cols[2]), // 周期（秒）
                            green: parseFloat(cols[3]), // 青信号時間（秒）
                            phase: parseFloat(cols[4]) || 0, // 位相（秒）- オプション
                        });
                    });

                // Helper function to calculate route metrics
                const calculateRouteMetrics = (routeEdges) => {
                    let totalDistance = 0;
                    let totalTime = 0;
                    const K = 0.5;

                    routeEdges.forEach((edge) => {
                        const edgeData = routeDataMap.get(edge);
                        if (edgeData) {
                            totalDistance += edgeData.distance;
                            const adjustedSpeed = walkingSpeed * (1 - K * edgeData.gradient);
                            if (adjustedSpeed > 0) {
                                totalTime += edgeData.distance / adjustedSpeed;
                            }
                        }
                    });

                    // Calculate expected wait time and worst case wait time
                    let totalExpectedWaitTimeSeconds = 0;
                    let totalWorstWaitTimeSeconds = 0;
                    let cumulativeTime = 0; // 累積移動時間（秒）

                    for (const edge of routeEdges) {
                        const edgeData = routeDataMap.get(edge);
                        if (edgeData) {
                            // 移動時間を計算（秒単位）
                            const adjustedSpeed = walkingSpeed * (1 - K * edgeData.gradient);
                            if (adjustedSpeed > 0) {
                                const travelTimeSeconds = (edgeData.distance / adjustedSpeed) * 60;
                                cumulativeTime += travelTimeSeconds;
                            }

                            // 信号待ち時間を計算
                            if (edgeData.isSignal) {
                                const signalData = signalDataMap.get(edge);
                                if (signalData && signalData.cycle > 0) {
                                    const redTime = signalData.cycle - signalData.green;
                                    if (redTime > 0) {
                                        // Expected wait time (ランダム到着を仮定)
                                        const expectedWaitTime =
                                            (redTime * redTime) / (2 * signalData.cycle);
                                        totalExpectedWaitTimeSeconds += expectedWaitTime;

                                        // Worst case wait time: 実際の到着タイミングを厳密に考慮
                                        // 注意: これは期待値を参考にせず、実際の経路と到着タイミングから厳密に計算
                                        // 1. 累積移動時間を追跡（前の信号の待ち時間も含む）
                                        // 2. 信号の位相（phase）を考慮
                                        // 3. 到着時刻を信号サイクル内の位置に変換
                                        const arrivalTimeInCycle =
                                            (cumulativeTime -
                                                (signalData.phase || 0) +
                                                signalData.cycle) %
                                            signalData.cycle;

                                        let worstWaitTimeForThisSignal = 0;
                                        if (arrivalTimeInCycle > signalData.green) {
                                            // 赤信号中に到着した場合: 残りの赤信号時間を待つ
                                            worstWaitTimeForThisSignal =
                                                signalData.cycle - arrivalTimeInCycle;
                                        } else {
                                            // 青信号中に到着した場合でも、最悪ケースを考慮
                                            // 最悪ケース：次の赤信号の開始直前に到着した場合
                                            // その場合、次の赤信号時間全体を待つ必要がある
                                            worstWaitTimeForThisSignal = redTime;
                                        }

                                        totalWorstWaitTimeSeconds += worstWaitTimeForThisSignal;
                                        // 待ち時間を累積時間に加算（次の信号の計算に影響）
                                        // これにより、前の信号の待ち時間が次の信号の到着タイミングに影響する
                                        cumulativeTime += worstWaitTimeForThisSignal;
                                    }
                                }
                            }
                            // 横断歩道の待ち時間を計算（信号がない大通りの横断歩道、例: 60-209）
                            else if (edgeData.isCrosswalk) {
                                const crosswalkWaitTime = calculateCrosswalkWaitTime(
                                    edge,
                                    cumulativeTime
                                );
                                totalExpectedWaitTimeSeconds += crosswalkWaitTime;
                                totalWorstWaitTimeSeconds += crosswalkWaitTime;
                                // 待ち時間を累積時間に加算
                                cumulativeTime += crosswalkWaitTime;
                            }
                        }
                    }

                    const totalExpectedWaitTimeMinutes = totalExpectedWaitTimeSeconds / 60;
                    const totalWorstWaitTimeMinutes = totalWorstWaitTimeSeconds / 60;
                    const totalTimeWithExpected = totalTime + totalExpectedWaitTimeMinutes;
                    const totalTimeWithWorst = totalTime + totalWorstWaitTimeMinutes;

                    return {
                        totalDistance,
                        totalTime,
                        totalExpectedWaitTimeMinutes,
                        totalWorstWaitTimeMinutes,
                        totalTimeWithExpected,
                        totalTimeWithWorst,
                    };
                };

                // 1. Build graph structure from result.csv
                const resultCsv = fs.readFileSync('result.csv', 'utf8');
                const graph = new Map(); // node -> [{to, weight}, ...]

                resultCsv.split('\n').forEach((line) => {
                    if (line.trim() === '') return;
                    const cols = line.trim().split(',');
                    if (cols.length < 3) return;
                    const from = parseInt(cols[0], 10);
                    const to = parseInt(cols[1], 10);
                    const weight = parseFloat(cols[2]);

                    if (!isNaN(from) && !isNaN(to) && !isNaN(weight)) {
                        if (!graph.has(from)) {
                            graph.set(from, []);
                        }
                        graph.get(from).push({ to, weight });
                    }
                });

                console.log(`[グラフ構築] ${graph.size}個のノードからグラフを構築しました。`);

                // 2. Calculate shortest path cost first (for pruning)
                const startNodeInt = parseInt(startNode, 10);
                const endNodeInt = parseInt(endNode, 10);

                if (isNaN(startNodeInt) || isNaN(endNodeInt)) {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ error: 'Invalid start or end node' }));
                    return;
                }

                // Helper function to normalize edge key (smaller node first)
                const normalizeEdgeKey = (from, to) => {
                    return from < to ? `${from}-${to}` : `${to}-${from}`;
                };

                // Helper function to get time in seconds calculated from distance and walking speed
                const getEdgeTimeSeconds = (from, to) => {
                    const edgeKey = normalizeEdgeKey(from, to);
                    const edgeData = routeDataMap.get(edgeKey);
                    if (!edgeData) return Number.MAX_SAFE_INTEGER;

                    // 距離と歩行速度から時間を計算
                    // 勾配を考慮した歩行速度を計算（calculateRouteMetricsと同じ計算式）
                    const K = 0.5;
                    const adjustedSpeed = walkingSpeed * (1 - K * edgeData.gradient);
                    if (adjustedSpeed <= 0) return Number.MAX_SAFE_INTEGER;

                    // 時間（秒） = 距離（m） / 調整後の歩行速度（m/min） * 60
                    const timeSeconds = (edgeData.distance / adjustedSpeed) * 60;
                    return timeSeconds;
                };

                // Helper function to check if an edge is a signal
                const isSignalEdge = (from, to) => {
                    const edgeKey = normalizeEdgeKey(from, to);
                    const edgeData = routeDataMap.get(edgeKey);
                    return edgeData ? edgeData.isSignal : false;
                };

                // Helper function to check if an edge is a crosswalk
                const isCrosswalkEdge = (from, to) => {
                    const edgeKey = normalizeEdgeKey(from, to);
                    const edgeData = routeDataMap.get(edgeKey);
                    return edgeData ? edgeData.isCrosswalk : false;
                };

                // Helper function to calculate signal wait time
                const calculateSignalWaitTime = (edge, arrivalTimeSeconds) => {
                    const signalData = signalDataMap.get(edge);
                    if (!signalData || signalData.cycle <= 0) return 0;

                    const redTime = signalData.cycle - signalData.green;
                    if (redTime <= 0) return 0;

                    // 到着時刻を信号サイクル内の位置に変換
                    const arrivalTimeInCycle =
                        (arrivalTimeSeconds - (signalData.phase || 0) + signalData.cycle) %
                        signalData.cycle;

                    if (arrivalTimeInCycle > signalData.green) {
                        // 赤信号中に到着した場合: 残りの赤信号時間を待つ
                        return signalData.cycle - arrivalTimeInCycle;
                    } else {
                        // 青信号中に到着した場合: 期待待ち時間（ランダム到着を仮定）
                        return (redTime * redTime) / (2 * signalData.cycle);
                    }
                };

                // Helper function to calculate crosswalk wait time (for signal-less crosswalks on major roads)
                // 60-209のような信号のない大通りの横断歩道の待ち時間を計算
                const calculateCrosswalkWaitTime = (edge, arrivalTimeSeconds) => {
                    // 大通りの横断歩道では、車の通過を待つ時間を考慮
                    // 一般的に、大通りの車の通過間隔を考慮した期待待ち時間を計算
                    // 信号の待ち時間と同様に、ランダム到着を仮定

                    // 大通りの車の通過間隔を仮定（秒）
                    // 一般的な大通りでは、車の通過間隔は10-30秒程度
                    const averageCarInterval = 15; // 平均車通過間隔（秒）
                    const safeCrossingGap = 5; // 安全に横断できる間隔（秒）

                    // ランダム到着を仮定した期待待ち時間
                    // 信号の待ち時間の計算方法を参考に、平均待ち時間を計算
                    // 車が通過するまでの平均待ち時間 = (平均間隔 - 安全間隔) / 2
                    const expectedWaitTime = (averageCarInterval - safeCrossingGap) / 2;

                    return Math.max(0, expectedWaitTime);
                };

                // Dijkstra algorithm with signal wait time consideration
                const dijkstraWithSignals = (start, end, startTimeSeconds = 0) => {
                    const distances = new Map();
                    const previous = new Map();
                    const visited = new Set();
                    const arrivalTimes = new Map(); // 各ノードへの到着時刻（秒）
                    const INF = Number.MAX_SAFE_INTEGER;

                    // Initialize distances
                    for (const node of graph.keys()) {
                        distances.set(node, INF);
                        arrivalTimes.set(node, INF);
                    }
                    distances.set(start, 0);
                    arrivalTimes.set(start, startTimeSeconds);

                    // Linear search for minimum
                    while (true) {
                        let minNode = -1;
                        let minDist = INF;

                        // Find unvisited node with minimum distance
                        for (const node of graph.keys()) {
                            if (!visited.has(node) && distances.get(node) < minDist) {
                                minDist = distances.get(node);
                                minNode = node;
                            }
                        }

                        if (minNode === -1 || minNode === end) break; // No more nodes or found target
                        visited.add(minNode);

                        const currentArrivalTime = arrivalTimes.get(minNode);
                        const neighbors = graph.get(minNode) || [];
                        for (const neighbor of neighbors) {
                            if (!visited.has(neighbor.to)) {
                                const edgeTimeSeconds = getEdgeTimeSeconds(minNode, neighbor.to);
                                if (edgeTimeSeconds === Number.MAX_SAFE_INTEGER) continue;

                                // Normalize edge key
                                const edgeKey = normalizeEdgeKey(minNode, neighbor.to);
                                const isSignal = isSignalEdge(minNode, neighbor.to);
                                const isCrosswalk = isCrosswalkEdge(minNode, neighbor.to);

                                // 移動時間を加算
                                let newArrivalTime = currentArrivalTime + edgeTimeSeconds;
                                let waitTime = 0;

                                // 信号の待ち時間を計算
                                if (isSignal) {
                                    waitTime = calculateSignalWaitTime(edgeKey, newArrivalTime);
                                    newArrivalTime += waitTime;
                                }
                                // 横断歩道の待ち時間を計算（信号がない大通りの横断歩道、例: 60-209）
                                else if (isCrosswalk) {
                                    waitTime = calculateCrosswalkWaitTime(edgeKey, newArrivalTime);
                                    newArrivalTime += waitTime;
                                }

                                // 総コスト = 移動時間 + 待ち時間
                                const newCost = minDist + edgeTimeSeconds + waitTime;
                                const currentDist = distances.get(neighbor.to);
                                if (newCost < currentDist) {
                                    distances.set(neighbor.to, newCost);
                                    arrivalTimes.set(neighbor.to, newArrivalTime);
                                    previous.set(neighbor.to, minNode);
                                }
                            }
                        }
                    }

                    // Reconstruct path with normalized edge keys
                    const path = [];
                    let currentNode = end;
                    while (currentNode !== undefined && currentNode !== start) {
                        const prevNode = previous.get(currentNode);
                        if (prevNode === undefined) {
                            return { cost: INF, path: [], arrivalTime: INF };
                        }
                        // Normalize edge key (smaller node first) to match geojson file names
                        const edgeKey = normalizeEdgeKey(prevNode, currentNode);
                        path.unshift(edgeKey);
                        currentNode = prevNode;
                    }

                    return {
                        cost: distances.get(end),
                        path: path,
                        arrivalTime: arrivalTimes.get(end),
                    };
                };

                // 2. Identify all signal edges and signal nodes
                const signalEdges = [];
                const signalNodes = new Set();
                for (const [edgeKey, edgeData] of routeDataMap.entries()) {
                    if (edgeData.isSignal) {
                        signalEdges.push(edgeKey);
                        const [from, to] = edgeKey.split('-').map(Number);
                        signalNodes.add(from);
                        signalNodes.add(to);
                    }
                }
                console.log(
                    `[信号エッジ特定] ${signalEdges.length}個の信号エッジ、${signalNodes.size}個の信号ノードを特定しました。`
                );

                // 3. Find routes (both with and without signals) using Dijkstra
                const startTime = Date.now();
                console.log(`[経路探索開始] 開始時刻: ${new Date(startTime).toISOString()}`);

                const allRoutes = [];

                // Dijkstra algorithm that avoids specific edges (for Yen's algorithm)
                const dijkstraAvoidingEdges = (start, end, startTimeSeconds, forbiddenEdges) => {
                    // forbiddenEdges: Set of edge keys (e.g., "1-2") to avoid
                    const distances = new Map();
                    const previous = new Map();
                    const visited = new Set();
                    const arrivalTimes = new Map();
                    const INF = Number.MAX_SAFE_INTEGER;

                    // Initialize distances
                    for (const node of graph.keys()) {
                        distances.set(node, INF);
                        arrivalTimes.set(node, INF);
                    }
                    distances.set(start, 0);
                    arrivalTimes.set(start, startTimeSeconds);

                    // Linear search for minimum
                    while (true) {
                        let minNode = -1;
                        let minDist = INF;

                        // Find unvisited node with minimum distance
                        for (const node of graph.keys()) {
                            if (!visited.has(node) && distances.get(node) < minDist) {
                                minDist = distances.get(node);
                                minNode = node;
                            }
                        }

                        if (minNode === -1 || minNode === end) break;
                        visited.add(minNode);

                        const currentArrivalTime = arrivalTimes.get(minNode);
                        const neighbors = graph.get(minNode) || [];
                        for (const neighbor of neighbors) {
                            if (!visited.has(neighbor.to)) {
                                // Check if this edge is forbidden
                                const edgeKey = normalizeEdgeKey(minNode, neighbor.to);
                                if (forbiddenEdges.has(edgeKey)) continue;

                                const edgeTimeSeconds = getEdgeTimeSeconds(minNode, neighbor.to);
                                if (edgeTimeSeconds === Number.MAX_SAFE_INTEGER) continue;

                                const isSignal = isSignalEdge(minNode, neighbor.to);
                                const isCrosswalk = isCrosswalkEdge(minNode, neighbor.to);

                                let newArrivalTime = currentArrivalTime + edgeTimeSeconds;
                                let waitTime = 0;

                                if (isSignal) {
                                    waitTime = calculateSignalWaitTime(edgeKey, newArrivalTime);
                                    newArrivalTime += waitTime;
                                } else if (isCrosswalk) {
                                    waitTime = calculateCrosswalkWaitTime(edgeKey, newArrivalTime);
                                    newArrivalTime += waitTime;
                                }

                                const newCost = minDist + edgeTimeSeconds + waitTime;
                                const currentDist = distances.get(neighbor.to);
                                if (newCost < currentDist) {
                                    distances.set(neighbor.to, newCost);
                                    arrivalTimes.set(neighbor.to, newArrivalTime);
                                    previous.set(neighbor.to, minNode);
                                }
                            }
                        }
                    }

                    // Reconstruct path
                    const path = [];
                    let currentNode = end;
                    while (currentNode !== undefined && currentNode !== start) {
                        const prevNode = previous.get(currentNode);
                        if (prevNode === undefined) {
                            return { cost: INF, path: [], arrivalTime: INF };
                        }
                        const edgeKey = normalizeEdgeKey(prevNode, currentNode);
                        path.unshift(edgeKey);
                        currentNode = prevNode;
                    }

                    return {
                        cost: distances.get(end),
                        path: path,
                        arrivalTime: arrivalTimes.get(end),
                    };
                };

                // Helper function to convert edge path to node path
                const edgePathToNodePath = (edgePath, startNode) => {
                    if (edgePath.length === 0) return [startNode];
                    const nodePath = [startNode];
                    let currentNode = startNode;
                    for (const edge of edgePath) {
                        const [from, to] = edge.split('-').map(Number);
                        if (from === currentNode) {
                            currentNode = to;
                            nodePath.push(to);
                        } else if (to === currentNode) {
                            currentNode = from;
                            nodePath.push(from);
                        } else {
                            // Edge doesn't connect, this shouldn't happen in a valid path
                            console.warn(
                                `[警告] エッジ${edge}が現在のノード${currentNode}に接続していません`
                            );
                            break;
                        }
                    }
                    return nodePath;
                };

                // Yen's algorithm for finding K shortest paths
                const yensAlgorithm = (start, end, basePath, k = 5) => {
                    // basePath: The base path (array of edge keys) to start from
                    // k: Number of shortest paths to find
                    const shortestPaths = [];
                    const candidates = [];

                    // Add base path as the first shortest path
                    if (basePath && basePath.length > 0) {
                        const baseMetrics = calculateRouteMetrics(basePath);
                        shortestPaths.push({
                            path: [...basePath],
                            cost: baseMetrics.totalTimeWithExpected * 60,
                            metrics: baseMetrics,
                        });
                    }

                    // Find k-1 more paths
                    for (let i = 1; i < k; i++) {
                        const previousPath = shortestPaths[i - 1].path;
                        if (previousPath.length === 0) break;

                        // Convert edge path to node path for easier manipulation
                        const nodePath = edgePathToNodePath(previousPath, start);

                        // For each node in the previous path (except the last), try to find an alternative path
                        for (let j = 0; j < nodePath.length - 1; j++) {
                            const spurNode = nodePath[j];
                            const rootPathEdges = [];

                            // Build root path (edges from start to spur node)
                            for (let m = 0; m < j; m++) {
                                rootPathEdges.push(previousPath[m]);
                            }

                            // Calculate arrival time at spur node
                            let arrivalTimeAtSpur = 0;
                            if (rootPathEdges.length > 0) {
                                const rootMetrics = calculateRouteMetrics(rootPathEdges);
                                arrivalTimeAtSpur = rootMetrics.totalTimeWithExpected * 60;
                            }

                            // Forbid edges in root path
                            const forbiddenEdges = new Set();
                            for (const edge of rootPathEdges) {
                                forbiddenEdges.add(edge);
                            }

                            // Forbid the edge from spur node in the previous path
                            if (j < previousPath.length) {
                                forbiddenEdges.add(previousPath[j]);
                            }

                            // Forbid edges that appear in all previous shortest paths at the same position
                            for (const prevPath of shortestPaths) {
                                if (prevPath.path.length > j) {
                                    forbiddenEdges.add(prevPath.path[j]);
                                }
                            }

                            // Find spur path (from spur node to end, avoiding forbidden edges)
                            const spurPath = dijkstraAvoidingEdges(
                                spurNode,
                                end,
                                arrivalTimeAtSpur,
                                forbiddenEdges
                            );

                            if (
                                spurPath.cost < Number.MAX_SAFE_INTEGER &&
                                spurPath.path.length > 0
                            ) {
                                // Combine root path and spur path
                                const candidatePath = [...rootPathEdges, ...spurPath.path];
                                // Remove duplicates while preserving order
                                const uniqueEdges = [];
                                const seen = new Set();
                                for (const edge of candidatePath) {
                                    if (!seen.has(edge)) {
                                        seen.add(edge);
                                        uniqueEdges.push(edge);
                                    }
                                }

                                // Check if this candidate is already in shortestPaths or candidates
                                const candidateKey = uniqueEdges.sort().join(',');
                                const isDuplicate =
                                    shortestPaths.some((p) => {
                                        const pKey = p.path.slice().sort().join(',');
                                        return pKey === candidateKey;
                                    }) ||
                                    candidates.some((c) => {
                                        const cKey = c.path.slice().sort().join(',');
                                        return cKey === candidateKey;
                                    });

                                if (!isDuplicate) {
                                    // Calculate metrics for candidate path
                                    const candidateMetrics = calculateRouteMetrics(uniqueEdges);
                                    candidates.push({
                                        path: uniqueEdges,
                                        cost: candidateMetrics.totalTimeWithExpected * 60,
                                        metrics: candidateMetrics,
                                    });
                                }
                            }
                        }

                        // If no candidates found, break
                        if (candidates.length === 0) break;

                        // Sort candidates by cost and select the shortest one
                        candidates.sort((a, b) => {
                            // First compare by distance
                            const distanceDiff = a.metrics.totalDistance - b.metrics.totalDistance;
                            if (Math.abs(distanceDiff) > 0.1) {
                                return distanceDiff;
                            }
                            // Then by total time
                            return a.cost - b.cost;
                        });

                        // Add the shortest candidate to shortestPaths
                        shortestPaths.push(candidates.shift());
                    }

                    return shortestPaths;
                };

                // First, find shortest path (may include signals)
                const shortestPath = dijkstraWithSignals(startNodeInt, endNodeInt, 0);
                let basePathForYen = null;
                let basePathMetrics = null;

                if (shortestPath.cost < Number.MAX_SAFE_INTEGER && shortestPath.path.length > 0) {
                    // Calculate metrics for shortest path (strict recalculation)
                    basePathMetrics = calculateRouteMetrics(shortestPath.path);
                    basePathForYen = shortestPath.path;

                    console.log(
                        `[基準経路] 厳密に再計算した基準経路を発見しました（総推定時間: ${basePathMetrics.totalTimeWithExpected.toFixed(
                            2
                        )}分, 距離: ${basePathMetrics.totalDistance.toFixed(2)}m）`
                    );
                } else {
                    // No path found
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify([]));
                    return;
                }

                // Use Yen's algorithm to find K shortest paths based on the base path
                console.log(`[イェンのアルゴリズム] K最短経路を探索中...`);
                const yenStartTime = Date.now();
                const kShortestPaths = yensAlgorithm(startNodeInt, endNodeInt, basePathForYen, 5);
                const yenTime = Date.now() - yenStartTime;
                console.log(
                    `[イェンのアルゴリズム完了] ${kShortestPaths.length}件の経路を発見しました（${(
                        yenTime / 1000
                    ).toFixed(2)}秒）`
                );

                if (kShortestPaths.length === 0) {
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify([]));
                    return;
                }

                // Convert to response format
                const top5Routes = kShortestPaths.map((route) => ({
                    userPref: route.path.map((e) => `${e}.geojson`).join('\n'),
                    totalDistance: route.metrics.totalDistance,
                    totalTime: route.metrics.totalTimeWithExpected,
                    totalWaitTime: route.metrics.totalExpectedWaitTimeMinutes,
                }));

                const totalTime = Date.now() - startTime;
                console.log(
                    `[最終結果] イェンのアルゴリズム: ${kShortestPaths.length}件の経路を発見`
                );
                console.log(`[最終結果] 上位${top5Routes.length}件の経路を選択`);
                top5Routes.forEach((route, index) => {
                    console.log(
                        `[経路${index + 1}] 総推定時間: ${route.totalTime.toFixed(
                            2
                        )}分, 距離: ${route.totalDistance.toFixed(
                            2
                        )}m, 待ち時間: ${route.totalWaitTime.toFixed(2)}分`
                    );
                });
                console.log(
                    `[総処理時間] ${(totalTime / 1000).toFixed(2)}秒（${(totalTime / 60000).toFixed(
                        2
                    )}分）`
                );
                console.log(
                    `[処理時間内訳] イェンのアルゴリズム: ${(yenTime / 1000).toFixed(2)}秒`
                );

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify(top5Routes));
            } catch (err) {
                console.error(`実行エラー詳細: ${err.message}`);
                console.error(`stderr: ${err.stderr}`);
                console.error(`stdout: ${err.stdout}`);
                console.error(`status: ${err.status}`);
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(
                    JSON.stringify({ error: err.message, stderr: err.stderr, stdout: err.stdout })
                );
            }
        });
        return;
    }

    /* ===== 特定の信号までの待ち時間計算(ユーザー指定ロジック) ===== */
    if (req.url === '/calculate-wait-time' && req.method === 'POST') {
        let body = '';
        req.on('data', (chunk) => (body += chunk));
        req.on('end', () => {
            console.log('\n--- [/calculate-wait-time]リクエスト受信 ---');
            try {
                console.log('Request body:', body);
                const { referenceEdge, walkingSpeed } = JSON.parse(body);
                if (!referenceEdge || !walkingSpeed) {
                    throw new Error('referenceEdgeとwalkingSpeedは必須です。');
                }
                console.log(
                    `[OK] パラメータ取得: referenceEdge=${referenceEdge}, walkingSpeed=${walkingSpeed}`
                );

                // Robustness checks
                if (!fs.existsSync('signal_inf.csv'))
                    throw new Error('サーバーに signal_inf.csv が見つかりません。');
                if (!fs.existsSync('result2.txt'))
                    throw new Error('result2.txt が見つかりません。先に経路を計算してください。');
                console.log('[OK] 必須ファイルの存在を確認');

                // 1. Read data files
                const routeInfoCsv = fs.readFileSync('oomiya_route_inf_4.csv', 'utf8');
                const signalInfoCsv = fs.readFileSync('signal_inf.csv', 'utf8');
                const routeResultTxt = fs.readFileSync('result2.txt', 'utf8');
                console.log('[OK] データファイルの読み込み完了');

                if (signalInfoCsv.trim() === '') throw new Error('signal_inf.csv が空です。');
                if (routeResultTxt.trim() === '') throw new Error('result2.txt が空です。');
                console.log('[OK] ファイルが空でないことを確認');

                // 2. Create lookup maps
                const routeDataMap = new Map();
                routeInfoCsv
                    .split('\n')
                    .slice(1)
                    .forEach((line) => {
                        if (line.trim() === '') return;
                        const cols = line.trim().split(',');
                        routeDataMap.set(`${cols[0]}-${cols[1]}`, {
                            distance: parseFloat(cols[2]),
                            gradient: parseFloat(cols[4]),
                            isSignal: cols[8] === '1',
                        });
                    });

                const signalDataMap = new Map();
                signalInfoCsv
                    .split('\n')
                    .slice(1)
                    .forEach((line) => {
                        if (line.trim() === '') return;
                        const cols = line.trim().split(',');
                        signalDataMap.set(`${cols[0]}-${cols[1]}`, {
                            cycle: parseInt(cols[2], 10),
                            green: parseInt(cols[3], 10),
                            phase: parseInt(cols[4], 10),
                        });
                    });
                console.log('[OK] データマップの作成完了');

                const routeEdges = routeResultTxt
                    .split('\n')
                    .filter((l) => l.trim() !== '')
                    .map((l) => l.replace('.geojson', ''));
                const signalizedRouteEdges = routeEdges.filter(
                    (edge) => routeDataMap.get(edge)?.isSignal
                );

                // 3. Get reference phase
                const referenceSignalData = signalDataMap.get(referenceEdge);
                if (!referenceSignalData) {
                    throw new Error(
                        `基準信号 ${referenceEdge} が signal_inf.csv に見つかりません。`
                    );
                }
                const referencePhase = referenceSignalData.phase;
                console.log(`[OK] 基準位相を取得: ${referencePhase}`);

                // 4. Simulate and calculate wait time
                console.log('シミュレーションを開始...');
                let totalWaitTime = 0;
                let cumulativeTime = 0;
                const K = 0.5;

                for (const edge of routeEdges) {
                    const edgeData = routeDataMap.get(edge);
                    if (!edgeData) continue;

                    const adjustedSpeed = walkingSpeed * (1 - K * edgeData.gradient);
                    let travelTime = 0;
                    if (adjustedSpeed > 0) {
                        travelTime = edgeData.distance / adjustedSpeed;
                    }
                    cumulativeTime += travelTime;

                    if (signalizedRouteEdges.includes(edge)) {
                        const signalData = signalDataMap.get(edge);
                        if (signalData) {
                            const phaseDiff = Math.abs(signalData.phase - referencePhase);
                            const arrivalTime = cumulativeTime;
                            const timeIntoCycle =
                                (arrivalTime - phaseDiff + signalData.cycle) % signalData.cycle;

                            if (timeIntoCycle > signalData.green) {
                                const waitTime = signalData.cycle - timeIntoCycle;
                                totalWaitTime += waitTime;
                                cumulativeTime += waitTime;
                            }
                        }
                    }
                }
                console.log(`[OK] シミュレーション完了。総待ち時間: ${totalWaitTime}秒`);

                const responsePayload = { totalWaitTime: totalWaitTime / 60 };
                console.log('成功レスポンスを送信:', JSON.stringify(responsePayload));
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify(responsePayload));
            } catch (err) {
                console.error('!!! /calculate-wait-time でエラー発生 !!!');
                console.error('エラーメッセージ:', err.message);
                console.error('スタックトレース:', err.stack);
                const errorPayload = { error: err.message };
                console.log('エラーレスポンスを送信:', JSON.stringify(errorPayload));
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify(errorPayload));
            }
        });
        return;
    }

    /* ===== 信号待ち時間計算用のエンドポイント ===== */
    if (req.url === '/calculate-wait-time' && req.method === 'POST') {
        let body = '';
        req.on('data', (chunk) => (body += chunk));
        req.on('end', () => {
            try {
                const { startTime = 0 } = JSON.parse(body);

                // 1. 必要なデータを読み込み
                const routeInfoCsv = fs.readFileSync('oomiya_route_inf_4.csv', 'utf8');
                const signalInfoCsv = fs.readFileSync('signal_inf.csv', 'utf8');
                const routeResultTxt = fs.readFileSync('result2.txt', 'utf8');

                // 2. Process data into efficient lookup maps
                const routeDataMap = new Map();
                const routeInfoLines = routeInfoCsv.split('\n').slice(1); // skip header
                for (const line of routeInfoLines) {
                    if (line.trim() === '') continue;
                    const cols = line.trim().split(',');
                    const timeInMinutes = parseFloat(cols[3]);
                    const isSignal = cols[8] === '1';
                    routeDataMap.set(`${cols[0]}-${cols[1]}`, { time: timeInMinutes, isSignal });
                }

                const signalDataMap = new Map();
                const signalInfoLines = signalInfoCsv.split('\n');
                for (const line of signalInfoLines) {
                    if (line.trim() === '') continue;
                    const cols = line.trim().split(',');
                    signalDataMap.set(`${cols[0]}-${cols[1]}`, {
                        cycle: parseInt(cols[2], 10),
                        green: parseInt(cols[3], 10),
                        phase: parseInt(cols[4], 10),
                    });
                }

                // 3. 計算REを取得
                const routeEdges = routeResultTxt
                    .split('\n')
                    .filter((l) => l.trim() !== '')
                    .map((l) => l.replace('.geojson', ''));

                // 4. 総待ち時間を計算
                let totalWaitTime = 0;
                let cumulativeTime = startTime * 60;

                for (const edge of routeEdges) {
                    const edgeData = routeDataMap.get(edge);
                    if (!edgeData) continue;

                    cumulativeTime += edgeData.time * 60;

                    if (edgeData.isSignal) {
                        const signalData = signalDataMap.get(edge);
                        if (signalData) {
                            const { cycle, green, phase } = signalData;
                            const arrivalTime = cumulativeTime;
                            const timeIntoCycle = (arrivalTime - phase + cycle) % cycle;

                            if (timeIntoCycle > green) {
                                const waitTime = cycle - timeIntoCycle;
                                totalWaitTime += waitTime;
                                cumulativeTime += waitTime;
                            }
                        }
                    }
                }

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ totalWaitTime: totalWaitTime / 60 }));
            } catch (err) {
                console.error(`Wait time calculation error: ${err.message}`);
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
        });
        return;
    }

    /* ===== ④ /csv-data への GET - CSVデータを返す ===== */
    if (req.url === '/csv-data' && req.method === 'GET') {
        try {
            const csvPath = path.join(__dirname, 'oomiya_route_inf_4.csv');
            const csvData = fs.readFileSync(csvPath, 'utf8');
            res.writeHead(200, { 'Content-Type': 'text/csv; charset=utf-8' });
            res.end(csvData);
        } catch (err) {
            res.writeHead(500, { 'Content-Type': 'text/plain' });
            res.end(`CSV Error: ${err.message}`);
        }
        return;
    }

    /* ===== ⑤ /route-analysis への POST - ルート解析 ===== */
    if (req.url === '/route-analysis' && req.method === 'POST') {
        let body = '';
        req.on('data', (chunk) => (body += chunk));
        req.on('end', () => {
            try {
                const requestData = JSON.parse(body);
                const { startNode, endNode, weights } = requestData;

                const csvPath = path.join(__dirname, 'oomiya_route_inf_4.csv');
                const csvData = fs.readFileSync(csvPath, 'utf8');

                const routes = analyzeRoutes(csvData, startNode, endNode, weights);

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: true, routes: routes }));
            } catch (err) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: false, error: err.message }));
            }
        });
        return;
    }

    /* ===== /list_saved_routes への GET ===== */
    if (req.url === '/list_saved_routes' && req.method === 'GET') {
        const saveDir = path.join(__dirname, 'saving_route');
        if (!fs.existsSync(saveDir)) {
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify([])); // ディレクトリがなければ空配列を返す
            return;
        }
        fs.readdir(saveDir, (err, files) => {
            if (err) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Failed to list files' }));
                return;
            }
            const csvFiles = files.filter((file) => file.toLowerCase().endsWith('.csv'));
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(csvFiles));
        });
        return;
    }

    /* ===== /get_saved_route への GET ===== */
    if (req.url.startsWith('/get_saved_route') && req.method === 'GET') {
        const url = new URL(req.url, `http://${req.headers.host}`);
        const fileName = url.searchParams.get('fileName');
        if (!fileName) {
            res.writeHead(400, { 'Content-Type': 'text/plain' });
            res.end('File name is required');
            return;
        }

        const filePath = path.join(__dirname, 'saving_route', fileName);
        if (fs.existsSync(filePath)) {
            fs.readFile(filePath, 'utf8', (err, data) => {
                if (err) {
                    res.writeHead(500, { 'Content-Type': 'text/plain' });
                    res.end('Error reading file');
                    return;
                }
                res.writeHead(200, { 'Content-Type': 'text/csv; charset=utf-8' });
                res.end(data);
            });
        } else {
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            res.end('File not found');
        }
        return;
    }

    /* ===== /save_route への POST ===== */
    if (req.url.startsWith('/save_route') && req.method === 'POST') {
        let body = '';
        req.on('data', (chunk) => (body += chunk));
        req.on('end', () => {
            try {
                let { fileName, csvContent, overwrite } = JSON.parse(body);

                if (!fileName.toLowerCase().endsWith('.csv')) {
                    fileName += '.csv';
                }

                const saveDir = path.join(__dirname, 'saving_route');
                const filePath = path.join(saveDir, fileName);

                if (!fs.existsSync(saveDir)) {
                    fs.mkdirSync(saveDir, { recursive: true });
                }

                if (fs.existsSync(filePath) && !overwrite) {
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: 'exists', finalFileName: fileName }));
                } else {
                    fs.writeFileSync(filePath, csvContent, 'utf8');
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: 'success', finalFileName: fileName }));
                }
            } catch (err) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: 'error', message: err.message }));
            }
        });
        return;
    }

    /* ===== 通常ファイルの配信 ===== */
    const filePath = path.join(__dirname, req.url === '/' ? 'min_map_ver4.0.html' : req.url);

    const actualFilePath = fs.existsSync(filePath)
        ? filePath
        : path.join(__dirname, req.url === '/' ? 'min_map_ver4.0.html' : req.url);

    fs.readFile(actualFilePath, (err, data) => {
        if (err) {
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            return res.end('Not Found');
        }
        // Add cache-control headers for HTML files
        if (actualFilePath.endsWith('.html')) {
            res.setHeader('Cache-Control', 'no-cache, no-store, must-revalidate');
            res.setHeader('Pragma', 'no-cache');
            res.setHeader('Expires', '0');
        }
        res.writeHead(200, {
            'Content-Type': mime.lookup(actualFilePath) || 'application/octet-stream',
        });
        res.end(data);
    });
});

// ルート解析関数（サンプル実装）
const analyzeRoutes = (csvData, startNode, endNode, weights) => {
    const lines = csvData.split('\n');
    const headers = lines[0].split(',');
    const routes = [];

    for (let i = 1; i < lines.length; i++) {
        const line = lines[i].trim();
        if (!line) continue;

        const values = line.split(',');
        const route = {};

        headers.forEach((header, index) => {
            route[header.trim()] = values[index];
        });

        if (
            route.node1 == startNode ||
            route.node2 == startNode ||
            route.node1 == endNode ||
            route.node2 == endNode
        ) {
            routes.push(route);
        }
    }

    return routes;
};

// 勾配を考慮した移動時間を計算する関数
const calcTravelTime = () => {};

/* ===== ポート 8081 で待ち受け ===== */
server.listen(8081, () => {
    console.log('Server is running at http://localhost:8081');
});
