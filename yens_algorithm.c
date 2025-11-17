/* イェンのアルゴリズムを用いたK最短経路探索（信号待ち時間考慮） */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>

#define MAX_NODES 300
#define MAX_EDGES 1000
#define MAX_PATH_LENGTH 200
#define MAX_K 5
#define MAX_SIGNALS 50  // 信号エッジの最大数（signal_inf.csvには37個あるため余裕を持たせる）
#define MAX_COMBINATIONS 511  // 2^9 - 1 = 511通り（9個の信号を想定）
#define INF DBL_MAX
#define K_GRADIENT 0.5
#define AVERAGE_CAR_INTERVAL 15.0
#define SAFE_CROSSING_GAP 5.0

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

// グローバル変数
GraphNode graph[MAX_NODES];
EdgeData edgeDataArray[MAX_EDGES];
int edgeDataCount = 0;
double walkingSpeed = 80.0; // m/min
int signalEdges[MAX_SIGNALS];  // 信号エッジのインデックスリスト
int signalCount = 0;  // 信号の数

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

// 信号待ち時間の期待値を計算（秒）- 全ての信号で同じ値を使用
double calculateSignalWaitTimeExpected(int edgeIdx) {
    EdgeData *edge = &edgeDataArray[edgeIdx];
    if (!edge->isSignal || edge->signalCycle <= 0) return 0.0;
    
    double redTime = edge->signalCycle - edge->signalGreen;
    if (redTime <= 0) return 0.0;
    
    // 期待値として (redTime^2) / (2 * cycle) を返す
    return (redTime * redTime) / (2.0 * edge->signalCycle);
}

// 全ての信号の平均的な期待値を計算（秒）
double calculateAverageSignalWaitTimeExpected() {
    if (signalCount == 0) return 0.0;
    
    double totalExpected = 0.0;
    int count = 0;
    for (int i = 0; i < signalCount; i++) {
        double expected = calculateSignalWaitTimeExpected(signalEdges[i]);
        if (expected > 0) {
            totalExpected += expected;
            count++;
        }
    }
    
    return count > 0 ? totalExpected / count : 0.0;
}

// 信号待ち時間を計算（秒）- 期待値を使用（全ての信号で同じ）
double calculateSignalWaitTime(int edgeIdx, double arrivalTimeSeconds) {
    // 期待値を使用（全ての信号で同じ）
    static double averageExpected = -1.0;
    if (averageExpected < 0) {
        averageExpected = calculateAverageSignalWaitTimeExpected();
    }
    return averageExpected;
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
                waitTime = calculateSignalWaitTime(edgeIdx, newArrivalTime);
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

// イェンのアルゴリズム（信号組み合わせ用に拡張）
typedef struct {
    Path paths[MAX_COMBINATIONS];  // 511通り全てを保存可能に
    int pathCount;
} YenResult;

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
        
        char edgeKey[64];
        int cycle, green;
        double phase;
        
        // signal_inf.csvのフォーマット: node1,node2,cycle,green,phase
        // cycle, green, phaseは浮動小数点数として読み込む
        int from, to;
        double cycle_d, green_d;
        if (sscanf(line, "%d,%d,%lf,%lf,%lf", &from, &to, &cycle_d, &green_d, &phase) == 5) {
            // 浮動小数点数を整数に変換（cycle, greenは整数として扱う）
            cycle = (int)cycle_d;
            green = (int)green_d;
            int edgeIdx = findEdgeIndex(from, to);
            if (edgeIdx >= 0) {
                edgeDataArray[edgeIdx].signalCycle = cycle;
                edgeDataArray[edgeIdx].signalGreen = green;
                edgeDataArray[edgeIdx].signalPhase = phase;
                
                // 信号エッジのリストに追加（重複を避ける）
                // signal_inf.csvに含まれる信号エッジのみを追加
                if (!seenEdges[edgeIdx] && signalCount < MAX_SIGNALS) {
                    signalEdges[signalCount++] = edgeIdx;
                    seenEdges[edgeIdx] = true;
                    fprintf(stderr, "Signal %d: edge %d (%d-%d) cycle=%d green=%d phase=%.2f\n", 
                            signalCount, edgeIdx, from, to, cycle, green, phase);
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
    
    // 信号数が少ない場合は全ての順列を生成
    // 信号数が多い場合は、最大24個の順列を生成（4! = 24）
    int maxPerms = (signalCount <= 4) ? 24 : 24;
    
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
    
    // 各信号を順番に通る
    for (int i = 0; i < signalCount; i++) {
        int signalEdgeIdx = signalEdgeIndices[i];
        EdgeData *signalEdge = &edgeDataArray[signalEdgeIdx];
        int signalFrom = signalEdge->from;
        int signalTo = signalEdge->to;
        
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
            
            // 信号エッジの通過時間を加算
            double signalEdgeTime = getEdgeTimeSeconds(signalFrom, signalTo);
            if (signalEdgeTime < INF) {
                result.cost += signalEdgeTime;
            }
        } else {
            // 信号エッジがグラフに存在しない場合、警告を出力
            fprintf(stderr, "Warning: Signal edge %d (%d-%d) not found in graph, skipping\n", 
                    signalEdgeIdx, signalFrom, signalTo);
            // 信号エッジを通らないが、経路は続行
        }
        
        result.cost += segment.cost;
        currentNode = reverseDirection ? signalFrom : signalTo;
    }
    
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
    
    result.cost += finalSegment.cost;
    
    return result;
}

// 信号の組み合わせに基づいて経路を生成
YenResult generateRoutesBySignalCombinations(int start, int end) {
    YenResult result;
    result.pathCount = 0;
    
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
    
    // 信号待ち時間の期待値（全ての信号で同じ）
    double signalWaitTimeExpected = calculateAverageSignalWaitTimeExpected();
    
    // 信号の組み合わせを生成（2^signalCount - 1通り、全て通らない組み合わせを除く）
    // 9個の信号の場合、2^9 - 1 = 511通り
    int maxCombinations = (1 << signalCount) - 1;  // 2^signalCount - 1
    
    // 経路の重複チェック用
    bool seenPaths[MAX_COMBINATIONS][MAX_PATH_LENGTH];
    int seenPathLengths[MAX_COMBINATIONS];
    int seenPathCount = 0;
    
    // 511通り全ての組み合わせを試行
    for (int combination = 1; combination <= maxCombinations && result.pathCount < MAX_COMBINATIONS; combination++) {
        // この組み合わせに含まれる信号エッジのリストを取得
        int requiredSignals[MAX_SIGNALS];
        int requiredSignalCount = 0;
        for (int i = 0; i < signalCount; i++) {
            if (combination & (1 << i)) {
                requiredSignals[requiredSignalCount++] = signalEdges[i];
            }
        }
        
        // バリエーションを最大化するため、可能な限り多くの順序を試す
        int permutations[24][MAX_SIGNALS];  // 最大24個の順列
        int permCount = 0;
        generateSignalPermutations(requiredSignals, requiredSignalCount, permutations, &permCount);
        
        // 各順列について経路を探索（最低1つは見つけるまで試す）
        bool foundPath = false;
        for (int permIdx = 0; permIdx < permCount && result.pathCount < MAX_COMBINATIONS; permIdx++) {
            int orderedSignals[MAX_SIGNALS];
            for (int i = 0; i < requiredSignalCount; i++) {
                orderedSignals[i] = permutations[permIdx][i];
            }
            
            // この順序で信号を通る経路を探索
            DijkstraResult path = findPathThroughSignalsInOrder(start, end, orderedSignals, requiredSignalCount);
            
            if (path.cost < INF && path.pathLength > 0) {
                // 経路に含まれる信号エッジを確認
                bool signalsInPath[MAX_SIGNALS] = {false};
                int signalsFoundCount = 0;
                
                for (int k = 0; k < path.pathLength; k++) {
                    for (int s = 0; s < requiredSignalCount; s++) {
                        // エッジインデックスで直接比較
                        if (path.path[k] == orderedSignals[s]) {
                            if (!signalsInPath[s]) {
                                signalsInPath[s] = true;
                                signalsFoundCount++;
                            }
                        }
                        // グラフのエッジインデックスと信号エッジインデックスが異なる可能性があるため、
                        // エッジデータで比較
                        EdgeData *pathEdge = &edgeDataArray[path.path[k]];
                        EdgeData *requiredSignalEdge = &edgeDataArray[orderedSignals[s]];
                        if ((pathEdge->from == requiredSignalEdge->from && pathEdge->to == requiredSignalEdge->to) ||
                            (pathEdge->from == requiredSignalEdge->to && pathEdge->to == requiredSignalEdge->from)) {
                            if (!signalsInPath[s]) {
                                signalsInPath[s] = true;
                                signalsFoundCount++;
                            }
                        }
                    }
                }
                
                // 全ての必要な信号が経路に含まれているか確認
                if (signalsFoundCount == requiredSignalCount) {
                    foundPath = true;
                } else {
                    // 一部の信号が経路に含まれていない場合、警告を出力して経路を破棄
                    fprintf(stderr, "Warning: Only %d/%d signals found in path for combination %d\n", 
                            signalsFoundCount, requiredSignalCount, combination);
                    for (int s = 0; s < requiredSignalCount; s++) {
                        if (!signalsInPath[s]) {
                            EdgeData *signalEdge = &edgeDataArray[orderedSignals[s]];
                            fprintf(stderr, "  Missing signal edge %d (%d-%d)\n", 
                                    orderedSignals[s], signalEdge->from, signalEdge->to);
                        }
                    }
                    // 全ての信号が含まれていない場合は経路を破棄
                    continue;
                }
                
                // 重複チェック
                bool isDuplicate = false;
                for (int p = 0; p < seenPathCount; p++) {
                    if (seenPathLengths[p] == path.pathLength) {
                        bool same = true;
                        for (int j = 0; j < path.pathLength; j++) {
                            if (seenPaths[p][j] != path.path[j]) {
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
                
                if (!isDuplicate) {
                    // 経路メトリクスを計算（信号待ち時間は期待値として追加）
                    RouteMetrics metrics = calculateRouteMetrics(path.path, path.pathLength);
                    
                    // 信号待ち時間を期待値として追加（通る信号の数 × 期待値）
                    double totalSignalWaitTime = signalWaitTimeExpected * requiredSignalCount / 60.0;  // 分に変換
                    
                    Path *p = &result.paths[result.pathCount];
                    memcpy(p->edges, path.path, path.pathLength * sizeof(int));
                    p->edgeCount = path.pathLength;
                    p->totalDistance = metrics.totalDistance;
                    p->totalWaitTime = totalSignalWaitTime;
                    p->totalTime = metrics.totalTime;  // 移動時間のみ（信号待ち時間なし）
                    p->totalTimeWithExpected = metrics.totalTime + totalSignalWaitTime;  // 移動時間 + 信号待ち時間
                    
                    // 重複チェック用に保存
                    memcpy(seenPaths[seenPathCount], path.path, path.pathLength * sizeof(int));
                    seenPathLengths[seenPathCount] = path.pathLength;
                    seenPathCount++;
                    
                    result.pathCount++;
                }
            }
        }
        
        // 経路が見つからない場合は警告を出力（デバッグ用）
        if (!foundPath && requiredSignalCount > 0) {
            fprintf(stderr, "Warning: No path found for combination %d with %d signals\n", combination, requiredSignalCount);
        }
    }
    
    fprintf(stderr, "Generated %d routes total\n", result.pathCount);
    
    // 全ての経路を総推定時間でソート（最短のものを最初に）
    for (int i = 0; i < result.pathCount - 1; i++) {
        for (int j = i + 1; j < result.pathCount; j++) {
            if (result.paths[i].totalTimeWithExpected > result.paths[j].totalTimeWithExpected) {
                // 経路を交換
                Path temp = result.paths[i];
                result.paths[i] = result.paths[j];
                result.paths[j] = temp;
            }
        }
    }
    
    return result;
}

// JSON形式で結果を出力（全経路を表示）
void printJSONResult(YenResult *result) {
    printf("[\n");
    for (int i = 0; i < result->pathCount; i++) {
        Path *p = &result->paths[i];
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
    
    int startNode = atoi(argv[1]);
    int endNode = atoi(argv[2]);
    walkingSpeed = atof(argv[3]);
    
    if (startNode < 1 || startNode >= MAX_NODES || endNode < 1 || endNode >= MAX_NODES) {
        fprintf(stderr, "Error: Invalid node numbers\n");
        return 1;
    }
    
    // グラフを初期化
    initGraph();
    
    // result.csvからグラフを構築
    loadGraphFromResult("result.csv");
    
    // エッジデータ（距離、勾配、信号、横断歩道情報）を読み込む
    loadRouteData("oomiya_route_inf_4.csv");
    
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

