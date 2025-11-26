/* 網羅的経路探索（信号待ち時間考慮）- 条件に当てはまる経路を全て出力 */

#define _GNU_SOURCE  // M_PIを使用するため
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846  // C99標準でM_PIが定義されていない場合のフォールバック
#endif

#define MAX_NODES 300
#define MAX_EDGES 1000
#define MAX_PATH_LENGTH 200
#define MAX_K 5
#define MAX_SIGNALS 50  // 信号エッジの最大数（signal_inf.csvには37個あるため余裕を持たせる）
#define MAX_COMBINATIONS 1024 // 経路数の上限を増加（2^11 = 2048）
#define INF DBL_MAX
#define K_GRADIENT 0.5
#define AVERAGE_CAR_INTERVAL 15.0
#define SAFE_CROSSING_GAP 5.0
#define MAX_FILENAME_LENGTH 256
#define ANGLE_TOLERANCE 45.0  // 方向フィルターの角度許容範囲（度、ゴール方向を0度として左右±45度）
#define DETOUR_THRESHOLD 1.3  // 遠回り判定の閾値（最短経路の1.3倍以上を遠回りと判定）- 1.5から1.3に強化
#define MAX_SIGNALS_PER_COMBINATION 4  // 組み合わせあたりの最大信号数（フィルター4）
#define MAX_TOTAL_EXPECTED_WAIT_TIME 90.0  // 最大合計待ち時間（秒、フィルター5）
#define MIN_SIGNAL_DISTANCE 100.0  // 信号間の最小距離（メートル、フィルター6）

// エッジ情報
typedef struct {
    int from;
    int to;
    double distance;
    double gradient;
    int isSignal;
    int isCrosswalk;
    int signalCycle;
    int signalGreen;
    double signalPhase;
    double expectedWaitTime;  // 各信号ごとの期待値（秒）。CSVから読み込む
} EdgeData;

// グラフ構造
typedef struct {
    int node;
    int edgeIndex; // EdgeData配列のインデックス
} GraphEdge;

typedef struct {
    GraphEdge edges[7];
    int edge_count;
} GraphNode;

// 経路情報
typedef struct {
    int edges[MAX_PATH_LENGTH];
    int edgeCount;
    double totalDistance;
    double totalTime;
    double totalWaitTime;
    double totalTimeWithExpected;
} Path;

// ノード座標情報
typedef struct {
    double longitude;  // 経度
    double latitude;   // 緯度
    bool hasCoordinates;  // 座標が読み込まれているか
} NodeCoordinates;

// グローバル変数
GraphNode graph[MAX_NODES];
EdgeData edgeDataArray[MAX_EDGES];
int edgeDataCount = 0;
double walkingSpeed = 80.0; // m/min
int signalEdges[MAX_SIGNALS];  // 信号エッジのインデックスリスト
int signalCount = 0;  // 信号の数
NodeCoordinates nodeCoords[MAX_NODES];  // ノード座標
int startNode = 0;  // スタートノード（フィルター用）
int endNode = 0;  // エンドノード（フィルター用）

// 大きな配列をグローバル変数に移動（スタックオーバーフロー防止）
typedef struct {
    int pathLength;
    int edges[MAX_PATH_LENGTH];
} PathSet;

Path globalPaths[MAX_COMBINATIONS];  // 経路配列（グローバル）
PathSet globalSeenPathSets[MAX_COMBINATIONS];  // 重複チェック用（グローバル）

// グラフを初期化
void initGraph() {
    for (int i = 0; i < MAX_NODES; i++) {
        graph[i].edge_count = 0;
    }
}

// エッジキーを正規化（小さいノードを先に）
void normalizeEdgeKey(int from, int to, int *outFrom, int *outTo) {
    if (from < to) {
        *outFrom = from;
        *outTo = to;
    } else {
        *outFrom = to;
        *outTo = from;
    }
}

// エッジキーからインデックスを取得
int findEdgeIndex(int from, int to) {
    int normFrom, normTo;
    normalizeEdgeKey(from, to, &normFrom, &normTo);
    
    for (int i = 0; i < edgeDataCount; i++) {
        int edgeFrom, edgeTo;
        normalizeEdgeKey(edgeDataArray[i].from, edgeDataArray[i].to, &edgeFrom, &edgeTo);
        if (edgeFrom == normFrom && edgeTo == normTo) {
            return i;
        }
    }
    return -1;
}

// エッジの移動時間を計算（秒）
double getEdgeTimeSeconds(int from, int to) {
    int edgeIdx = findEdgeIndex(from, to);
    if (edgeIdx < 0) return INF;
    
    EdgeData *edge = &edgeDataArray[edgeIdx];
    double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * edge->gradient);
    if (adjustedSpeed <= 0) return INF;
    
    return (edge->distance / adjustedSpeed) * 60.0;
}

// 信号待ち時間の期待値を取得（秒）- CSVのexpectedカラムから直接使用
double calculateSignalWaitTimeExpected(int edgeIdx) {
    EdgeData *edge = &edgeDataArray[edgeIdx];
    if (!edge->isSignal || edge->signalCycle <= 0) return 0.0;
    
    // CSVから読み込んだ期待値を直接返す
    return edge->expectedWaitTime >= 0.0 ? edge->expectedWaitTime : 0.0;
}

// 信号待ち時間を厳密に計算（秒）- 実際の到着時刻とCSVのphase、cycle、greenを使用
// arrivalTimeSeconds: 信号への到着時刻（秒）
// logSignalInfo: trueの場合、ログを出力する
double calculateSignalWaitTime(int edgeIdx, double arrivalTimeSeconds, bool logSignalInfo) {
    EdgeData *edge = &edgeDataArray[edgeIdx];
    if (!edge->isSignal || edge->signalCycle <= 0) return 0.0;
    
    // 信号サイクル内での到着時刻の位置を計算
    double cycleTime = (double)edge->signalCycle;
    double arrivalPhase = fmod(arrivalTimeSeconds, cycleTime);
    double actualWaitTime = 0.0;
    bool isGreen = false;
    
    // 信号の位相を考慮して待ち時間を計算
    // phase: 青信号の開始時刻（サイクル内での位置）
    // green: 青信号の継続時間
    if (arrivalPhase < edge->signalPhase) {
        // 青信号の開始前：青信号開始まで待つ
        actualWaitTime = edge->signalPhase - arrivalPhase;
    } else if (arrivalPhase >= edge->signalPhase + (double)edge->signalGreen) {
        // 青信号の終了後：次のサイクルの青信号開始まで待つ
        actualWaitTime = cycleTime - arrivalPhase + edge->signalPhase;
    } else {
        // 青信号中：待ち時間なし
        actualWaitTime = 0.0;
        isGreen = true;
    }
    
    // ログ出力
    if (logSignalInfo) {
        if (isGreen) {
            fprintf(stderr, "Signal arrival: edgeIdx=%d (%d-%d), arrivalTime=%.2fs, arrivalPhase=%.2fs, waitTime=0.00s (GREEN)\n",
                    edgeIdx, edge->from, edge->to, arrivalTimeSeconds, arrivalPhase);
        } else {
            fprintf(stderr, "Signal arrival: edgeIdx=%d (%d-%d), arrivalTime=%.2fs, arrivalPhase=%.2fs, waitTime=%.2fs\n",
                    edgeIdx, edge->from, edge->to, arrivalTimeSeconds, arrivalPhase, actualWaitTime);
        }
    }
    
    return actualWaitTime;
}

// 横断歩道待ち時間を計算（秒）
double calculateCrosswalkWaitTime(int edgeIdx, double arrivalTimeSeconds) {
    EdgeData *edge = &edgeDataArray[edgeIdx];
    if (!edge->isCrosswalk) return 0.0;
    
    double expectedWaitTime = (AVERAGE_CAR_INTERVAL - SAFE_CROSSING_GAP) / 2.0;
    return expectedWaitTime > 0 ? expectedWaitTime : 0.0;
}

// ダイクストラ法（信号待ち時間考慮、禁止エッジあり）
typedef struct {
    double cost;
    int path[MAX_PATH_LENGTH];
    int pathLength;
    double arrivalTime;
} DijkstraResult;

// ダイクストラ法（信号待ち時間を考慮しない版 - セグメント間の最短経路用）
DijkstraResult dijkstraWithoutSignalWait(int start, int end, bool *forbiddenEdges) {
    double distances[MAX_NODES];
    int previous[MAX_NODES];
    bool visited[MAX_NODES];
    DijkstraResult result;
    
    // 初期化
    for (int i = 0; i < MAX_NODES; i++) {
        distances[i] = INF;
        previous[i] = -1;
        visited[i] = false;
    }
    distances[start] = 0.0;
    
    // ダイクストラ法のメインループ
    while (true) {
        int minNode = -1;
        double minDist = INF;
        
        // 未訪問ノードで最小距離のものを探す
        for (int i = 0; i < MAX_NODES; i++) {
            if (!visited[i] && distances[i] < minDist) {
                minDist = distances[i];
                minNode = i;
            }
        }
        
        if (minNode == -1 || minNode == end) break;
        visited[minNode] = true;
        
        // 隣接ノードを更新
        for (int i = 0; i < graph[minNode].edge_count; i++) {
            int neighbor = graph[minNode].edges[i].node;
            if (visited[neighbor]) continue;
            
            int edgeIdx = graph[minNode].edges[i].edgeIndex;
            if (forbiddenEdges && forbiddenEdges[edgeIdx]) continue;
            
            double edgeTime = getEdgeTimeSeconds(minNode, neighbor);
            if (edgeTime >= INF) continue;
            
            // 信号待ち時間は考慮しない（セグメント間の最短経路用）
            double newCost = minDist + edgeTime;
            if (newCost < distances[neighbor]) {
                distances[neighbor] = newCost;
                previous[neighbor] = minNode;
            }
        }
    }
    
    // 経路を再構築
    result.cost = distances[end];
    result.arrivalTime = distances[end];
    result.pathLength = 0;
    
    if (distances[end] < INF) {
        int currentNode = end;
        int pathNodes[MAX_PATH_LENGTH];
        int pathNodeCount = 0;
        
        while (currentNode != -1 && currentNode != start) {
            pathNodes[pathNodeCount++] = currentNode;
            currentNode = previous[currentNode];
            if (currentNode == -1) {
                result.cost = INF;
                result.pathLength = 0;
                return result;
            }
        }
        pathNodes[pathNodeCount++] = start;
        
        // エッジに変換
        for (int i = pathNodeCount - 1; i > 0; i--) {
            int from = pathNodes[i];
            int to = pathNodes[i - 1];
            int edgeIdx = findEdgeIndex(from, to);
            if (edgeIdx >= 0) {
                result.path[result.pathLength++] = edgeIdx;
            }
        }
    }
    
    return result;
}

DijkstraResult dijkstraWithSignals(int start, int end, double startTimeSeconds, 
                                    bool *forbiddenEdges) {
    double distances[MAX_NODES];
    int previous[MAX_NODES];
    bool visited[MAX_NODES];
    double arrivalTimes[MAX_NODES];
    DijkstraResult result;
    
    // 初期化
    for (int i = 0; i < MAX_NODES; i++) {
        distances[i] = INF;
        previous[i] = -1;
        visited[i] = false;
        arrivalTimes[i] = INF;
    }
    distances[start] = 0.0;
    arrivalTimes[start] = startTimeSeconds;
    
    // ダイクストラ法のメインループ
    while (true) {
        int minNode = -1;
        double minDist = INF;
        
        // 未訪問ノードで最小距離のものを探す
        for (int i = 0; i < MAX_NODES; i++) {
            if (!visited[i] && distances[i] < minDist) {
                minDist = distances[i];
                minNode = i;
            }
        }
        
        if (minNode == -1 || minNode == end) break;
        visited[minNode] = true;
        
        // 隣接ノードを更新
        for (int i = 0; i < graph[minNode].edge_count; i++) {
            int neighbor = graph[minNode].edges[i].node;
            if (visited[neighbor]) continue;
            
            int edgeIdx = graph[minNode].edges[i].edgeIndex;
            if (forbiddenEdges && forbiddenEdges[edgeIdx]) continue;
            
            double edgeTime = getEdgeTimeSeconds(minNode, neighbor);
            if (edgeTime >= INF) continue;
            
            double newArrivalTime = arrivalTimes[minNode] + edgeTime;
            double waitTime = 0.0;
            
            EdgeData *edge = &edgeDataArray[edgeIdx];
            if (edge->isSignal) {
                waitTime = calculateSignalWaitTime(edgeIdx, newArrivalTime, false);  // ダイクストラ法内ではログ出力しない
                newArrivalTime += waitTime;
            } else if (edge->isCrosswalk) {
                waitTime = calculateCrosswalkWaitTime(edgeIdx, newArrivalTime);
                newArrivalTime += waitTime;
            }
            
            double newCost = minDist + edgeTime + waitTime;
            if (newCost < distances[neighbor]) {
                distances[neighbor] = newCost;
                arrivalTimes[neighbor] = newArrivalTime;
                previous[neighbor] = minNode;
            }
        }
    }
    
    // 経路を再構築
    result.cost = distances[end];
    result.arrivalTime = arrivalTimes[end];
    result.pathLength = 0;
    
    if (distances[end] < INF) {
        int currentNode = end;
        int pathNodes[MAX_PATH_LENGTH];
        int pathNodeCount = 0;
        
        while (currentNode != -1 && currentNode != start) {
            pathNodes[pathNodeCount++] = currentNode;
            currentNode = previous[currentNode];
            if (currentNode == -1) {
                result.cost = INF;
                result.pathLength = 0;
                return result;
            }
        }
        pathNodes[pathNodeCount++] = start;
        
        // エッジに変換
        for (int i = pathNodeCount - 1; i > 0; i--) {
            int from = pathNodes[i];
            int to = pathNodes[i - 1];
            int edgeIdx = findEdgeIndex(from, to);
            if (edgeIdx >= 0) {
                result.path[result.pathLength++] = edgeIdx;
            }
        }
    }
    
    return result;
}

// 期待値ベースのDijkstra（信号待ち時間を期待値で考慮）
DijkstraResult dijkstraWithSignalWaitExpected(int start, int end, bool *forbiddenEdges) {
    double distances[MAX_NODES];
    int previous[MAX_NODES];
    bool visited[MAX_NODES];
    DijkstraResult result;
    
    // 初期化
    for (int i = 0; i < MAX_NODES; i++) {
        distances[i] = INF;
        previous[i] = -1;
        visited[i] = false;
    }
    distances[start] = 0.0;
    
    // ダイクストラ法のメインループ
    while (true) {
        int minNode = -1;
        double minDist = INF;
        
        // 未訪問ノードで最小距離のものを探す
        for (int i = 0; i < MAX_NODES; i++) {
            if (!visited[i] && distances[i] < minDist) {
                minDist = distances[i];
                minNode = i;
            }
        }
        
        if (minNode == -1 || minNode == end) break;
        visited[minNode] = true;
        
        // 隣接ノードを更新
        for (int i = 0; i < graph[minNode].edge_count; i++) {
            int neighbor = graph[minNode].edges[i].node;
            if (visited[neighbor]) continue;
            
            int edgeIdx = graph[minNode].edges[i].edgeIndex;
            if (forbiddenEdges && forbiddenEdges[edgeIdx]) continue;
            
            double edgeTime = getEdgeTimeSeconds(minNode, neighbor);
            if (edgeTime >= INF) continue;
            
            EdgeData *edge = &edgeDataArray[edgeIdx];
            double waitTime = 0.0;
            
            // 信号エッジの場合、期待待ち時間を加算
            if (edge->isSignal) {
                waitTime = edge->expectedWaitTime;  // 期待値を使用（秒）
            } else if (edge->isCrosswalk) {
                waitTime = calculateCrosswalkWaitTime(edgeIdx, 0.0);  // 到着時刻は不要
            }
            
            double newCost = minDist + edgeTime + waitTime;
            if (newCost < distances[neighbor]) {
                distances[neighbor] = newCost;
                previous[neighbor] = minNode;
            }
        }
    }
    
    // 経路を再構築
    result.cost = distances[end];
    result.arrivalTime = distances[end];
    result.pathLength = 0;
    
    if (distances[end] < INF) {
        int currentNode = end;
        int pathNodes[MAX_PATH_LENGTH];
        int pathNodeCount = 0;
        
        while (currentNode != -1 && currentNode != start) {
            pathNodes[pathNodeCount++] = currentNode;
            currentNode = previous[currentNode];
            if (currentNode == -1) {
                result.cost = INF;
                result.pathLength = 0;
                return result;
            }
        }
        pathNodes[pathNodeCount++] = start;
        
        // エッジに変換
        for (int i = pathNodeCount - 1; i > 0; i--) {
            int from = pathNodes[i];
            int to = pathNodes[i - 1];
            int edgeIdx = findEdgeIndex(from, to);
            if (edgeIdx >= 0 && result.pathLength < MAX_PATH_LENGTH) {
                result.path[result.pathLength++] = edgeIdx;
            }
        }
    }
    
    return result;
}

// 経路メトリクスを計算
typedef struct {
    double totalDistance;
    double totalTime;
    double totalExpectedWaitTimeMinutes;
    double totalTimeWithExpected;
} RouteMetrics;

RouteMetrics calculateRouteMetrics(int *edgeIndices, int edgeCount) {
    RouteMetrics metrics;
    metrics.totalDistance = 0.0;
    metrics.totalTime = 0.0;
    metrics.totalExpectedWaitTimeMinutes = 0.0;
    metrics.totalTimeWithExpected = 0.0;
    
    // セグメント間の経路では信号待ち時間を考慮しない（移動時間のみ）
    for (int i = 0; i < edgeCount; i++) {
        EdgeData *edge = &edgeDataArray[edgeIndices[i]];
        metrics.totalDistance += edge->distance;
        
        double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * edge->gradient);
        if (adjustedSpeed > 0) {
            // 移動時間のみを計算（信号待ち時間は考慮しない）
            metrics.totalTime += edge->distance / adjustedSpeed;
        }
    }
    
    // 信号待ち時間は期待値として後で追加されるため、ここでは0
    metrics.totalExpectedWaitTimeMinutes = 0.0;
    metrics.totalTimeWithExpected = metrics.totalTime;
    return metrics;
}

// 経路を順番にたどって、各信号への到着時刻を計算し、厳密な待ち時間を合計（秒）
// 経路のエッジを順番にたどり、各信号への到着時刻を追跡して待ち時間を計算
double calculateTotalSignalWaitTimeFromPath(int *edgeIndices, int edgeCount) {
    double totalSignalWaitTime = 0.0;
    double currentArrivalTime = 0.0;  // 現在の到着時刻（秒）
    
    for (int i = 0; i < edgeCount; i++) {
        EdgeData *edge = &edgeDataArray[edgeIndices[i]];
        
        // エッジの移動時間を計算（秒）
        double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * edge->gradient);
        if (adjustedSpeed > 0) {
            double edgeTime = (edge->distance / adjustedSpeed) * 60.0;  // 秒に変換
            currentArrivalTime += edgeTime;
        }
        
        // 信号エッジの場合、到着時刻を使って待ち時間を厳密に計算
        if (edge->isSignal) {
            double signalWaitTime = calculateSignalWaitTime(edgeIndices[i], currentArrivalTime, true);  // ログ出力
            totalSignalWaitTime += signalWaitTime;
            currentArrivalTime += signalWaitTime;  // 待ち時間を加算して次のエッジの到着時刻を更新
        }
    }
    
    return totalSignalWaitTime;  // 秒単位で返す
}

// 期待値ベースのコスト計算（距離 + 期待待ち時間）
double calculatePathCostWithExpectedWait(int *edgeIndices, int edgeCount) {
    double totalTime = 0.0;
    double totalExpectedWaitTime = 0.0;
    
    for (int i = 0; i < edgeCount; i++) {
        EdgeData *edge = &edgeDataArray[edgeIndices[i]];
        
        // 移動時間を計算（秒）
        double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * edge->gradient);
        if (adjustedSpeed > 0) {
            totalTime += (edge->distance / adjustedSpeed) * 60.0;  // 秒に変換
        }
        
        // 信号エッジの場合、期待待ち時間を加算
        if (edge->isSignal) {
            totalExpectedWaitTime += edge->expectedWaitTime;  // 秒
        }
    }
    
    // コスト = 移動時間 + 期待待ち時間（秒）
    return totalTime + totalExpectedWaitTime;
}

// 経路の距離を計算
double calculatePathDistance(int *edgeIndices, int edgeCount) {
    double totalDistance = 0.0;
    for (int i = 0; i < edgeCount; i++) {
        EdgeData *edge = &edgeDataArray[edgeIndices[i]];
        totalDistance += edge->distance;
    }
    return totalDistance;
}

// 経路の移動時間を計算（信号待ち時間なし）
double calculatePathTime(int *edgeIndices, int edgeCount) {
    double totalTime = 0.0;
    for (int i = 0; i < edgeCount; i++) {
        EdgeData *edge = &edgeDataArray[edgeIndices[i]];
        double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * edge->gradient);
        if (adjustedSpeed > 0) {
            totalTime += (edge->distance / adjustedSpeed) * 60.0;  // 秒に変換
        }
    }
    return totalTime;
}

// 最短経路を実際に通った場合の待ち時間を計算
double calculateActualWaitTimeOnShortestPath(DijkstraResult shortestPath) {
    double totalWaitTime = 0.0;
    double currentArrivalTime = 0.0;
    
    for (int i = 0; i < shortestPath.pathLength; i++) {
        EdgeData *edge = &edgeDataArray[shortestPath.path[i]];
        
        // エッジの移動時間を加算
        double edgeTime = getEdgeTimeSeconds(edge->from, edge->to);
        currentArrivalTime += edgeTime;
        
        // 信号エッジの場合、実際の待ち時間を計算
        if (edge->isSignal) {
            double waitTime = calculateSignalWaitTime(shortestPath.path[i], currentArrivalTime, false);
            totalWaitTime += waitTime;
            currentArrivalTime += waitTime;
        }
    }
    
    return totalWaitTime;
}

// 上限時間を計算（重み付き平均を使用）
double calculateMaxTime(int start, int end) {
    // 最短経路を計算（信号待ち時間なし）
    DijkstraResult shortestPath = dijkstraWithoutSignalWait(start, end, NULL);
    if (shortestPath.cost >= INF || shortestPath.pathLength == 0) {
        return INF;
    }
    
    double shortestPathTime = shortestPath.cost;  // 移動時間のみ（秒）
    
    // 最短経路で通る信号の最悪待ち時間と期待値を計算
    double totalWorstWaitTime = 0.0;
    double totalExpectedWaitTime = 0.0;
    
    for (int i = 0; i < shortestPath.pathLength; i++) {
        EdgeData *edge = &edgeDataArray[shortestPath.path[i]];
        if (edge->isSignal) {
            totalWorstWaitTime += (double)(edge->signalCycle - edge->signalGreen);
            totalExpectedWaitTime += edge->expectedWaitTime;
        }
    }
    
    // 実際の待ち時間を計算（より正確）
    double actualWaitTime = calculateActualWaitTimeOnShortestPath(shortestPath);
    
    // 重み付き平均: α × 実際の待ち時間 + (1-α) × 最悪待ち時間
    // α = 0.7（実際の待ち時間を重視）
    double alpha = 0.7;
    double estimatedWaitTime = alpha * actualWaitTime + (1 - alpha) * totalWorstWaitTime;
    
    // 上限 = 最短経路の時間 + 推定待ち時間
    double maxTime = shortestPathTime + estimatedWaitTime;
    
    return maxTime;
}

// 経路内の各信号の待ち時間が差分（最悪待ち時間 - 期待値）より長いかチェック
// 1つでも差分より長い信号があればfalseを返す（経路を除外）
bool checkPathSignalWaitTimes(int *edgeIndices, int edgeCount) {
    double currentArrivalTime = 0.0;  // 現在の到着時刻（秒）
    
    for (int i = 0; i < edgeCount; i++) {
        EdgeData *edge = &edgeDataArray[edgeIndices[i]];
        
        // エッジの移動時間を計算（秒）
        double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * edge->gradient);
        if (adjustedSpeed > 0) {
            double edgeTime = (edge->distance / adjustedSpeed) * 60.0;  // 秒に変換
            currentArrivalTime += edgeTime;
        }
        
        // 信号エッジの場合、待ち時間を計算して差分と比較
        if (edge->isSignal && edge->signalCycle > 0) {
            // 最悪待ち時間 = cycle - green（秒）
            double worstWaitTime = (double)(edge->signalCycle - edge->signalGreen);
            
            // 期待値（秒）
            double expectedWaitTime = edge->expectedWaitTime;
            
            // 差分 = 最悪待ち時間 - 期待値
            double diff = worstWaitTime - expectedWaitTime;
            
            // 実際の待ち時間を計算
            double actualWaitTime = calculateSignalWaitTime(edgeIndices[i], currentArrivalTime, false);  // ログ出力なし
            
            // 待ち時間が差分より長い場合、この経路を除外
            if (actualWaitTime > diff) {
                fprintf(stderr, "Path excluded: signal edge %d (%d-%d) waitTime=%.2fs > diff=%.2fs (worst=%.2f, expected=%.2f, arrivalTime=%.2fs)\n",
                        edgeIndices[i], edge->from, edge->to, actualWaitTime, diff, worstWaitTime, expectedWaitTime, currentArrivalTime);
                return false;  // 経路を除外
            }
            
            // 待ち時間を加算して次のエッジの到着時刻を更新
            currentArrivalTime += actualWaitTime;
        }
    }
    
    return true;  // すべての信号が差分以下なので経路を含める
}

// エッジパスをノードパスに変換
void edgePathToNodePath(int *edgeIndices, int edgeCount, int startNode, 
                        int *nodePath, int *nodeCount) {
    nodePath[0] = startNode;
    *nodeCount = 1;
    int currentNode = startNode;
    
    for (int i = 0; i < edgeCount; i++) {
        EdgeData *edge = &edgeDataArray[edgeIndices[i]];
        if (edge->from == currentNode) {
            currentNode = edge->to;
        } else if (edge->to == currentNode) {
            currentNode = edge->from;
        } else {
            break; // 接続していない
        }
        nodePath[(*nodeCount)++] = currentNode;
    }
}

// 経路探索結果（網羅的探索用）
// 大きな配列をグローバル変数に移動（スタックオーバーフロー防止）
Path globalPaths[MAX_COMBINATIONS];  // 経路配列（グローバル）

typedef struct {
    Path *paths;  // グローバル配列へのポインタ
    int pathCount;
} YenResult;  // 型名は互換性のため維持

// 2段階Yen'sアルゴリズム（期待値ベースで探索し、実際の待ち時間で再評価）
YenResult yensAlgorithmTwoStage(int start, int end, int K) {
    YenResult result;
    result.paths = globalPaths;
    result.pathCount = 0;
    
    // ステップ1: 上限時間を計算
    double maxTime = calculateMaxTime(start, end);
    if (maxTime >= INF) {
        fprintf(stderr, "Error: Cannot calculate max time\n");
        return result;
    }
    fprintf(stderr, "=== 2段階Yen'sアルゴリズム開始 ===\n");
    fprintf(stderr, "上限時間: %.2f秒 (最短経路の時間 + 推定待ち時間)\n", maxTime);
    
    // ステップ2: 期待値ベースで最短経路を計算
    DijkstraResult basePath = dijkstraWithSignalWaitExpected(start, end, NULL);
    if (basePath.cost >= INF || basePath.pathLength == 0) {
        fprintf(stderr, "Error: No path found\n");
        return result;
    }
    
    // 候補経路を保存する配列
    Path candidates[MAX_COMBINATIONS];
    int candidateCount = 0;
    
    // 最短経路を第1候補として追加（上限チェックを緩和）
    double baseCost = calculatePathCostWithExpectedWait(basePath.path, basePath.pathLength);
    // 上限チェックをコメントアウト（期待値ベースの候補を全て出力）
    // if (baseCost <= maxTime) {
        Path *p = &candidates[candidateCount++];
        memcpy(p->edges, basePath.path, basePath.pathLength * sizeof(int));
        p->edgeCount = basePath.pathLength;
        p->totalDistance = calculatePathDistance(basePath.path, basePath.pathLength);
        p->totalTime = baseCost;
    // }
    
    // K-1個の追加経路を探索
    for (int k = 1; k < K && candidateCount < MAX_COMBINATIONS; k++) {
        if (k - 1 >= candidateCount) break;
        Path *previousPath = &candidates[k - 1];
        if (previousPath->edgeCount == 0) break;
        
        // ノードパスに変換
        int nodePath[MAX_PATH_LENGTH];
        int nodeCount = 0;
        nodePath[nodeCount++] = start;
        int currentNode = start;
        
        for (int i = 0; i < previousPath->edgeCount; i++) {
            EdgeData *edge = &edgeDataArray[previousPath->edges[i]];
            if (edge->from == currentNode) {
                currentNode = edge->to;
            } else if (edge->to == currentNode) {
                currentNode = edge->from;
            } else {
                break;
            }
            nodePath[nodeCount++] = currentNode;
        }
        
        if (nodeCount < 2) break;
        
        // 各ノードから代替経路を探索
        Path newCandidates[MAX_EDGES];
        int newCandidateCount = 0;
        
        for (int j = 0; j < nodeCount - 1 && newCandidateCount < MAX_EDGES; j++) {
            int spurNode = nodePath[j];
            
            // ルートパスを構築（スタートからspurNodeまで）
            int rootPathEdges[MAX_PATH_LENGTH];
            int rootPathLength = 0;
            for (int m = 0; m < j; m++) {
                if (m < previousPath->edgeCount) {
                    rootPathEdges[rootPathLength++] = previousPath->edges[m];
                }
            }
            
            // 禁止エッジを設定
            bool forbiddenEdges[MAX_EDGES];
            for (int m = 0; m < MAX_EDGES; m++) {
                forbiddenEdges[m] = false;
            }
            // ルートパスのエッジを禁止
            for (int m = 0; m < rootPathLength; m++) {
                forbiddenEdges[rootPathEdges[m]] = true;
            }
            // 前の経路のj番目のエッジを禁止
            if (j < previousPath->edgeCount) {
                forbiddenEdges[previousPath->edges[j]] = true;
            }
            // 既存の候補経路のj番目のエッジを禁止
            for (int p = 0; p < candidateCount; p++) {
                if (candidates[p].edgeCount > j) {
                    forbiddenEdges[candidates[p].edges[j]] = true;
                }
            }
            
            // スパー経路を探索（期待値ベース）
            DijkstraResult spurPath = dijkstraWithSignalWaitExpected(spurNode, end, forbiddenEdges);
            
            if (spurPath.cost < INF && spurPath.pathLength > 0) {
                // 候補経路を構築
                int candidateEdges[MAX_PATH_LENGTH];
                int candidateEdgeCount = 0;
                
                // ルートパスのエッジを追加
                for (int m = 0; m < rootPathLength; m++) {
                    if (candidateEdgeCount < MAX_PATH_LENGTH) {
                        candidateEdges[candidateEdgeCount++] = rootPathEdges[m];
                    }
                }
                
                // スパー経路のエッジを追加（重複チェック）
                bool seen[MAX_EDGES];
                for (int m = 0; m < MAX_EDGES; m++) {
                    seen[m] = false;
                }
                for (int m = 0; m < rootPathLength; m++) {
                    seen[rootPathEdges[m]] = true;
                }
                
                for (int m = 0; m < spurPath.pathLength; m++) {
                    if (!seen[spurPath.path[m]] && candidateEdgeCount < MAX_PATH_LENGTH) {
                        candidateEdges[candidateEdgeCount++] = spurPath.path[m];
                        seen[spurPath.path[m]] = true;
                    }
                }
                
                // 重複チェック（既存の候補と比較）
                bool isDuplicate = false;
                for (int c = 0; c < newCandidateCount; c++) {
                    if (newCandidates[c].edgeCount == candidateEdgeCount) {
                        bool same = true;
                        // エッジをソートして比較
                        int sorted1[MAX_PATH_LENGTH];
                        int sorted2[MAX_PATH_LENGTH];
                        memcpy(sorted1, candidateEdges, candidateEdgeCount * sizeof(int));
                        memcpy(sorted2, newCandidates[c].edges, candidateEdgeCount * sizeof(int));
                        
                        // 簡単なソート
                        for (int c1 = 0; c1 < candidateEdgeCount - 1; c1++) {
                            for (int c2 = c1 + 1; c2 < candidateEdgeCount; c2++) {
                                if (sorted1[c1] > sorted1[c2]) {
                                    int temp = sorted1[c1];
                                    sorted1[c1] = sorted1[c2];
                                    sorted1[c2] = temp;
                                }
                                if (sorted2[c1] > sorted2[c2]) {
                                    int temp = sorted2[c1];
                                    sorted2[c1] = sorted2[c2];
                                    sorted2[c2] = temp;
                                }
                            }
                        }
                        
                        for (int m = 0; m < candidateEdgeCount; m++) {
                            if (sorted1[m] != sorted2[m]) {
                                same = false;
                                break;
                            }
                        }
                        if (same) {
                            isDuplicate = true;
                            break;
                        }
                    }
                }
                
                // 既存の結果と比較
                if (!isDuplicate) {
                    for (int p = 0; p < candidateCount; p++) {
                        if (candidates[p].edgeCount == candidateEdgeCount) {
                            bool same = true;
                            for (int m = 0; m < candidateEdgeCount; m++) {
                                if (candidateEdges[m] != candidates[p].edges[m]) {
                                    same = false;
                                    break;
                                }
                            }
                            if (same) {
                                isDuplicate = true;
                                break;
                            }
                        }
                    }
                }
                
                if (!isDuplicate && newCandidateCount < MAX_EDGES) {
                    double candidateCost = calculatePathCostWithExpectedWait(candidateEdges, candidateEdgeCount);
                    // 上限チェックをコメントアウト（期待値ベースの候補を全て出力）
                    // if (candidateCost <= maxTime) {
                        Path *cand = &newCandidates[newCandidateCount++];
                        memcpy(cand->edges, candidateEdges, candidateEdgeCount * sizeof(int));
                        cand->edgeCount = candidateEdgeCount;
                        cand->totalDistance = calculatePathDistance(candidateEdges, candidateEdgeCount);
                        cand->totalTime = candidateCost;
                    // }
                }
            }
        }
        
        if (newCandidateCount == 0) break;
        
        // 候補をソート（コスト優先）
        for (int c1 = 0; c1 < newCandidateCount - 1; c1++) {
            for (int c2 = c1 + 1; c2 < newCandidateCount; c2++) {
                if (newCandidates[c1].totalTime > newCandidates[c2].totalTime) {
                    Path temp = newCandidates[c1];
                    newCandidates[c1] = newCandidates[c2];
                    newCandidates[c2] = temp;
                }
            }
        }
        
        // 最短候補を追加
        if (candidateCount < MAX_COMBINATIONS) {
            candidates[candidateCount++] = newCandidates[0];
        }
    }
    
    fprintf(stderr, "第1段階完了: %d個の候補経路を生成\n", candidateCount);
    
    // ステップ3: 期待値ベースの候補を全て出力
    // 厳密な待ち時間の計算はコメントアウト（期待値のみを使用）
    for (int i = 0; i < candidateCount; i++) {
        Path *path = &candidates[i];
        
        // 期待値ベースのコストを使用（既に計算済み）
        double expectedTime = calculatePathTime(path->edges, path->edgeCount);
        double expectedWaitTime = 0.0;
        
        // 期待値ベースの待ち時間を計算
        for (int j = 0; j < path->edgeCount; j++) {
            EdgeData *edge = &edgeDataArray[path->edges[j]];
            if (edge->isSignal) {
                expectedWaitTime += edge->expectedWaitTime;
            }
        }
        
        // 厳密な待ち時間の計算はコメントアウト
        /*
        // 実際の経路をたどって待ち時間を計算
        double actualTime = calculatePathTime(path->edges, path->edgeCount);
        double actualWaitTime = calculateTotalSignalWaitTimeFromPath(path->edges, path->edgeCount);
        double totalTime = actualTime + actualWaitTime;
        
        // 上限チェック
        if (totalTime <= maxTime) {
        */
        
        // 期待値ベースの候補を全て結果に追加
        Path *p = &result.paths[result.pathCount++];
        memcpy(p->edges, path->edges, path->edgeCount * sizeof(int));
        p->edgeCount = path->edgeCount;
        p->totalDistance = path->totalDistance;
        p->totalTime = expectedTime / 60.0;  // 分に変換（移動時間のみ）
        p->totalWaitTime = expectedWaitTime / 60.0;  // 分に変換（期待値ベースの待ち時間）
        p->totalTimeWithExpected = (expectedTime + expectedWaitTime) / 60.0;  // 分に変換（期待値ベースの合計時間）
        /*
        }
        */
    }
    
    fprintf(stderr, "第2段階完了: %d個の経路を出力（期待値ベース）\n", result.pathCount);
    fprintf(stderr, "=== 2段階Yen'sアルゴリズム終了 ===\n");
    
    return result;
}

// イェンのアルゴリズム（K最短経路の順次探索）- 現在は使用していない（網羅的探索に変更）
// 将来の参考用にコメントアウトして保持
/*
YenResult yensAlgorithm(int start, int end, int *basePathEdges, int basePathLength, int k) {
    YenResult result;
    result.pathCount = 0;
    
    // 基準経路を最初の最短経路として追加
    if (basePathLength > 0) {
        RouteMetrics baseMetrics = calculateRouteMetrics(basePathEdges, basePathLength);
        Path *p = &result.paths[result.pathCount++];
        memcpy(p->edges, basePathEdges, basePathLength * sizeof(int));
        p->edgeCount = basePathLength;
        p->totalDistance = baseMetrics.totalDistance;
        p->totalTime = baseMetrics.totalTimeWithExpected;
        p->totalWaitTime = baseMetrics.totalExpectedWaitTimeMinutes;
        p->totalTimeWithExpected = baseMetrics.totalTimeWithExpected;
    }
    
    // k-1個の追加経路を探索
    for (int i = 1; i < k && result.pathCount < MAX_K; i++) {
        Path *previousPath = &result.paths[i - 1];
        if (previousPath->edgeCount == 0) break;
        
        // ノードパスに変換
        int nodePath[MAX_PATH_LENGTH];
        int nodeCount;
        edgePathToNodePath(previousPath->edges, previousPath->edgeCount, start, 
                          nodePath, &nodeCount);
        
        // 候補経路
        Path candidates[MAX_EDGES];
        int candidateCount = 0;
        
        // 各ノードから代替経路を探索
        for (int j = 0; j < nodeCount - 1 && candidateCount < MAX_EDGES; j++) {
            int spurNode = nodePath[j];
            int rootPathEdges[MAX_PATH_LENGTH];
            int rootPathLength = 0;
            
            // ルートパスを構築
            for (int m = 0; m < j; m++) {
                rootPathEdges[rootPathLength++] = previousPath->edges[m];
            }
            
            // ルートパスの到着時刻を計算
            double arrivalTimeAtSpur = 0.0;
            if (rootPathLength > 0) {
                RouteMetrics rootMetrics = calculateRouteMetrics(rootPathEdges, rootPathLength);
                arrivalTimeAtSpur = rootMetrics.totalTimeWithExpected * 60.0;
            }
            
            // 禁止エッジを設定
            bool forbiddenEdges[MAX_EDGES];
            for (int m = 0; m < MAX_EDGES; m++) {
                forbiddenEdges[m] = false;
            }
            for (int m = 0; m < rootPathLength; m++) {
                forbiddenEdges[rootPathEdges[m]] = true;
            }
            if (j < previousPath->edgeCount) {
                forbiddenEdges[previousPath->edges[j]] = true;
            }
            for (int p = 0; p < result.pathCount; p++) {
                if (result.paths[p].edgeCount > j) {
                    forbiddenEdges[result.paths[p].edges[j]] = true;
                }
            }
            
            // スパー経路を探索
            DijkstraResult spurPath = dijkstraWithSignals(spurNode, end, arrivalTimeAtSpur, 
                                                          forbiddenEdges);
            
            if (spurPath.cost < INF && spurPath.pathLength > 0) {
                // 候補経路を構築
                int candidateEdges[MAX_PATH_LENGTH];
                int candidateEdgeCount = 0;
                
                // 重複を避けながらエッジを追加
                bool seen[MAX_EDGES];
                for (int m = 0; m < MAX_EDGES; m++) {
                    seen[m] = false;
                }
                for (int m = 0; m < rootPathLength; m++) {
                    if (!seen[rootPathEdges[m]]) {
                        candidateEdges[candidateEdgeCount++] = rootPathEdges[m];
                        seen[rootPathEdges[m]] = true;
                    }
                }
                for (int m = 0; m < spurPath.pathLength; m++) {
                    if (!seen[spurPath.path[m]]) {
                        candidateEdges[candidateEdgeCount++] = spurPath.path[m];
                        seen[spurPath.path[m]] = true;
                    }
                }
                
                // 重複チェック（エッジのセットとして比較）
                bool isDuplicate = false;
                
                // 候補のエッジをソートして比較用に準備
                int sortedCandidateEdges[MAX_PATH_LENGTH];
                memcpy(sortedCandidateEdges, candidateEdges, candidateEdgeCount * sizeof(int));
                for (int c1 = 0; c1 < candidateEdgeCount - 1; c1++) {
                    for (int c2 = c1 + 1; c2 < candidateEdgeCount; c2++) {
                        if (sortedCandidateEdges[c1] > sortedCandidateEdges[c2]) {
                            int temp = sortedCandidateEdges[c1];
                            sortedCandidateEdges[c1] = sortedCandidateEdges[c2];
                            sortedCandidateEdges[c2] = temp;
                        }
                    }
                }
                
                // 既存の候補と比較
                for (int c = 0; c < candidateCount; c++) {
                    if (candidates[c].edgeCount == candidateEdgeCount) {
                        int sortedExisting[MAX_PATH_LENGTH];
                        memcpy(sortedExisting, candidates[c].edges, candidateEdgeCount * sizeof(int));
                        for (int c1 = 0; c1 < candidateEdgeCount - 1; c1++) {
                            for (int c2 = c1 + 1; c2 < candidateEdgeCount; c2++) {
                                if (sortedExisting[c1] > sortedExisting[c2]) {
                                    int temp = sortedExisting[c1];
                                    sortedExisting[c1] = sortedExisting[c2];
                                    sortedExisting[c2] = temp;
                                }
                            }
                        }
                        bool same = true;
                        for (int m = 0; m < candidateEdgeCount; m++) {
                            if (sortedExisting[m] != sortedCandidateEdges[m]) {
                                same = false;
                                break;
                            }
                        }
                        if (same) {
                            isDuplicate = true;
                            break;
                        }
                    }
                }
                
                // 既存の結果と比較
                if (!isDuplicate) {
                    for (int p = 0; p < result.pathCount; p++) {
                        if (result.paths[p].edgeCount == candidateEdgeCount) {
                            int sortedExisting[MAX_PATH_LENGTH];
                            memcpy(sortedExisting, result.paths[p].edges, candidateEdgeCount * sizeof(int));
                            for (int c1 = 0; c1 < candidateEdgeCount - 1; c1++) {
                                for (int c2 = c1 + 1; c2 < candidateEdgeCount; c2++) {
                                    if (sortedExisting[c1] > sortedExisting[c2]) {
                                        int temp = sortedExisting[c1];
                                        sortedExisting[c1] = sortedExisting[c2];
                                        sortedExisting[c2] = temp;
                                    }
                                }
                            }
                            bool same = true;
                            for (int m = 0; m < candidateEdgeCount; m++) {
                                if (sortedExisting[m] != sortedCandidateEdges[m]) {
                                    same = false;
                                    break;
                                }
                            }
                            if (same) {
                                isDuplicate = true;
                                break;
                            }
                        }
                    }
                }
                
                if (!isDuplicate && candidateCount < MAX_EDGES) {
                    RouteMetrics candidateMetrics = calculateRouteMetrics(candidateEdges, 
                                                                          candidateEdgeCount);
                    Path *cand = &candidates[candidateCount++];
                    memcpy(cand->edges, candidateEdges, candidateEdgeCount * sizeof(int));
                    cand->edgeCount = candidateEdgeCount;
                    cand->totalDistance = candidateMetrics.totalDistance;
                    cand->totalTime = candidateMetrics.totalTimeWithExpected;
                    cand->totalWaitTime = candidateMetrics.totalExpectedWaitTimeMinutes;
                    cand->totalTimeWithExpected = candidateMetrics.totalTimeWithExpected;
                }
            }
        }
        
        if (candidateCount == 0) break;
        
        // 候補をソート（距離優先、その後時間）
        for (int c1 = 0; c1 < candidateCount - 1; c1++) {
            for (int c2 = c1 + 1; c2 < candidateCount; c2++) {
                double distDiff = candidates[c1].totalDistance - candidates[c2].totalDistance;
                if (fabs(distDiff) > 0.1) {
                    if (distDiff > 0) {
                        Path temp = candidates[c1];
                        candidates[c1] = candidates[c2];
                        candidates[c2] = temp;
                    }
                } else {
                    if (candidates[c1].totalTimeWithExpected > candidates[c2].totalTimeWithExpected) {
                        Path temp = candidates[c1];
                        candidates[c1] = candidates[c2];
                        candidates[c2] = temp;
                    }
                }
            }
        }
        
        // 最短候補を追加
        result.paths[result.pathCount++] = candidates[0];
    }
    
    return result;
}
*/

// result.csvからグラフを構築（重み情報を使用）
void loadGraphFromResult(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open %s\n", filename);
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        
        int from, to;
        double weight;
        if (sscanf(line, "%d,%d,%lf", &from, &to, &weight) == 3) {
            if (from > 0 && from < MAX_NODES && to > 0 && to < MAX_NODES) {
                // エッジデータを探すか作成
                int edgeIdx = findEdgeIndex(from, to);
                if (edgeIdx < 0 && edgeDataCount < MAX_EDGES) {
                    edgeIdx = edgeDataCount++;
                    edgeDataArray[edgeIdx].from = from;
                    edgeDataArray[edgeIdx].to = to;
                    edgeDataArray[edgeIdx].distance = 0; // 後で更新
                    edgeDataArray[edgeIdx].gradient = 0;
                    edgeDataArray[edgeIdx].isSignal = 0;
                    edgeDataArray[edgeIdx].isCrosswalk = 0;
                    edgeDataArray[edgeIdx].signalCycle = 0;
                    edgeDataArray[edgeIdx].signalGreen = 0;
                    edgeDataArray[edgeIdx].signalPhase = 0.0;
                    edgeDataArray[edgeIdx].expectedWaitTime = 0.0;  // デフォルト値
                }
                
                if (edgeIdx >= 0) {
                    // グラフに追加（双方向、重複チェック）
                    bool fromExists = false;
                    bool toExists = false;
                    
                    for (int i = 0; i < graph[from].edge_count; i++) {
                        if (graph[from].edges[i].node == to) {
                            fromExists = true;
                            break;
                        }
                    }
                    for (int i = 0; i < graph[to].edge_count; i++) {
                        if (graph[to].edges[i].node == from) {
                            toExists = true;
                            break;
                        }
                    }
                    
                    if (!fromExists && graph[from].edge_count < 7) {
                        graph[from].edges[graph[from].edge_count].node = to;
                        graph[from].edges[graph[from].edge_count].edgeIndex = edgeIdx;
                        graph[from].edge_count++;
                    }
                    if (!toExists && graph[to].edge_count < 7) {
                        graph[to].edges[graph[to].edge_count].node = from;
                        graph[to].edges[graph[to].edge_count].edgeIndex = edgeIdx;
                        graph[to].edge_count++;
                    }
                }
            }
        }
    }
    
    fclose(file);
}

// CSVファイルからエッジデータを読み込む（距離、勾配、信号、横断歩道情報）
void loadRouteData(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open %s\n", filename);
        return;
    }
    
    char line[1024];
    fgets(line, sizeof(line), file); // ヘッダーをスキップ
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        
        int from, to;
        double distance, gradient;
        int isSignal, isCrosswalk;
        char *token = strtok(line, ",");
        if (!token) continue;
        from = atoi(token);
        
        token = strtok(NULL, ",");
        if (!token) continue;
        to = atoi(token);
        
        token = strtok(NULL, ",");
        if (!token) continue;
        distance = atof(token);
        
        token = strtok(NULL, ",");
        if (!token) continue;
        // time_minutes をスキップ
        
        token = strtok(NULL, ",");
        if (!token) continue;
        gradient = atof(token);
        
        // 残りのカラムをスキップして信号と横断歩道フラグを取得
        for (int i = 0; i < 3; i++) {
            token = strtok(NULL, ",");
            if (!token) break;
        }
        if (token) {
            isSignal = atoi(token);
        } else {
            isSignal = 0;
        }
        
        // 横断歩道フラグ（15番目のカラム）
        for (int i = 0; i < 6; i++) {
            token = strtok(NULL, ",");
            if (!token) break;
        }
        if (token) {
            isCrosswalk = atoi(token);
        } else {
            isCrosswalk = 0;
        }
        
        if (from > 0 && to > 0) {
            int edgeIdx = findEdgeIndex(from, to);
            if (edgeIdx < 0 && edgeDataCount < MAX_EDGES) {
                // 新しいエッジを作成
                edgeIdx = edgeDataCount++;
                edgeDataArray[edgeIdx].from = from;
                edgeDataArray[edgeIdx].to = to;
                edgeDataArray[edgeIdx].expectedWaitTime = 0.0;  // デフォルト値
            }
            if (edgeIdx >= 0) {
                // エッジデータを更新
                edgeDataArray[edgeIdx].distance = distance;
                edgeDataArray[edgeIdx].gradient = gradient;
                edgeDataArray[edgeIdx].isSignal = isSignal;
                edgeDataArray[edgeIdx].isCrosswalk = isCrosswalk;
            }
        }
    }
    
    fclose(file);
}

// ノード座標を読み込む（GeoJSONファイルから）
void loadNodeCoordinates(const char *pointDir) {
    // 全てのノード座標を初期化
    for (int i = 0; i < MAX_NODES; i++) {
        nodeCoords[i].hasCoordinates = false;
        nodeCoords[i].longitude = 0.0;
        nodeCoords[i].latitude = 0.0;
    }
    
    // 各ノードのGeoJSONファイルを読み込む
    for (int nodeId = 1; nodeId < MAX_NODES; nodeId++) {
        char filename[MAX_FILENAME_LENGTH];
        snprintf(filename, sizeof(filename), "%s/%d.geojson", pointDir, nodeId);
        
        FILE *file = fopen(filename, "r");
        if (!file) continue;  // ファイルが存在しない場合はスキップ
        
        char line[1024];
        if (fgets(line, sizeof(line), file)) {
            // 簡易的なJSONパース（coordinates配列を探す）
            // フォーマット: {"type":"Feature","geometry":{"type":"Point","coordinates":[lng,lat]},...}
            char *coordsStart = strstr(line, "\"coordinates\":[");
            if (coordsStart) {
                coordsStart += strlen("\"coordinates\":[");
                char *coordsEnd = strstr(coordsStart, "]");
                if (coordsEnd) {
                    *coordsEnd = '\0';
                    double lng, lat;
                    if (sscanf(coordsStart, "%lf,%lf", &lng, &lat) == 2) {
                        nodeCoords[nodeId].longitude = lng;
                        nodeCoords[nodeId].latitude = lat;
                        nodeCoords[nodeId].hasCoordinates = true;
                    }
                }
            }
        }
        fclose(file);
    }
    
    fprintf(stderr, "Loaded node coordinates from %s\n", pointDir);
}

// 2点間の距離を計算（メートル、Haversine公式）
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;  // 地球の半径（メートル）
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dLon / 2.0) * sin(dLon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return R * c;
}

// 2点間の方位角を計算（度、0-360）
double calculateBearing(double lat1, double lon1, double lat2, double lon2) {
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double lat1Rad = lat1 * M_PI / 180.0;
    double lat2Rad = lat2 * M_PI / 180.0;
    
    double y = sin(dLon) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dLon);
    
    double bearing = atan2(y, x) * 180.0 / M_PI;
    return fmod(bearing + 360.0, 360.0);  // 0-360の範囲に正規化
}

// 角度の差を計算（0-180度）
double angleDifference(double angle1, double angle2) {
    double diff = fabs(angle1 - angle2);
    if (diff > 180.0) {
        diff = 360.0 - diff;
    }
    return diff;
}

// フィルター1: 最悪待ち時間と期待値の差分による信号除外
// 最悪待ち時間から期待値を引いた差分が、信号に到着した時の待ち時間より短い場合、その信号を除外
bool filterByWorstWaitTime(int signalEdgeIdx, double arrivalTimeAtSignal) {
    EdgeData *edge = &edgeDataArray[signalEdgeIdx];
    if (!edge->isSignal || edge->signalCycle <= 0) return true;  // 信号でない場合は除外しない
    
    // 最悪待ち時間 = cycle - green（秒）
    double worstWaitTime = (double)(edge->signalCycle - edge->signalGreen);
    
    // 期待値（秒）
    double expectedWaitTime = edge->expectedWaitTime;
    
    // 差分 = 最悪待ち時間 - 期待値
    double diff = worstWaitTime - expectedWaitTime;
    
    // 最適化: 差分が負または非常に小さい場合は早期除外（計算をスキップ）
    if (diff <= 0.0) {
        return false;  // 差分が0以下なら常に除外
    }
    
    // 信号に到着した時の待ち時間を計算
    // 到着時刻をサイクルで割った余りから、信号サイクル内での位置を計算
    double cycleTime = (double)edge->signalCycle;
    double arrivalPhase = fmod(arrivalTimeAtSignal, cycleTime);
    double actualWaitTime = 0.0;
    
    // 信号の位相を考慮して待ち時間を計算
    // phase: 青信号の開始時刻（サイクル内での位置）
    // green: 青信号の継続時間
    if (arrivalPhase < edge->signalPhase) {
        // 青信号の開始前：青信号開始まで待つ
        actualWaitTime = edge->signalPhase - arrivalPhase;
    } else if (arrivalPhase >= edge->signalPhase + (double)edge->signalGreen) {
        // 青信号の終了後：次のサイクルの青信号開始まで待つ
        actualWaitTime = cycleTime - arrivalPhase + edge->signalPhase;
    } else {
        // 青信号中：待ち時間なし
        actualWaitTime = 0.0;
    }
    
    // 差分が実際の待ち時間より短い場合、この信号を除外
    // つまり、実際の待ち時間が大きい（差分が小さい）場合に除外
    bool shouldExclude = (diff < actualWaitTime);
    
    // ログ出力を削減（デバッグ時のみ）
    // if (shouldExclude) {
    //     fprintf(stderr, "Filter1: Excluding signal edge %d (%d-%d): diff=%.2f < actualWait=%.2f\n",
    //             signalEdgeIdx, edge->from, edge->to, diff, actualWaitTime);
    // }
    
    return !shouldExclude;  // trueなら含める、falseなら除外
}

// フィルター2: スタート地点からの方向範囲による信号除外
bool filterByDirection(int signalEdgeIdx) {
    if (!nodeCoords[startNode].hasCoordinates) return true;  // 座標がない場合は除外しない
    
    EdgeData *edge = &edgeDataArray[signalEdgeIdx];
    
    // 信号エッジの中点を計算
    int signalNode1 = edge->from;
    int signalNode2 = edge->to;
    
    if (!nodeCoords[signalNode1].hasCoordinates || !nodeCoords[signalNode2].hasCoordinates) {
        return true;  // 座標がない場合は除外しない
    }
    
    // 信号エッジの中点座標
    double signalLat = (nodeCoords[signalNode1].latitude + nodeCoords[signalNode2].latitude) / 2.0;
    double signalLon = (nodeCoords[signalNode1].longitude + nodeCoords[signalNode2].longitude) / 2.0;
    
    // スタート地点からゴール地点への方向（基準方向、0度とする）
    if (!nodeCoords[endNode].hasCoordinates) return true;
    double goalBearing = calculateBearing(
        nodeCoords[startNode].latitude, nodeCoords[startNode].longitude,
        nodeCoords[endNode].latitude, nodeCoords[endNode].longitude
    );
    
    // スタート地点から信号への方向
    double signalBearing = calculateBearing(
        nodeCoords[startNode].latitude, nodeCoords[startNode].longitude,
        signalLat, signalLon
    );
    
    // ゴール方向を0度とした相対角度を計算（-180度から+180度の範囲）
    double relativeAngle = signalBearing - goalBearing;
    // -180度から+180度の範囲に正規化
    while (relativeAngle > 180.0) relativeAngle -= 360.0;
    while (relativeAngle < -180.0) relativeAngle += 360.0;
    
    // 角度差の絶対値が許容範囲内かチェック（左右±45度の範囲）
    double angleDiff = fabs(relativeAngle);
    bool shouldInclude = (angleDiff <= ANGLE_TOLERANCE);
    
    // ログ出力を削減（デバッグ時のみ）
    // if (!shouldInclude) {
    //     fprintf(stderr, "Filter2: Excluding signal edge %d (%d-%d): angleDiff=%.2f > tolerance=%.2f\n",
    //             signalEdgeIdx, edge->from, edge->to, angleDiff, ANGLE_TOLERANCE);
    // }
    
    return shouldInclude;
}

// フィルター3: 遠回り経路の除外（削除済み - フィルター5と7に置き換え）
// 2段階フィルタリングで処理時間を短縮：
// 第1段階（高速）: 直線距離ベースで推定し、明らかに遠回りの組み合わせを除外
// 第2段階（正確）: 第1段階を通過した組み合わせのみDijkstraで正確に計算
// 最適化: 最短経路距離をキャッシュして重複計算を削減
static double cachedShortestDistance = -1.0;
static int cachedStart = -1;
static int cachedEnd = -1;

bool filterByDetour(int *signalEdgeIndices, int signalCount, int start, int end) {
    if (signalCount == 0) return true;  // 信号がない場合は除外しない
    
    // 最短経路距離をキャッシュ（start/endが同じ場合は再利用）
    double shortestDistance;
    if (cachedStart == start && cachedEnd == end && cachedShortestDistance >= 0.0) {
        shortestDistance = cachedShortestDistance;
    } else {
    // 最短経路（信号を考慮しない）を計算
    DijkstraResult shortestPath = dijkstraWithoutSignalWait(start, end, NULL);
    if (shortestPath.cost >= INF || shortestPath.pathLength == 0) {
        return true;  // 最短経路が見つからない場合は除外しない
    }
    
    // 最短経路の距離を計算
        shortestDistance = 0.0;
    for (int i = 0; i < shortestPath.pathLength; i++) {
        EdgeData *edge = &edgeDataArray[shortestPath.path[i]];
        shortestDistance += edge->distance;
    }
    
        // キャッシュに保存
        cachedShortestDistance = shortestDistance;
        cachedStart = start;
        cachedEnd = end;
    }
    
    // === 第1段階: 直線距離ベースの高速フィルタリング ===
    // 座標が利用可能な場合、直線距離で推定して早期除外
    if (nodeCoords[start].hasCoordinates && nodeCoords[end].hasCoordinates) {
        double straightLineDistance = 0.0;
        int lastNode = start;
        
        // スタート→信号1→信号2→...→ゴールの直線距離の合計を計算
    for (int i = 0; i < signalCount; i++) {
        int signalEdgeIdx = signalEdgeIndices[i];
        EdgeData *signalEdge = &edgeDataArray[signalEdgeIdx];
        int signalFrom = signalEdge->from;
        int signalTo = signalEdge->to;
        
            // 前のノードから信号までの直線距離を計算
            // 信号のfrom/toのうち、前のノードに近い方を選択
            double distToFrom = INF;
            double distToTo = INF;
            
            if (nodeCoords[lastNode].hasCoordinates) {
                if (nodeCoords[signalFrom].hasCoordinates) {
                    distToFrom = calculateDistance(
                        nodeCoords[lastNode].latitude, nodeCoords[lastNode].longitude,
                        nodeCoords[signalFrom].latitude, nodeCoords[signalFrom].longitude
                    );
                }
                if (nodeCoords[signalTo].hasCoordinates) {
                    distToTo = calculateDistance(
                        nodeCoords[lastNode].latitude, nodeCoords[lastNode].longitude,
                        nodeCoords[signalTo].latitude, nodeCoords[signalTo].longitude
                    );
                }
            }
            
            // 近い方の距離を加算
            if (distToFrom < distToTo) {
                straightLineDistance += distToFrom;
                lastNode = signalTo;  // 信号を通過した後のノード
            } else if (distToTo < INF) {
                straightLineDistance += distToTo;
                lastNode = signalFrom;
        } else {
                // 座標がない場合は第2段階に進む
                break;
            }
        }
        
        // 最後の信号からゴールまでの直線距離
        if (nodeCoords[lastNode].hasCoordinates && nodeCoords[end].hasCoordinates) {
            straightLineDistance += calculateDistance(
                nodeCoords[lastNode].latitude, nodeCoords[lastNode].longitude,
                nodeCoords[end].latitude, nodeCoords[end].longitude
            );
        }
        
        // 直線距離ベースの比率を計算（実際の経路は直線より長いため、1.2倍のマージンを考慮）
        // 直線距離 × 1.2 が最短経路 × 閾値を超える場合は除外
        double estimatedPathDistance = straightLineDistance * 1.2;  // マージン係数
        double estimatedRatio = estimatedPathDistance / shortestDistance;
        
        // 第1段階のみで判定（Dijkstra計算をスキップして高速化）
        // estimatedRatio > DETOUR_THRESHOLD の場合、実際の経路は閾値を超える可能性が高い
        // マージン係数1.2を考慮して、より厳しい閾値を使用
        if (estimatedRatio > DETOUR_THRESHOLD) {
            return false;  // 早期除外（Dijkstra計算をスキップ）
        }
        // estimatedRatio <= DETOUR_THRESHOLD の場合は含める
    } else {
        // 座標がない場合は、第2段階（Dijkstra計算）を実行
        // ただし、処理時間を考慮して簡略化
        // ここでは座標がない場合のみDijkstra計算を行う
        // 通常は座標があるため、この分岐はほとんど実行されない
    }
    
    // 第2段階（Dijkstra計算）はスキップして、第1段階のみで判定
    // 処理時間を短縮するため、Dijkstra計算による正確な判定は行わない
    return true;  // 第1段階を通過した場合は含める
}

// フィルター4: 信号数の上限による除外
// 一定数以上の信号を通る組み合わせを除外（複雑すぎる経路を除外）
bool filterBySignalCount(int signalCount) {
    if (signalCount > MAX_SIGNALS_PER_COMBINATION) {
        return false;  // 除外
    }
    return true;  // 含める
}

// フィルター5: 信号待ち時間の合計による除外
// 期待値の合計が一定以上（例：90秒以上）の組み合わせを除外
bool filterByTotalWaitTime(int *signalEdgeIndices, int signalCount) {
    double totalExpectedWaitTime = 0.0;
    for (int i = 0; i < signalCount; i++) {
        EdgeData *edge = &edgeDataArray[signalEdgeIndices[i]];
        totalExpectedWaitTime += edge->expectedWaitTime;
    }
    if (totalExpectedWaitTime > MAX_TOTAL_EXPECTED_WAIT_TIME) {
        return false;  // 除外
    }
    return true;  // 含める
}

// フィルター6: 信号間の距離による除外
// 信号間が近すぎる（例：100m以内）組み合わせを除外
bool filterBySignalProximity(int *signalEdgeIndices, int signalCount) {
    if (signalCount < 2) return true;  // 信号が1個以下の場合は除外しない
    
    int closePairs = 0;  // 近接する信号ペアの数
    
    for (int i = 0; i < signalCount; i++) {
        EdgeData *edge1 = &edgeDataArray[signalEdgeIndices[i]];
        int signal1From = edge1->from;
        int signal1To = edge1->to;
        
        // 信号1の中点座標を計算
        double signal1Lat, signal1Lon;
        if (nodeCoords[signal1From].hasCoordinates && nodeCoords[signal1To].hasCoordinates) {
            signal1Lat = (nodeCoords[signal1From].latitude + nodeCoords[signal1To].latitude) / 2.0;
            signal1Lon = (nodeCoords[signal1From].longitude + nodeCoords[signal1To].longitude) / 2.0;
        } else {
            continue;  // 座標がない場合はスキップ
        }
        
        // 他の信号との距離をチェック
        for (int j = i + 1; j < signalCount; j++) {
            EdgeData *edge2 = &edgeDataArray[signalEdgeIndices[j]];
            int signal2From = edge2->from;
            int signal2To = edge2->to;
            
            // 信号2の中点座標を計算
            if (nodeCoords[signal2From].hasCoordinates && nodeCoords[signal2To].hasCoordinates) {
                double signal2Lat = (nodeCoords[signal2From].latitude + nodeCoords[signal2To].latitude) / 2.0;
                double signal2Lon = (nodeCoords[signal2From].longitude + nodeCoords[signal2To].longitude) / 2.0;
                
                // 信号間の距離を計算
                double distance = calculateDistance(signal1Lat, signal1Lon, signal2Lat, signal2Lon);
                
                if (distance < MIN_SIGNAL_DISTANCE) {
                    closePairs++;
                }
            }
        }
    }
    
    // 近接する信号ペアが2組以上ある場合は除外
    if (closePairs >= 2) {
        return false;  // 除外
    }
    return true;  // 含める
}

// フィルター7: 信号の順序の合理性チェック
// 地理的に不合理な順序（後戻り）の組み合わせを除外
// 信号の位置をスタートからの距離でソートし、その順序と大きく異なる順列を除外
bool filterBySignalOrder(int *signalEdgeIndices, int signalCount, int start, int end) {
    if (signalCount < 2) return true;  // 信号が1個以下の場合は除外しない
    if (!nodeCoords[start].hasCoordinates) return true;  // 座標がない場合は除外しない
    
    // 各信号のスタートからの距離を計算
    typedef struct {
        int signalIdx;
        double distanceFromStart;
    } SignalDistance;
    
    SignalDistance signalDistances[MAX_SIGNALS];
    int validSignals = 0;
    
    for (int i = 0; i < signalCount; i++) {
        EdgeData *edge = &edgeDataArray[signalEdgeIndices[i]];
        int signalFrom = edge->from;
        int signalTo = edge->to;
        
        // 信号の中点座標を計算
        double signalLat, signalLon;
        if (nodeCoords[signalFrom].hasCoordinates && nodeCoords[signalTo].hasCoordinates) {
            signalLat = (nodeCoords[signalFrom].latitude + nodeCoords[signalTo].latitude) / 2.0;
            signalLon = (nodeCoords[signalFrom].longitude + nodeCoords[signalTo].longitude) / 2.0;
            
            // スタートからの距離を計算
            double distance = calculateDistance(
                nodeCoords[start].latitude, nodeCoords[start].longitude,
                signalLat, signalLon
            );
            
            signalDistances[validSignals].signalIdx = signalEdgeIndices[i];
            signalDistances[validSignals].distanceFromStart = distance;
            validSignals++;
        }
    }
    
    if (validSignals < 2) return true;  // 有効な信号が2個未満の場合は除外しない
    
    // 距離でソート（バブルソート）
    for (int i = 0; i < validSignals - 1; i++) {
        for (int j = 0; j < validSignals - 1 - i; j++) {
            if (signalDistances[j].distanceFromStart > signalDistances[j + 1].distanceFromStart) {
                SignalDistance temp = signalDistances[j];
                signalDistances[j] = signalDistances[j + 1];
                signalDistances[j + 1] = temp;
            }
        }
    }
    
    // 理想的な順序（距離の小さい順）を取得
    int idealOrder[MAX_SIGNALS];
    for (int i = 0; i < validSignals; i++) {
        idealOrder[i] = signalDistances[i].signalIdx;
    }
    
    // 現在の順序と理想的な順序を比較
    // 後戻り（逆順）のペアの数をカウント
    int reversePairs = 0;
    for (int i = 0; i < signalCount; i++) {
        int currentSignal = signalEdgeIndices[i];
        int idealPos = -1;
        
        // 理想的な順序での位置を探す
        for (int j = 0; j < validSignals; j++) {
            if (idealOrder[j] == currentSignal) {
                idealPos = j;
                break;
            }
        }
        
        if (idealPos < 0) continue;  // 見つからない場合はスキップ
        
        // 後続の信号で、理想的な順序では前にあるものをカウント
        for (int j = i + 1; j < signalCount; j++) {
            int nextSignal = signalEdgeIndices[j];
            int nextIdealPos = -1;
            
            for (int k = 0; k < validSignals; k++) {
                if (idealOrder[k] == nextSignal) {
                    nextIdealPos = k;
                    break;
                }
            }
            
            if (nextIdealPos >= 0 && nextIdealPos < idealPos) {
                reversePairs++;  // 後戻りを検出
            }
        }
    }
    
    // 後戻りのペアが信号数の半分を超える場合は除外
    // 例：信号3個の場合、後戻りが2個以上ある場合は除外
    int maxReversePairs = validSignals / 2;
    if (reversePairs > maxReversePairs) {
        return false;  // 除外
    }
    
    return true;  // 含める
}

// 信号情報を読み込む
void loadSignalData(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Warning: Cannot open %s\n", filename);
        return;
    }
    
    char line[1024];
    fgets(line, sizeof(line), file); // ヘッダーをスキップ
    
    signalCount = 0;
    bool seenEdges[MAX_EDGES];
    for (int i = 0; i < MAX_EDGES; i++) {
        seenEdges[i] = false;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        
        int cycle, green;
        double phase;
        double expectedWaitTime = 0.0;  // CSVから読み込んだ期待値
        
        // signal_inf.csvのフォーマット: node1,node2,cycle,green,phase,expected
        // cycle, green, phaseは浮動小数点数として読み込む
        // expected（期待値）は必須
        int from, to;
        double cycle_d, green_d;
        int parsed = sscanf(line, "%d,%d,%lf,%lf,%lf,%lf", &from, &to, &cycle_d, &green_d, &phase, &expectedWaitTime);
        if (parsed >= 6) {
            // 浮動小数点数を整数に変換（cycle, greenは整数として扱う）
            cycle = (int)cycle_d;
            green = (int)green_d;
            int edgeIdx = findEdgeIndex(from, to);
            if (edgeIdx >= 0) {
                edgeDataArray[edgeIdx].signalCycle = cycle;
                edgeDataArray[edgeIdx].signalGreen = green;
                edgeDataArray[edgeIdx].signalPhase = phase;
                edgeDataArray[edgeIdx].expectedWaitTime = expectedWaitTime;  // CSVから読み込んだ期待値を設定
                
                // 信号エッジのリストに追加（重複を避ける）
                // signal_inf.csvに含まれる信号エッジのみを追加
                if (!seenEdges[edgeIdx] && signalCount < MAX_SIGNALS) {
                    signalEdges[signalCount++] = edgeIdx;
                    seenEdges[edgeIdx] = true;
                    fprintf(stderr, "Signal %d: edge %d (%d-%d) cycle=%d green=%d phase=%.2f expected=%.2f\n", 
                            signalCount, edgeIdx, from, to, cycle, green, phase, expectedWaitTime);
                }
            } else {
                fprintf(stderr, "Warning: Signal edge %d-%d not found in graph (edgeIdx=%d)\n", from, to, edgeIdx);
                // グラフに存在するか確認
                bool foundInGraph = false;
                for (int i = 0; i < edgeDataCount; i++) {
                    if ((edgeDataArray[i].from == from && edgeDataArray[i].to == to) ||
                        (edgeDataArray[i].from == to && edgeDataArray[i].to == from)) {
                        fprintf(stderr, "  Found similar edge: %d-%d (edgeIdx=%d)\n", 
                                edgeDataArray[i].from, edgeDataArray[i].to, i);
                        foundInGraph = true;
                    }
                }
                if (!foundInGraph) {
                    fprintf(stderr, "  Edge %d-%d does not exist in graph at all\n", from, to);
                }
            }
        } else {
            // パースに失敗した場合のデバッグ情報
            fprintf(stderr, "Warning: Failed to parse signal line: %s", line);
        }
    }
    
    fclose(file);
    
    // signal_inf.csvから読み込んだ信号エッジでisSignalフラグを設定
    for (int i = 0; i < edgeDataCount; i++) {
        if (edgeDataArray[i].signalCycle > 0) {
            edgeDataArray[i].isSignal = 1;
        }
    }
    
    // デバッグ用：読み込んだ信号数を出力
    fprintf(stderr, "Loaded %d signals from signal_inf.csv\n", signalCount);
    
    // 信号エッジがグラフに接続されているか確認
    int connectedSignalCount = 0;
    for (int i = 0; i < signalCount; i++) {
        int edgeIdx = signalEdges[i];
        EdgeData *edge = &edgeDataArray[edgeIdx];
        bool connected = false;
        
        // グラフに接続されているか確認
        for (int j = 0; j < graph[edge->from].edge_count; j++) {
            if (graph[edge->from].edges[j].node == edge->to) {
                connected = true;
                break;
            }
        }
        if (!connected) {
            for (int j = 0; j < graph[edge->to].edge_count; j++) {
                if (graph[edge->to].edges[j].node == edge->from) {
                    connected = true;
                    break;
                }
            }
        }
        
        if (connected) {
            connectedSignalCount++;
        } else {
            fprintf(stderr, "Warning: Signal edge %d (%d-%d) not connected in graph\n", 
                    edgeIdx, edge->from, edge->to);
        }
    }
    
    fprintf(stderr, "Connected signals: %d/%d\n", connectedSignalCount, signalCount);
}

// 信号の順列を生成（再帰的）- 全ての順列を生成してバリエーションを最大化
void generateAllPermutations(int *arr, int start, int end, 
                             int (*permutations)[MAX_SIGNALS], int *permCount, int maxPerms) {
    if (*permCount >= maxPerms) return;
    
    if (start == end) {
        // 順列が完成したら保存
        for (int i = 0; i <= end; i++) {
            permutations[*permCount][i] = arr[i];
        }
        (*permCount)++;
        return;
    }
    
    for (int i = start; i <= end && *permCount < maxPerms; i++) {
        // スワップ
        int temp = arr[start];
        arr[start] = arr[i];
        arr[i] = temp;
        
        // 再帰
        generateAllPermutations(arr, start + 1, end, permutations, permCount, maxPerms);
        
        // バックトラック
        temp = arr[start];
        arr[start] = arr[i];
        arr[i] = temp;
    }
}

// 信号の順列を生成して、異なる順序で通る経路を探索するためのヘルパー関数
// バリエーションを最大化するため、可能な限り多くの順列を生成
void generateSignalPermutations(int *signalEdgeIndices, int signalCount, 
                                 int (*permutations)[MAX_SIGNALS], int *permCount) {
    *permCount = 0;
    
    if (signalCount <= 1) {
        // 信号が1個以下の場合は順列は1つだけ
        if (signalCount == 1) {
            permutations[0][0] = signalEdgeIndices[0];
            *permCount = 1;
        }
        return;
    }
    
    // 信号数が多い場合は順列生成を制限（処理時間短縮）
    // 信号数が2-3個: 全順列、4個以上: 最大6個（処理時間短縮のため）
    int maxPerms = (signalCount <= 3) ? 24 : 6;
    
    // 配列をコピーして順列を生成
    int arr[MAX_SIGNALS];
    for (int i = 0; i < signalCount; i++) {
        arr[i] = signalEdgeIndices[i];
    }
    
    generateAllPermutations(arr, 0, signalCount - 1, permutations, permCount, maxPerms);
    
    // 順列が少ない場合は、シフトした順序も追加
    if (*permCount < maxPerms) {
        for (int offset = 1; offset < signalCount && *permCount < maxPerms; offset++) {
            bool exists = false;
            for (int p = 0; p < *permCount; p++) {
                bool same = true;
                for (int i = 0; i < signalCount; i++) {
                    if (permutations[p][i] != signalEdgeIndices[(i + offset) % signalCount]) {
                        same = false;
                        break;
                    }
                }
                if (same) {
                    exists = true;
                    break;
                }
            }
            if (!exists && *permCount < maxPerms) {
                for (int i = 0; i < signalCount; i++) {
                    permutations[*permCount][i] = signalEdgeIndices[(i + offset) % signalCount];
                }
                (*permCount)++;
            }
        }
    }
}

// 信号エッジの方向がスタート→ゴールの方向かどうかを判定
// ゴール→スタートの方向の信号エッジは除外する
bool isSignalInStartToGoalDirection(int signalEdgeIdx, int start, int end) {
    if (!nodeCoords[start].hasCoordinates || !nodeCoords[end].hasCoordinates) {
        return true;  // 座標がない場合は除外しない
    }
    
    EdgeData *edge = &edgeDataArray[signalEdgeIdx];
    int signalFrom = edge->from;
    int signalTo = edge->to;
    
    if (!nodeCoords[signalFrom].hasCoordinates || !nodeCoords[signalTo].hasCoordinates) {
        return true;  // 座標がない場合は除外しない
    }
    
    // スタート→ゴールの方向ベクトル
    double startToGoalBearing = calculateBearing(
        nodeCoords[start].latitude, nodeCoords[start].longitude,
        nodeCoords[end].latitude, nodeCoords[end].longitude
    );
    
    // 信号エッジの方向ベクトル（from→to）
    double signalBearing = calculateBearing(
        nodeCoords[signalFrom].latitude, nodeCoords[signalFrom].longitude,
        nodeCoords[signalTo].latitude, nodeCoords[signalTo].longitude
    );
    
    // 角度差を計算（0-180度）
    double angleDiff = angleDifference(startToGoalBearing, signalBearing);
    
    // 角度差が90度以下の場合、スタート→ゴールの方向と一致しているとみなす
    // 角度差が90度を超える場合、ゴール→スタートの方向なので除外
    return angleDiff <= 90.0;
}

// 信号の順序に従って経路を生成（スタート→信号1→信号2→...→信号N→ゴール）
// 各セグメントは信号待ち時間を考慮せずにダイクストラ法で最短経路を算出
DijkstraResult findPathThroughSignalsInOrder(int start, int end, int *signalEdgeIndices, int signalCount) {
    DijkstraResult result;
    result.cost = 0.0;
    result.arrivalTime = 0.0;
    result.pathLength = 0;
    
    if (signalCount == 0) {
        // 信号がない場合は通常の最短経路
        return dijkstraWithoutSignalWait(start, end, NULL);
    }
    
    // 信号の順序は最適化せず、元の順序のまま使用（バリエーションを増やすため）
    int currentNode = start;
    double currentArrivalTime = 0.0;  // 現在の到着時刻を追跡（秒）
    
    // 各信号を順番に通る
    for (int i = 0; i < signalCount; i++) {
        int signalEdgeIdx = signalEdgeIndices[i];
        EdgeData *signalEdge = &edgeDataArray[signalEdgeIdx];
        int signalFrom = signalEdge->from;
        int signalTo = signalEdge->to;
        
        // 信号エッジの方向をチェック：ゴール→スタートの方向の信号は除外
        if (!isSignalInStartToGoalDirection(signalEdgeIdx, start, end)) {
            // ゴール→スタートの方向の信号なので、この経路を除外
            fprintf(stderr, "Path excluded: signal edge %d (%d-%d) is in goal-to-start direction\n",
                    signalEdgeIdx, signalFrom, signalTo);
            result.cost = INF;
            result.pathLength = 0;
            return result;
        }
        
        // 現在のノードから信号のfromノードまでの最短経路（信号待ち時間なし）
        DijkstraResult segment = dijkstraWithoutSignalWait(currentNode, signalFrom, NULL);
        bool reverseDirection = false;
        
        if (segment.cost >= INF || segment.pathLength == 0) {
            // 経路が見つからない場合は、双方向を試す
            segment = dijkstraWithoutSignalWait(currentNode, signalTo, NULL);
            if (segment.cost >= INF || segment.pathLength == 0) {
                // デバッグ情報：信号エッジに到達できない
                fprintf(stderr, "Warning: Cannot reach signal edge %d (%d-%d) from node %d\n", 
                        signalEdgeIdx, signalFrom, signalTo, currentNode);
                result.cost = INF;
                result.pathLength = 0;
                return result;
            }
            reverseDirection = true;
        }
        
        // セグメントのエッジを追加
        for (int j = 0; j < segment.pathLength; j++) {
            if (result.pathLength < MAX_PATH_LENGTH) {
                result.path[result.pathLength++] = segment.path[j];
            }
        }
        
        // セグメントの移動時間を加算（到着時刻を更新）
        currentArrivalTime += segment.cost;
        result.cost += segment.cost;
        
        // 信号エッジがグラフに接続されているか確認
        bool signalEdgeExists = false;
        int signalEdgeGraphIdx = -1;
        
        // グラフから信号エッジを探す
        for (int j = 0; j < graph[signalFrom].edge_count; j++) {
            if (graph[signalFrom].edges[j].node == signalTo) {
                signalEdgeExists = true;
                signalEdgeGraphIdx = graph[signalFrom].edges[j].edgeIndex;
                break;
            }
        }
        
        // 双方向を確認
        if (!signalEdgeExists) {
            for (int j = 0; j < graph[signalTo].edge_count; j++) {
                if (graph[signalTo].edges[j].node == signalFrom) {
                    signalEdgeExists = true;
                    signalEdgeGraphIdx = graph[signalTo].edges[j].edgeIndex;
                    break;
                }
            }
        }
        
        // 信号エッジがグラフに存在する場合のみ追加
        if (signalEdgeExists && signalEdgeGraphIdx >= 0) {
            // 信号エッジを追加（グラフのエッジインデックスを使用）
            if (result.pathLength < MAX_PATH_LENGTH) {
                result.path[result.pathLength++] = signalEdgeGraphIdx;
            }
            
            // 信号への到着時刻を計算（信号エッジのfromノードに到着した時刻）
            double arrivalTimeAtSignal = currentArrivalTime;
            
            // 信号待ち時間を厳密に計算（CSVのphase、cycle、greenを使用）
            double signalWaitTime = calculateSignalWaitTime(signalEdgeGraphIdx, arrivalTimeAtSignal, true);  // ログ出力
            
            // 信号エッジの通過時間を加算
            double signalEdgeTime = getEdgeTimeSeconds(signalFrom, signalTo);
            if (signalEdgeTime < INF) {
                result.cost += signalEdgeTime;
                currentArrivalTime += signalEdgeTime;
            }
            
            // 信号待ち時間を加算
            result.cost += signalWaitTime;
            currentArrivalTime += signalWaitTime;
        } else {
            // 信号エッジがグラフに存在しない場合、警告を出力
            fprintf(stderr, "Warning: Signal edge %d (%d-%d) not found in graph, skipping\n", 
                    signalEdgeIdx, signalFrom, signalTo);
            // 信号エッジを通らないが、経路は続行
        }
        
        currentNode = reverseDirection ? signalFrom : signalTo;
    }
    
    // 到着時刻を更新
    result.arrivalTime = currentArrivalTime;
    
    // 最後の信号からゴールまでの最短経路（信号待ち時間なし）
    DijkstraResult finalSegment = dijkstraWithoutSignalWait(currentNode, end, NULL);
    if (finalSegment.cost >= INF || finalSegment.pathLength == 0) {
        result.cost = INF;
        result.pathLength = 0;
        return result;
    }
    
    // 最終セグメントのエッジを追加
    for (int j = 0; j < finalSegment.pathLength; j++) {
        if (result.pathLength < MAX_PATH_LENGTH) {
            result.path[result.pathLength++] = finalSegment.path[j];
        }
    }
    
    // 最終セグメントの移動時間を加算（到着時刻を更新）
    result.cost += finalSegment.cost;
    currentArrivalTime += finalSegment.cost;
    result.arrivalTime = currentArrivalTime;
    
    return result;
}

// 信号の組み合わせに基づいて経路を生成（2段階Yen'sアルゴリズム使用）
YenResult generateRoutesBySignalCombinations(int start, int end) {
    YenResult result;
    result.paths = globalPaths;  // グローバル配列を使用
    result.pathCount = 0;
    
    // グローバル変数に設定（フィルター用）
    startNode = start;
    endNode = end;
    
    if (signalCount == 0) {
        // 信号がない場合は通常の最短経路を返す
        DijkstraResult basePath = dijkstraWithoutSignalWait(start, end, NULL);
        if (basePath.cost < INF && basePath.pathLength > 0) {
            RouteMetrics baseMetrics = calculateRouteMetrics(basePath.path, basePath.pathLength);
            Path *p = &result.paths[result.pathCount++];
            memcpy(p->edges, basePath.path, basePath.pathLength * sizeof(int));
            p->edgeCount = basePath.pathLength;
            p->totalDistance = baseMetrics.totalDistance;
            p->totalTime = baseMetrics.totalTimeWithExpected;
            p->totalWaitTime = 0.0;  // 信号待ち時間なし
            p->totalTimeWithExpected = baseMetrics.totalTimeWithExpected;
        }
        return result;
    }
    
    // 2段階Yen'sアルゴリズムを使用
    // K = 100（上位100本の経路を探索）
    int K = 100;
    result = yensAlgorithmTwoStage(start, end, K);
    
    return result;
}

// JSON形式で結果を出力（全経路を表示）
void printJSONResult(YenResult *result) {
    printf("[\n");
    for (int i = 0; i < result->pathCount; i++) {
        Path *p = &result->paths[i];
        
        // デバッグ: 経路の最初のエッジを確認
        if (p->edgeCount > 0) {
            EdgeData *firstEdge = &edgeDataArray[p->edges[0]];
            fprintf(stderr, "Route %d: First edge from=%d to=%d (startNode=%d, endNode=%d)\n", 
                    i+1, firstEdge->from, firstEdge->to, startNode, endNode);
        }
        
        printf("  {\n");
        printf("    \"userPref\": \"");
        for (int j = 0; j < p->edgeCount; j++) {
            EdgeData *edge = &edgeDataArray[p->edges[j]];
            int normFrom, normTo;
            normalizeEdgeKey(edge->from, edge->to, &normFrom, &normTo);
            printf("%d-%d.geojson", normFrom, normTo);
            if (j < p->edgeCount - 1) printf("\\n");
        }
        printf("\",\n");
        printf("    \"totalDistance\": %.2f,\n", p->totalDistance);
        printf("    \"totalTime\": %.2f,\n", p->totalTimeWithExpected);
        printf("    \"totalWaitTime\": %.2f\n", p->totalWaitTime);
        printf("  }");
        if (i < result->pathCount - 1) printf(",");
        printf("\n");
    }
    printf("]\n");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <start_node> <end_node> <walking_speed>\n", argv[0]);
        return 1;
    }
    
    // グローバル変数に直接設定（フィルター関数で使用されるため）
    startNode = atoi(argv[1]);
    endNode = atoi(argv[2]);
    walkingSpeed = atof(argv[3]);
    
    if (startNode < 1 || startNode >= MAX_NODES || endNode < 1 || endNode >= MAX_NODES) {
        fprintf(stderr, "Error: Invalid node numbers\n");
        return 1;
    }
    
    fprintf(stderr, "Start node: %d, End node: %d\n", startNode, endNode);
    
    // グラフを初期化
    initGraph();
    
    // result.csvからグラフを構築
    loadGraphFromResult("result.csv");
    
    // エッジデータ（距離、勾配、信号、横断歩道情報）を読み込む
    loadRouteData("oomiya_route_inf_4.csv");
    
    // ノード座標を読み込む（フィルター用）
    fprintf(stderr, "Loading node coordinates...\n");
    loadNodeCoordinates("oomiya_point");
    
    // 信号情報を読み込む
    fprintf(stderr, "Loading signal data...\n");
    loadSignalData("signal_inf.csv");
    fprintf(stderr, "Loaded %d signals total\n", signalCount);
    
    // 読み込んだ信号エッジの詳細を出力
    for (int i = 0; i < signalCount; i++) {
        int edgeIdx = signalEdges[i];
        EdgeData *edge = &edgeDataArray[edgeIdx];
        fprintf(stderr, "Signal %d: edgeIdx=%d, from=%d, to=%d, cycle=%d, green=%d, phase=%.2f\n",
                i+1, edgeIdx, edge->from, edge->to, edge->signalCycle, edge->signalGreen, edge->signalPhase);
        
        // グラフに接続されているか確認
        bool fromConnected = false;
        bool toConnected = false;
        for (int j = 0; j < graph[edge->from].edge_count; j++) {
            if (graph[edge->from].edges[j].node == edge->to) {
                fromConnected = true;
                break;
            }
        }
        for (int j = 0; j < graph[edge->to].edge_count; j++) {
            if (graph[edge->to].edges[j].node == edge->from) {
                toConnected = true;
                break;
            }
        }
        fprintf(stderr, "  Graph connection: from->to=%s, to->from=%s\n",
                fromConnected ? "YES" : "NO", toConnected ? "YES" : "NO");
    }
    
    // 信号の組み合わせに基づいて経路を生成
    YenResult result = generateRoutesBySignalCombinations(startNode, endNode);
    
    // JSON形式で出力
    printJSONResult(&result);
    
    return 0;
}

