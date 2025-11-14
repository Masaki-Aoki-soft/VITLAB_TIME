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

// 信号待ち時間を計算（秒）
double calculateSignalWaitTime(int edgeIdx, double arrivalTimeSeconds) {
    EdgeData *edge = &edgeDataArray[edgeIdx];
    if (!edge->isSignal || edge->signalCycle <= 0) return 0.0;
    
    double redTime = edge->signalCycle - edge->signalGreen;
    if (redTime <= 0) return 0.0;
    
    double arrivalTimeInCycle = fmod(
        arrivalTimeSeconds - edge->signalPhase + edge->signalCycle,
        edge->signalCycle
    );
    
    if (arrivalTimeInCycle > edge->signalGreen) {
        return edge->signalCycle - arrivalTimeInCycle;
    } else {
        return (redTime * redTime) / (2.0 * edge->signalCycle);
    }
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
    double cumulativeTime = 0.0;
    
    for (int i = 0; i < edgeCount; i++) {
        EdgeData *edge = &edgeDataArray[edgeIndices[i]];
        metrics.totalDistance += edge->distance;
        
        double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * edge->gradient);
        if (adjustedSpeed > 0) {
            double travelTimeSeconds = (edge->distance / adjustedSpeed) * 60.0;
            metrics.totalTime += edge->distance / adjustedSpeed;
            cumulativeTime += travelTimeSeconds;
            
            if (edge->isSignal) {
                double waitTime = calculateSignalWaitTime(edgeIndices[i], cumulativeTime);
                metrics.totalExpectedWaitTimeMinutes += waitTime / 60.0;
                cumulativeTime += waitTime;
            } else if (edge->isCrosswalk) {
                double waitTime = calculateCrosswalkWaitTime(edgeIndices[i], cumulativeTime);
                metrics.totalExpectedWaitTimeMinutes += waitTime / 60.0;
                cumulativeTime += waitTime;
            }
        }
    }
    
    metrics.totalTimeWithExpected = metrics.totalTime + metrics.totalExpectedWaitTimeMinutes;
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

// イェンのアルゴリズム
typedef struct {
    Path paths[MAX_K];
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
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        
        char edgeKey[64];
        int cycle, green;
        double phase;
        
        if (sscanf(line, "%[^,],%d,%d,%lf", edgeKey, &cycle, &green, &phase) == 4) {
            int from, to;
            if (sscanf(edgeKey, "%d-%d", &from, &to) == 2) {
                int edgeIdx = findEdgeIndex(from, to);
                if (edgeIdx >= 0) {
                    edgeDataArray[edgeIdx].signalCycle = cycle;
                    edgeDataArray[edgeIdx].signalGreen = green;
                    edgeDataArray[edgeIdx].signalPhase = phase;
                }
            }
        }
    }
    
    fclose(file);
}

// JSON形式で結果を出力
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
    loadSignalData("signal_inf.csv");
    
    // 基準経路を探索（ダイクストラ法）
    DijkstraResult basePath = dijkstraWithSignals(startNode, endNode, 0.0, NULL);
    
    if (basePath.cost >= INF || basePath.pathLength == 0) {
        printf("[]\n");
        return 0;
    }
    
    // イェンのアルゴリズムでK最短経路を探索
    YenResult result = yensAlgorithm(startNode, endNode, basePath.path, 
                                     basePath.pathLength, MAX_K);
    
    // JSON形式で出力
    printJSONResult(&result);
    
    return 0;
}

