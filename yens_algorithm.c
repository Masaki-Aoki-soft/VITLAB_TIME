/* シンプル版: signal_inf.csv の信号を使って
 * スタート→信号→ゴールの経路を網羅的に列挙する
 * - イェンのアルゴリズム・基準1/2 等はすべて削除
 * - 信号待ち時間も考慮せず、「距離＋勾配による歩行時間」のみを使う
 */

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_NODES       300
#define MAX_EDGES       1000
#define MAX_PATH_LENGTH 200
#define MAX_SIGNALS     50   // signal_inf.csv 側の最大信号数を想定

#define INF DBL_MAX
#define K_GRADIENT 0.5
#define DEFAULT_WALKING_SPEED 80.0  // m/min

/* ---------- データ構造 ---------- */

typedef struct {
    int from;
    int to;
    double distance;
    double gradient;
    int isSignal;
    double signalCycle;
    double signalGreen;
    double signalPhase;
    double signalExpected;  // 期待待ち時間
} EdgeData;

typedef struct {
    double lat;  // 緯度
    double lon;  // 経度
} NodePosition;

typedef struct {
    int node;
    int edgeIndex;  // EdgeData 配列のインデックス
} GraphEdge;

typedef struct {
    GraphEdge edges[8];  // ノードあたり最大8本程度と仮定
    int edge_count;
} GraphNode;

typedef struct {
    double cost;                 // 秒
    int path[MAX_PATH_LENGTH];   // edgeIndex の列
    int pathLength;
} DijkstraResult;

typedef struct {
    int signalEdgeIdx;           // どの信号か (-1の場合は信号なし)
    int edges[MAX_PATH_LENGTH];  // 経路のエッジ列
    int edgeCount;
    double totalDistance;        // m
    double totalTimeSeconds;     // 秒
    int routeType;               // 0: 基準時刻1（青）, 1: 基準時刻2（緑）, 2: その他（赤）
    int hasSignal;               // 経路に信号が含まれるか (1: 含む, 0: 含まない)
} RouteResult;

/* ---------- グローバル ---------- */

GraphNode graph[MAX_NODES];
EdgeData  edgeDataArray[MAX_EDGES];
int       edgeDataCount = 0;

int   signalEdges[MAX_SIGNALS];
int   signalCount = 0;

NodePosition nodePositions[MAX_NODES];  // ノード位置情報

double walkingSpeed = DEFAULT_WALKING_SPEED; // m/min

/* ---------- 共通ユーティリティ ---------- */

void initGraph(void) {
    for (int i = 0; i < MAX_NODES; i++) {
        graph[i].edge_count = 0;
    }
}

void normalizeEdgeKey(int from, int to, int *outFrom, int *outTo) {
    if (from < to) {
        *outFrom = from;
        *outTo   = to;
    } else {
        *outFrom = to;
        *outTo   = from;
    }
}

// EdgeData 配列から (from,to) に対応する edgeIndex を探す
int findEdgeIndex(int from, int to) {
    int nf, nt;
    normalizeEdgeKey(from, to, &nf, &nt);

    for (int i = 0; i < edgeDataCount; i++) {
        int ef, et;
        normalizeEdgeKey(edgeDataArray[i].from, edgeDataArray[i].to, &ef, &et);
        if (ef == nf && et == nt) return i;
    }
    return -1;
}

// エッジの移動時間（秒）
double getEdgeTimeSeconds(int from, int to) {
    int edgeIdx = findEdgeIndex(from, to);
    if (edgeIdx < 0) return INF;

    EdgeData *e = &edgeDataArray[edgeIdx];
    // 勾配による速度補正（元コードと同じロジック）
    double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * e->gradient);
    if (adjustedSpeed <= 0.0) return INF;

    double timeMinutes = e->distance / adjustedSpeed;  // 分
    return timeMinutes * 60.0;                         // 秒
}

// 前方宣言
double calculateBearing(double lat1, double lon1, double lat2, double lon2);
bool isWithinAngleRange(double angle1, double angle2, double tolerance);
bool appendSegment(RouteResult *res, const DijkstraResult *seg);
void getTargetSignalEdges(int *targetSignalIndices, int *targetCount);

// 基準時刻1を計算する関数（信号を避けた最短経路、方角±60度制約あり）
bool calculateBaseTime1(int startNode, int endNode, double targetBearing, RouteResult *outRoute);

// 基準時刻2を計算する関数（信号を通る最短経路、待ち時間0、方角制約なし）
bool calculateBaseTime2(int startNode, int endNode, RouteResult *outRoute);

// 全網羅経路を計算する関数（12個の信号から1個、2個、3個の組み合わせを全て探索）
int calculateAllEnumRoutes(int startNode, int endNode, int signalCount, RouteResult *outRoutes, int maxRoutes);

/* ---------- ダイクストラ（信号制限なし・待ち時間なし） ---------- */

// 指定された信号エッジを避ける方角制約付きダイクストラ
DijkstraResult dijkstraAvoidTargetSignals(int start, int goal, double targetBearing, int *avoidEdgeIndices, int avoidCount) {
    double dist[MAX_NODES];
    int    prev[MAX_NODES];
    bool   used[MAX_NODES];
    // 方角制約を使用するかどうか：ノード位置情報が読み込まれていて、targetBearingが有効な場合
    bool   useAngleConstraint = (nodePositions[start].lat != 0.0 && nodePositions[goal].lat != 0.0);
    
    if (useAngleConstraint) {
        fprintf(stderr, "方角制約を使用: ターゲット方角=%.2f度, ±60度範囲\n", targetBearing);
    } else {
        fprintf(stderr, "方角制約を使用しない（ノード位置情報が不足）\n");
    }
    
    // 避けるべきエッジのセットを作成
    bool avoidEdgeSet[MAX_EDGES];
    for (int i = 0; i < MAX_EDGES; i++) {
        avoidEdgeSet[i] = false;
    }
    int validAvoidCount = 0;
    for (int i = 0; i < avoidCount; i++) {
        if (avoidEdgeIndices[i] >= 0 && avoidEdgeIndices[i] < MAX_EDGES) {
            avoidEdgeSet[avoidEdgeIndices[i]] = true;
            validAvoidCount++;
        }
    }
    fprintf(stderr, "避けるべき信号エッジ: %d個設定\n", validAvoidCount);

    for (int i = 0; i < MAX_NODES; i++) {
        dist[i] = INF;
        prev[i] = -1;
        used[i] = false;
    }
    dist[start] = 0.0;

    int visitedCount = 0;
    int skippedByAngleCount = 0;
    int skippedBySignalCount = 0;
    
    while (1) {
        int    u   = -1;
        double d_u = INF;
        for (int i = 0; i < MAX_NODES; i++) {
            if (!used[i] && dist[i] < d_u) {
                d_u = dist[i];
                u   = i;
            }
        }
        if (u == -1 || u == goal) break;
        used[u] = true;
        visitedCount++;

        for (int i = 0; i < graph[u].edge_count; i++) {
            int v       = graph[u].edges[i].node;
            int edgeIdx = graph[u].edges[i].edgeIndex;
            if (used[v]) continue;
            
            EdgeData *e = &edgeDataArray[edgeIdx];
            
            // 指定された信号エッジのみを避ける（他の信号は通ってもよい）
            if (avoidEdgeSet[edgeIdx]) {
                skippedBySignalCount++;
                continue;
            }
            
            // 方角制約チェック（useAngleConstraintがtrueかつノード位置情報が読み込まれている場合のみ）
            // より柔軟な判定：エッジの方向またはvからgoalへの方向が範囲内であれば許可
            if (useAngleConstraint && nodePositions[u].lat != 0.0 && nodePositions[v].lat != 0.0) {
                // エッジの方向をチェック
                double edgeBearing = calculateBearing(nodePositions[u].lat, nodePositions[u].lon,
                                                      nodePositions[v].lat, nodePositions[v].lon);
                bool edgeOk = isWithinAngleRange(edgeBearing, targetBearing, 60.0);
                
                // エッジが範囲外の場合、vからgoalへの方向もチェック（より柔軟な判定）
                if (!edgeOk && nodePositions[goal].lat != 0.0) {
                    double toGoalBearing = calculateBearing(nodePositions[v].lat, nodePositions[v].lon,
                                                            nodePositions[goal].lat, nodePositions[goal].lon);
                    // vからgoalへの方角がターゲット方角に近い場合は許可
                    edgeOk = isWithinAngleRange(toGoalBearing, targetBearing, 60.0);
                }
                
                if (!edgeOk) {
                    skippedByAngleCount++;
                    continue;  // 方角が範囲外
                }
            }

            double t = getEdgeTimeSeconds(u, v);
            if (t >= INF) continue;

            double nd = dist[u] + t;
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u;
            }
        }
    }
    
    fprintf(stderr, "探索統計: 訪問ノード数=%d, 方角制約でスキップ=%d, 信号制約でスキップ=%d\n",
            visitedCount, skippedByAngleCount, skippedBySignalCount);

    DijkstraResult res;
    res.cost       = dist[goal];
    res.pathLength = 0;

    if (dist[goal] >= INF) {
        return res;
    }

    // ノード列を復元 → エッジ列に変換
    int nodes[MAX_PATH_LENGTH];
    int nodeCount = 0;
    int cur       = goal;

    while (cur != -1 && cur != start && nodeCount < MAX_PATH_LENGTH) {
        nodes[nodeCount++] = cur;
        cur = prev[cur];
    }
    if (cur == -1) {
        res.cost       = INF;
        res.pathLength = 0;
        return res;
    }
    nodes[nodeCount++] = start;

    // 逆順でエッジに
    for (int i = nodeCount - 1; i > 0; i--) {
        int from = nodes[i];
        int to   = nodes[i - 1];
        int edgeIdx = findEdgeIndex(from, to);
        if (edgeIdx < 0 || avoidEdgeSet[edgeIdx]) {
            res.cost       = INF;
            res.pathLength = 0;
            return res;
        }
        res.path[res.pathLength++] = edgeIdx;
    }

    return res;
}

// 方角制約付きダイクストラ（信号エッジを除外、方角±60度以内）
DijkstraResult dijkstraWithAngleConstraint(int start, int goal, double targetBearing, bool avoidSignals) {
    double dist[MAX_NODES];
    int    prev[MAX_NODES];
    bool   used[MAX_NODES];
    bool   useAngleConstraint = (targetBearing != 0.0 || nodePositions[start].lat != 0.0);

    for (int i = 0; i < MAX_NODES; i++) {
        dist[i] = INF;
        prev[i] = -1;
        used[i] = false;
    }
    dist[start] = 0.0;

    while (1) {
        int    u   = -1;
        double d_u = INF;
        for (int i = 0; i < MAX_NODES; i++) {
            if (!used[i] && dist[i] < d_u) {
                d_u = dist[i];
                u   = i;
            }
        }
        if (u == -1 || u == goal) break;
        used[u] = true;

        for (int i = 0; i < graph[u].edge_count; i++) {
            int v       = graph[u].edges[i].node;
            int edgeIdx = graph[u].edges[i].edgeIndex;
            if (used[v]) continue;
            
            EdgeData *e = &edgeDataArray[edgeIdx];
            
            // 信号エッジを避ける
            if (avoidSignals && e->isSignal) continue;
            
            // 方角制約チェック（useAngleConstraintがtrueかつノード位置情報が読み込まれている場合のみ）
            if (useAngleConstraint && nodePositions[u].lat != 0.0 && nodePositions[v].lat != 0.0) {
                double edgeBearing = calculateBearing(nodePositions[u].lat, nodePositions[u].lon,
                                                      nodePositions[v].lat, nodePositions[v].lon);
                if (!isWithinAngleRange(edgeBearing, targetBearing, 60.0)) {
                    continue;  // 方角が範囲外
                }
            }

            double t = getEdgeTimeSeconds(u, v);
            if (t >= INF) continue;

            double nd = dist[u] + t;
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u;
            }
        }
    }

    DijkstraResult res;
    res.cost       = dist[goal];
    res.pathLength = 0;

    if (dist[goal] >= INF) {
        return res;
    }

    // ノード列を復元 → エッジ列に変換
    int nodes[MAX_PATH_LENGTH];
    int nodeCount = 0;
    int cur       = goal;

    while (cur != -1 && cur != start && nodeCount < MAX_PATH_LENGTH) {
        nodes[nodeCount++] = cur;
        cur = prev[cur];
    }
    if (cur == -1) {
        res.cost       = INF;
        res.pathLength = 0;
        return res;
    }
    nodes[nodeCount++] = start;

    // 逆順でエッジに
    for (int i = nodeCount - 1; i > 0; i--) {
        int from = nodes[i];
        int to   = nodes[i - 1];
        int edgeIdx = findEdgeIndex(from, to);
        if (edgeIdx < 0) {
            res.cost       = INF;
            res.pathLength = 0;
            return res;
        }
        res.path[res.pathLength++] = edgeIdx;
    }

    return res;
}

// 信号エッジを除外したダイクストラ
DijkstraResult dijkstraAvoidSignal(int start, int goal, int avoidEdgeIdx) {
    double dist[MAX_NODES];
    int    prev[MAX_NODES];
    bool   used[MAX_NODES];

    for (int i = 0; i < MAX_NODES; i++) {
        dist[i] = INF;
        prev[i] = -1;
        used[i] = false;
    }
    dist[start] = 0.0;

    while (1) {
        int    u   = -1;
        double d_u = INF;
        for (int i = 0; i < MAX_NODES; i++) {
            if (!used[i] && dist[i] < d_u) {
                d_u = dist[i];
                u   = i;
            }
        }
        if (u == -1 || u == goal) break;
        used[u] = true;

        for (int i = 0; i < graph[u].edge_count; i++) {
            int v       = graph[u].edges[i].node;
            int edgeIdx = graph[u].edges[i].edgeIndex;
            if (used[v]) continue;
            
            // 避けるべき信号エッジをスキップ
            if (edgeIdx == avoidEdgeIdx) continue;

            double t = getEdgeTimeSeconds(u, v);
            if (t >= INF) continue;

            double nd = dist[u] + t;
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u;
            }
        }
    }

    DijkstraResult res;
    res.cost       = dist[goal];
    res.pathLength = 0;

    if (dist[goal] >= INF) {
        return res;
    }

    // ノード列を復元 → エッジ列に変換
    int nodes[MAX_PATH_LENGTH];
    int nodeCount = 0;
    int cur       = goal;

    while (cur != -1 && cur != start && nodeCount < MAX_PATH_LENGTH) {
        nodes[nodeCount++] = cur;
        cur = prev[cur];
    }
    if (cur == -1) {
        res.cost       = INF;
        res.pathLength = 0;
        return res;
    }
    nodes[nodeCount++] = start;

    // 逆順でエッジに
    for (int i = nodeCount - 1; i > 0; i--) {
        int from = nodes[i];
        int to   = nodes[i - 1];
        int edgeIdx = findEdgeIndex(from, to);
        if (edgeIdx < 0 || edgeIdx == avoidEdgeIdx) {
            res.cost       = INF;
            res.pathLength = 0;
            return res;
        }
        res.path[res.pathLength++] = edgeIdx;
    }

    return res;
}

DijkstraResult dijkstra(int start, int goal) {
    double dist[MAX_NODES];
    int    prev[MAX_NODES];
    bool   used[MAX_NODES];

    for (int i = 0; i < MAX_NODES; i++) {
        dist[i] = INF;
        prev[i] = -1;
        used[i] = false;
    }
    dist[start] = 0.0;

    while (1) {
        int    u   = -1;
        double d_u = INF;
        for (int i = 0; i < MAX_NODES; i++) {
            if (!used[i] && dist[i] < d_u) {
                d_u = dist[i];
                u   = i;
            }
        }
        if (u == -1 || u == goal) break;
        used[u] = true;

        for (int i = 0; i < graph[u].edge_count; i++) {
            int v       = graph[u].edges[i].node;
            int edgeIdx = graph[u].edges[i].edgeIndex;
            if (used[v]) continue;

            double t = getEdgeTimeSeconds(u, v);
            if (t >= INF) continue;

            double nd = dist[u] + t;
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u;
            }
        }
    }

    DijkstraResult res;
    res.cost       = dist[goal];
    res.pathLength = 0;

    if (dist[goal] >= INF) {
        return res;
    }

    // ノード列を復元 → エッジ列に変換
    int nodes[MAX_PATH_LENGTH];
    int nodeCount = 0;
    int cur       = goal;

    while (cur != -1 && cur != start && nodeCount < MAX_PATH_LENGTH) {
        nodes[nodeCount++] = cur;
        cur = prev[cur];
    }
    if (cur == -1) {
        res.cost       = INF;
        res.pathLength = 0;
        return res;
    }
    nodes[nodeCount++] = start;

    // 逆順でエッジに
    for (int i = nodeCount - 1; i > 0; i--) {
        int from = nodes[i];
        int to   = nodes[i - 1];
        int edgeIdx = findEdgeIndex(from, to);
        if (edgeIdx < 0) {
            res.cost       = INF;
            res.pathLength = 0;
            return res;
        }
        res.path[res.pathLength++] = edgeIdx;
    }

    return res;
}

/* ---------- メトリクス計算 ---------- */

// 待ち時間を含めたメトリクス計算（待ち時間を0にする場合と期待値を使う場合）
void calcRouteMetricsWithWaitTime(const int *edgeIdxs, int edgeCount,
                                   double *outDist, double *outTimeSec, bool useExpectedWaitTime) {
    double totalDist = 0.0;
    double totalTime = 0.0;

    for (int i = 0; i < edgeCount; i++) {
        int idx = edgeIdxs[i];
        if (idx < 0 || idx >= edgeDataCount) continue;
        EdgeData *e = &edgeDataArray[idx];
        totalDist += e->distance;

        double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * e->gradient);
        if (adjustedSpeed <= 0.0) continue;
        double timeMinutes = e->distance / adjustedSpeed;
        totalTime += timeMinutes * 60.0;  // 秒
        
        // 信号エッジの場合、待ち時間を追加
        if (e->isSignal) {
            if (useExpectedWaitTime) {
                totalTime += e->signalExpected * 60.0;  // 期待待ち時間を秒に変換
            }
            // useExpectedWaitTimeがfalseの場合は待ち時間0（追加しない）
        }
    }

    *outDist    = totalDist;
    *outTimeSec = totalTime;
}

void calcRouteMetrics(const int *edgeIdxs, int edgeCount,
                      double *outDist, double *outTimeSec) {
    calcRouteMetricsWithWaitTime(edgeIdxs, edgeCount, outDist, outTimeSec, false);
}

/* ---------- ファイル読み込み ---------- */

// result.csv: "from,to,weight" を想定（weight は未使用でもよい）
void loadGraphFromResult(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", filename);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '\n' || line[0] == '\0') continue;

        int from, to;
        double w;
        if (sscanf(line, "%d,%d,%lf", &from, &to, &w) != 3) continue;
        if (from <= 0 || from >= MAX_NODES || to <= 0 || to >= MAX_NODES) continue;

        int edgeIdx = findEdgeIndex(from, to);
        if (edgeIdx < 0 && edgeDataCount < MAX_EDGES) {
            edgeIdx = edgeDataCount++;
            edgeDataArray[edgeIdx].from      = from;
            edgeDataArray[edgeIdx].to        = to;
            edgeDataArray[edgeIdx].distance  = 0.0;
            edgeDataArray[edgeIdx].gradient  = 0.0;
            edgeDataArray[edgeIdx].isSignal  = 0;
        }

        // グラフ（双方向）に追加（重複は避ける）
        if (edgeIdx >= 0) {
            bool exist = false;
            for (int i = 0; i < graph[from].edge_count; i++) {
                if (graph[from].edges[i].node == to) {
                    exist = true;
                    break;
                }
            }
            if (!exist && graph[from].edge_count < 8) {
                graph[from].edges[graph[from].edge_count].node      = to;
                graph[from].edges[graph[from].edge_count].edgeIndex = edgeIdx;
                graph[from].edge_count++;
            }

            exist = false;
            for (int i = 0; i < graph[to].edge_count; i++) {
                if (graph[to].edges[i].node == from) {
                    exist = true;
                    break;
                }
            }
            if (!exist && graph[to].edge_count < 8) {
                graph[to].edges[graph[to].edge_count].node      = from;
                graph[to].edges[graph[to].edge_count].edgeIndex = edgeIdx;
                graph[to].edge_count++;
            }
        }
    }

    fclose(fp);
}

// oomiya_route_inf_4.csv: from,to,distance,time_minutes,gradient,...,isSignal,...
void loadRouteData(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", filename);
        return;
    }

    char line[1024];
    // ヘッダ行スキップ
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '\n' || line[0] == '\0') continue;

        char *tok = strtok(line, ",");
        if (!tok) continue;
        int from = atoi(tok);

        tok = strtok(NULL, ",");
        if (!tok) continue;
        int to = atoi(tok);

        tok = strtok(NULL, ",");
        if (!tok) continue;
        double dist = atof(tok);

        tok = strtok(NULL, ",");   // time_minutes はスキップ
        if (!tok) continue;

        tok = strtok(NULL, ",");   // gradient
        if (!tok) continue;
        double grad = atof(tok);

        // さらに先のカラムから isSignal を取得（元の位置に依存）
        // ここでは簡略化して、「次の数個を飛ばしてからフラグを読む」形
        int isSignal = 0;

        for (int i = 0; i < 3; i++) {
            tok = strtok(NULL, ",");
            if (!tok) break;
        }
        if (tok) {
            isSignal = atoi(tok);
        }

        if (from <= 0 || to <= 0) continue;

        int edgeIdx = findEdgeIndex(from, to);
        if (edgeIdx < 0 && edgeDataCount < MAX_EDGES) {
            edgeIdx = edgeDataCount++;
            edgeDataArray[edgeIdx].from = from;
            edgeDataArray[edgeIdx].to   = to;
        }

        if (edgeIdx >= 0) {
            edgeDataArray[edgeIdx].distance = dist;
            edgeDataArray[edgeIdx].gradient = grad;
            if (isSignal) edgeDataArray[edgeIdx].isSignal = 1;
            // 信号情報は後でloadSignalDataで上書きされる
            edgeDataArray[edgeIdx].signalCycle = 0.0;
            edgeDataArray[edgeIdx].signalGreen = 0.0;
            edgeDataArray[edgeIdx].signalPhase = 0.0;
            edgeDataArray[edgeIdx].signalExpected = 0.0;
        }
    }

    fclose(fp);
}

// signal_inf.csv: from,to,cycle,green,phase,expected
void loadSignalData(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Warning: cannot open %s\n", filename);
        return;
    }

    char line[1024];
    // ヘッダ行スキップ
    fgets(line, sizeof(line), fp);

    signalCount = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '\n' || line[0] == '\0') continue;

        int from, to;
        double cycle, green, phase, expected;
        int n = sscanf(line, "%d,%d,%lf,%lf,%lf,%lf",
                       &from, &to, &cycle, &green, &phase, &expected);
        if (n < 6) {
            fprintf(stderr, "Warning: failed to parse signal line: %s", line);
            continue;
        }

        int edgeIdx = findEdgeIndex(from, to);
        if (edgeIdx < 0) {
            fprintf(stderr,
                    "Warning: signal edge %d-%d not found in graph\n",
                    from, to);
            continue;
        }

        // 信号フラグと情報を保存
        edgeDataArray[edgeIdx].isSignal = 1;
        edgeDataArray[edgeIdx].signalCycle = cycle;
        edgeDataArray[edgeIdx].signalGreen = green;
        edgeDataArray[edgeIdx].signalPhase = phase;
        edgeDataArray[edgeIdx].signalExpected = expected;

        if (signalCount < MAX_SIGNALS) {
            signalEdges[signalCount++] = edgeIdx;
            fprintf(stderr,
                    "Signal %d: edge %d (%d-%d) cycle=%.0f green=%.0f phase=%.2f expected=%.2f\n",
                    signalCount, edgeIdx, from, to, cycle, green, phase, expected);
        }
    }

    fclose(fp);
    fprintf(stderr, "Loaded %d signals from signal_inf.csv\n", signalCount);
}

// GeoJSONからノード位置情報を読み込む
void loadNodePositions(void) {
    // 初期化：位置情報が読み込まれていないノードは0.0で初期化
    for (int i = 0; i < MAX_NODES; i++) {
        nodePositions[i].lat = 0.0;
        nodePositions[i].lon = 0.0;
    }
    
    // 各ノードのGeoJSONファイルを読み込む
    for (int nodeId = 1; nodeId < MAX_NODES; nodeId++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "oomiya_point/%d.geojson", nodeId);
        
        FILE *fp = fopen(filename, "r");
        if (!fp) continue;
        
        char line[1024];
        if (fgets(line, sizeof(line), fp)) {
            // 簡易JSONパース: coordinates を探す
            // {"type":"Feature","geometry":{"type":"Point","coordinates":[139.64066147804263,35.94875901101989]}
            char *coordsStart = strstr(line, "\"coordinates\":[");
            if (coordsStart) {
                double lon, lat;
                if (sscanf(coordsStart, "\"coordinates\":[%lf,%lf]", &lon, &lat) == 2) {
                    nodePositions[nodeId].lon = lon;
                    nodePositions[nodeId].lat = lat;
                }
            }
        }
        fclose(fp);
    }
}

// 2点間の方角（bearing）を計算（度）
double calculateBearing(double lat1, double lon1, double lat2, double lon2) {
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double lat1Rad = lat1 * M_PI / 180.0;
    double lat2Rad = lat2 * M_PI / 180.0;
    
    double y = sin(dLon) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dLon);
    
    double bearing = atan2(y, x) * 180.0 / M_PI;
    return fmod(bearing + 360.0, 360.0);  // 0-360度に正規化
}

// 角度差が±60度以内かチェック
bool isWithinAngleRange(double angle1, double angle2, double tolerance) {
    double diff = fabs(angle1 - angle2);
    if (diff > 180.0) diff = 360.0 - diff;
    return diff <= tolerance;
}

// 基準時刻1を計算する関数（信号を避けた最短経路、方角±60度制約あり）
// 指定された12個の信号以外の信号を通る経路も考慮する
bool calculateBaseTime1(int startNode, int endNode, double targetBearing, RouteResult *outRoute) {
    // 指定された12個の信号エッジのインデックスを取得（これらの信号は避ける）
    int targetSignalIndices[12];
    int targetSignalCount = 0;
    getTargetSignalEdges(targetSignalIndices, &targetSignalCount);
    
    fprintf(stderr, "基準時刻1探索: 方角±60度制約付きで信号を避けた経路を探索\n");
    fprintf(stderr, "指定された12個の信号を避けます。他の信号を通る経路も考慮します。\n");
    fprintf(stderr, "ターゲット方角: %.2f度, 指定信号数: %d個\n", targetBearing, targetSignalCount);
    
    // 指定された12個の信号のみを避けた経路を探索（他の信号は通ってもよい）
    // 方角±60度制約付きで探索
    DijkstraResult avoidSignalPath = dijkstraAvoidTargetSignals(startNode, endNode, targetBearing, 
                                                                 targetSignalIndices, targetSignalCount);
    
    fprintf(stderr, "基準時刻1探索結果: cost=%.2f, pathLength=%d\n", 
            avoidSignalPath.cost, avoidSignalPath.pathLength);
    
    if (avoidSignalPath.cost < INF) {
        RouteResult r;
        r.signalEdgeIdx = -1;
        r.edgeCount = 0;
        r.hasSignal = 0;
        
        if (appendSegment(&r, &avoidSignalPath)) {
            calcRouteMetrics(r.edges, r.edgeCount,
                             &r.totalDistance, &r.totalTimeSeconds);
            
            // 信号が含まれていないか確認
            for (int i = 0; i < r.edgeCount; i++) {
                int edgeIdx = r.edges[i];
                if (edgeIdx >= 0 && edgeIdx < edgeDataCount) {
                    if (edgeDataArray[edgeIdx].isSignal) {
                        r.hasSignal = 1;
                        r.signalEdgeIdx = edgeIdx;
                        fprintf(stderr, "警告: 基準時刻1の経路に信号が含まれています（edgeIdx=%d）\n", edgeIdx);
                        break;
                    }
                }
            }
            
            fprintf(stderr, "基準時刻1候補: edges=%d, distance=%.2f m, time=%.2f sec (%.2f min), hasSignal=%d\n",
                    r.edgeCount, r.totalDistance, r.totalTimeSeconds, r.totalTimeSeconds / 60.0, r.hasSignal);
            
            *outRoute = r;
            return true;
        }
    } else {
        fprintf(stderr, "基準時刻1: 方角±60度制約付きで経路が見つかりませんでした。方角制約なしで再探索します。\n");
        
        // 方角制約なしで信号を避けた最短経路を探索（フォールバック）
        DijkstraResult fallbackPath = dijkstra(startNode, endNode);
        
        // 信号が含まれていないか確認
        bool hasSignal = false;
        for (int i = 0; i < fallbackPath.pathLength; i++) {
            int edgeIdx = fallbackPath.path[i];
            if (edgeIdx >= 0 && edgeIdx < edgeDataCount) {
                if (edgeDataArray[edgeIdx].isSignal) {
                    hasSignal = true;
                    // 信号を避けて再探索
                    fallbackPath = dijkstraAvoidSignal(startNode, endNode, edgeIdx);
                    break;
                }
            }
        }
        
        // まだ信号が含まれている場合は、全信号を避けて探索
        if (hasSignal) {
            for (int i = 0; i < fallbackPath.pathLength; i++) {
                int edgeIdx = fallbackPath.path[i];
                if (edgeIdx >= 0 && edgeIdx < edgeDataCount) {
                    if (edgeDataArray[edgeIdx].isSignal) {
                        DijkstraResult testPath = dijkstraAvoidSignal(startNode, endNode, edgeIdx);
                        if (testPath.cost < fallbackPath.cost) {
                            fallbackPath = testPath;
                        }
                    }
                }
            }
        }
        
        if (fallbackPath.cost < INF) {
            RouteResult r;
            r.signalEdgeIdx = -1;
            r.edgeCount = 0;
            r.hasSignal = 0;
            
            if (appendSegment(&r, &fallbackPath)) {
                calcRouteMetrics(r.edges, r.edgeCount,
                                 &r.totalDistance, &r.totalTimeSeconds);
                
                // 信号が含まれていないか再確認
                for (int i = 0; i < r.edgeCount; i++) {
                    int edgeIdx = r.edges[i];
                    if (edgeIdx >= 0 && edgeIdx < edgeDataCount) {
                        if (edgeDataArray[edgeIdx].isSignal) {
                            r.hasSignal = 1;
                            r.signalEdgeIdx = edgeIdx;
                            break;
                        }
                    }
                }
                
                fprintf(stderr, "基準時刻1（フォールバック）: edges=%d, distance=%.2f m, time=%.2f sec (%.2f min), hasSignal=%d\n",
                        r.edgeCount, r.totalDistance, r.totalTimeSeconds, r.totalTimeSeconds / 60.0, r.hasSignal);
                
                *outRoute = r;
                return true;
            }
        }
    }
    
    fprintf(stderr, "基準時刻1: 経路が見つかりませんでした。\n");
    return false;
}

// 基準時刻2を計算する関数（信号を通る最短経路、待ち時間0、方角制約なし）
bool calculateBaseTime2(int startNode, int endNode, RouteResult *outRoute) {
    int useSignals = signalCount < 3 ? signalCount : 3;
    double minTime = INF;
    RouteResult bestRoute;
    bool found = false;
    
    // 信号を通る最短経路を探索（方角制約なし、待ち時間0）
    for (int i = 0; i < useSignals; i++) {
        int edgeIdx = signalEdges[i];
        EdgeData *sig = &edgeDataArray[edgeIdx];
        int sFrom = sig->from;
        int sTo   = sig->to;
        
        // パターンA: Start → sFrom →(信号エッジ)→ sTo → Goal
        DijkstraResult seg1 = dijkstra(startNode, sFrom);
        DijkstraResult seg2 = dijkstra(sTo, endNode);
        
        if (seg1.cost < INF && seg2.cost < INF) {
            RouteResult r;
            r.signalEdgeIdx = edgeIdx;
            r.edgeCount = 0;
            r.hasSignal = 1;
            
            if (appendSegment(&r, &seg1)) {
                if (r.edgeCount < MAX_PATH_LENGTH)
                    r.edges[r.edgeCount++] = edgeIdx;
                if (appendSegment(&r, &seg2)) {
                    // 待ち時間0で計算
                    calcRouteMetricsWithWaitTime(r.edges, r.edgeCount,
                                                 &r.totalDistance, &r.totalTimeSeconds, false);
                    if (r.totalTimeSeconds < minTime) {
                        minTime = r.totalTimeSeconds;
                        bestRoute = r;
                        found = true;
                    }
                }
            }
        }
        
        // パターンB: Start → sTo →(信号エッジ)→ sFrom → Goal
        seg1 = dijkstra(startNode, sTo);
        seg2 = dijkstra(sFrom, endNode);
        
        if (seg1.cost < INF && seg2.cost < INF) {
            RouteResult r;
            r.signalEdgeIdx = edgeIdx;
            r.edgeCount = 0;
            r.hasSignal = 1;
            
            if (appendSegment(&r, &seg1)) {
                if (r.edgeCount < MAX_PATH_LENGTH)
                    r.edges[r.edgeCount++] = edgeIdx;
                if (appendSegment(&r, &seg2)) {
                    // 待ち時間0で計算
                    calcRouteMetricsWithWaitTime(r.edges, r.edgeCount,
                                                 &r.totalDistance, &r.totalTimeSeconds, false);
                    if (r.totalTimeSeconds < minTime) {
                        minTime = r.totalTimeSeconds;
                        bestRoute = r;
                        found = true;
                    }
                }
            }
        }
    }
    
    if (found) {
        *outRoute = bestRoute;
    }
    return found;
}

// 指定された12個の信号エッジのインデックスを取得する関数
void getTargetSignalEdges(int *targetSignalIndices, int *targetCount) {
    // 指定された12個の信号経路
    int targetSignals[12][2] = {
        {66, 211}, {211, 212}, {210, 212}, {66, 210},
        {25, 196}, {196, 197}, {195, 197}, {25, 195},
        {26, 199}, {199, 200}, {26, 198}, {198, 200}
    };
    
    *targetCount = 0;
    
    for (int i = 0; i < 12; i++) {
        int from = targetSignals[i][0];
        int to = targetSignals[i][1];
        
        // 正規化
        int nf, nt;
        normalizeEdgeKey(from, to, &nf, &nt);
        
        // エッジインデックスを検索
        int edgeIdx = findEdgeIndex(nf, nt);
        if (edgeIdx >= 0 && edgeDataArray[edgeIdx].isSignal) {
            targetSignalIndices[*targetCount] = edgeIdx;
            (*targetCount)++;
            fprintf(stderr, "Target signal %d: edgeIdx=%d (%d-%d)\n", *targetCount, edgeIdx, nf, nt);
        } else {
            fprintf(stderr, "Warning: Target signal %d-%d not found or not a signal\n", nf, nt);
        }
    }
}

// 複数の信号を通る経路を探索する関数
bool findRouteThroughSignals(int startNode, int endNode, int *signalIndices, int signalCount, RouteResult *outRoute) {
    if (signalCount == 0) return false;
    
    // 1個の信号の場合
    if (signalCount == 1) {
        int edgeIdx = signalIndices[0];
        EdgeData *sig = &edgeDataArray[edgeIdx];
        int sFrom = sig->from;
        int sTo   = sig->to;
        
        // パターンA: Start → sFrom →(信号エッジ)→ sTo → Goal
        DijkstraResult seg1 = dijkstra(startNode, sFrom);
        DijkstraResult seg2 = dijkstra(sTo, endNode);
        
        if (seg1.cost < INF && seg2.cost < INF) {
            RouteResult r;
            r.signalEdgeIdx = edgeIdx;
            r.edgeCount = 0;
            r.hasSignal = 1;
            
            if (appendSegment(&r, &seg1)) {
                if (r.edgeCount < MAX_PATH_LENGTH)
                    r.edges[r.edgeCount++] = edgeIdx;
                if (appendSegment(&r, &seg2)) {
                    calcRouteMetricsWithWaitTime(r.edges, r.edgeCount,
                                                 &r.totalDistance, &r.totalTimeSeconds, true);
                    *outRoute = r;
                    return true;
                }
            }
        }
        
        // パターンB: Start → sTo →(信号エッジ)→ sFrom → Goal
        seg1 = dijkstra(startNode, sTo);
        seg2 = dijkstra(sFrom, endNode);
        
        if (seg1.cost < INF && seg2.cost < INF) {
            RouteResult r;
            r.signalEdgeIdx = edgeIdx;
            r.edgeCount = 0;
            r.hasSignal = 1;
            
            if (appendSegment(&r, &seg1)) {
                if (r.edgeCount < MAX_PATH_LENGTH)
                    r.edges[r.edgeCount++] = edgeIdx;
                if (appendSegment(&r, &seg2)) {
                    calcRouteMetricsWithWaitTime(r.edges, r.edgeCount,
                                                 &r.totalDistance, &r.totalTimeSeconds, true);
                    *outRoute = r;
                    return true;
                }
            }
        }
    } else {
        // 複数信号の場合: Start → Signal1 → Signal2 → ... → Goal
        // 簡易的な実装：最初の信号から最後の信号まで順番に通過する経路を探索
        // より高度な実装では、全ての順序を試す必要があるが、計算量が大きいため簡易版
        
        // 各信号を通る順序を試す（最初の信号と最後の信号を固定）
        for (int first = 0; first < signalCount; first++) {
            int firstEdgeIdx = signalIndices[first];
            EdgeData *firstSig = &edgeDataArray[firstEdgeIdx];
            int firstFrom = firstSig->from;
            int firstTo   = firstSig->to;
            
            // Start → firstSignal → ... → endNode
            DijkstraResult seg1 = dijkstra(startNode, firstFrom);
            if (seg1.cost >= INF) {
                seg1 = dijkstra(startNode, firstTo);
                if (seg1.cost >= INF) continue;
                firstFrom = firstTo;
                firstTo = firstSig->from;
            }
            
            // 中間の信号を通過
            int current = firstTo;
            RouteResult r;
            r.edgeCount = 0;
            r.hasSignal = 1;
            r.signalEdgeIdx = firstEdgeIdx;
            
            if (!appendSegment(&r, &seg1)) continue;
            if (r.edgeCount < MAX_PATH_LENGTH)
                r.edges[r.edgeCount++] = firstEdgeIdx;
            
            bool success = true;
            for (int i = 1; i < signalCount; i++) {
                int edgeIdx = signalIndices[i];
                EdgeData *sig = &edgeDataArray[edgeIdx];
                
                DijkstraResult mid = dijkstra(current, sig->from);
                if (mid.cost < INF && appendSegment(&r, &mid)) {
                    if (r.edgeCount < MAX_PATH_LENGTH)
                        r.edges[r.edgeCount++] = edgeIdx;
                    current = sig->to;
                } else {
                    mid = dijkstra(current, sig->to);
                    if (mid.cost < INF && appendSegment(&r, &mid)) {
                        if (r.edgeCount < MAX_PATH_LENGTH)
                            r.edges[r.edgeCount++] = edgeIdx;
                        current = sig->from;
                    } else {
                        success = false;
                        break;
                    }
                }
            }
            
            if (success) {
                DijkstraResult seg2 = dijkstra(current, endNode);
                if (seg2.cost < INF && appendSegment(&r, &seg2)) {
                    calcRouteMetricsWithWaitTime(r.edges, r.edgeCount,
                                                 &r.totalDistance, &r.totalTimeSeconds, true);
                    *outRoute = r;
                    return true;
                }
            }
        }
    }
    
    return false;
}

// 組み合わせを生成して経路を探索する再帰関数
void generateCombinations(int startNode, int endNode, int *signalIndices, int signalCount,
                          int *current, int currentSize, int maxSize, int startIdx,
                          RouteResult *outRoutes, int *outCount, int maxRoutes, int *calculatedCount) {
    if (*outCount >= maxRoutes) return;
    
    if (currentSize == maxSize) {
        RouteResult r;
        (*calculatedCount)++;  // 試行回数をカウント
        if (findRouteThroughSignals(startNode, endNode, current, currentSize, &r)) {
            r.routeType = 2;  // 赤
            outRoutes[*outCount] = r;
            // 最初の5件と最後の5件、および10件ごとにログ出力
            if (*outCount < 5 || (*outCount >= (*outCount / 10) * 10 && *outCount < (*outCount / 10) * 10 + 5) || *outCount >= maxRoutes - 5) {
                fprintf(stderr, "  経路[%d]: totalTimeSeconds=%.2f秒 (移動時間+信号待ち時間)\n", 
                        *outCount, r.totalTimeSeconds);
            }
            (*outCount)++;
        }
        return;
    }
    
    for (int i = startIdx; i < signalCount && *outCount < maxRoutes; i++) {
        current[currentSize] = signalIndices[i];
        generateCombinations(startNode, endNode, signalIndices, signalCount,
                            current, currentSize + 1, maxSize, i + 1,
                            outRoutes, outCount, maxRoutes, calculatedCount);
    }
}

// 全網羅経路を計算する関数（12個の信号から1個、2個、3個の組み合わせを全て探索）
int calculateAllEnumRoutes(int startNode, int endNode, int signalCount, RouteResult *outRoutes, int maxRoutes) {
    int count = 0;
    int calculatedCount = 0;  // 実際に計算した経路数
    
    // 指定された12個の信号エッジのインデックスを取得
    int targetSignalIndices[12];
    int targetSignalCount = 0;
    getTargetSignalEdges(targetSignalIndices, &targetSignalCount);
    
    fprintf(stderr, "指定された信号エッジ: %d個見つかりました\n", targetSignalCount);
    
    if (targetSignalCount == 0) {
        fprintf(stderr, "Warning: 指定された信号エッジが見つかりませんでした\n");
        return 0;
    }
    
    // 1個、2個、3個の組み合わせを全て生成
    int current[3];
    
    // 1個の組み合わせ
    fprintf(stderr, "1個の信号を通る経路を探索中...\n");
    for (int i = 0; i < targetSignalCount && count < maxRoutes; i++) {
        current[0] = targetSignalIndices[i];
        RouteResult r;
        calculatedCount++;  // 試行回数をカウント（経路探索を試みた回数）
        // findRouteThroughSignals内でcalcRouteMetricsWithWaitTime(..., true)が呼ばれるため、
        // 移動時間+信号待ち時間が計算される
        if (findRouteThroughSignals(startNode, endNode, current, 1, &r)) {
            r.routeType = 2;  // 赤
            outRoutes[count++] = r;
            // 各経路について、移動時間+信号待ち時間が計算されていることを確認
            fprintf(stderr, "  経路[%d]: totalTimeSeconds=%.2f秒 (移動時間+信号待ち時間が計算済み)\n", 
                    count - 1, r.totalTimeSeconds);
        }
    }
    fprintf(stderr, "1個の信号を通る経路: %d本生成 (試行: %d回、全試行で移動時間+信号待ち時間を計算)\n", count, calculatedCount);
    
    // 2個の組み合わせ
    fprintf(stderr, "2個の信号を通る経路を探索中...\n");
    int countBefore2 = count;
    int calculatedBefore2 = calculatedCount;
    generateCombinations(startNode, endNode, targetSignalIndices, targetSignalCount,
                        current, 0, 2, 0, outRoutes, &count, maxRoutes, &calculatedCount);
    fprintf(stderr, "2個の信号を通る経路: %d本生成 (試行: %d回)\n", 
            count - countBefore2, calculatedCount - calculatedBefore2);
    
    // 3個の組み合わせ
    fprintf(stderr, "3個の信号を通る経路を探索中...\n");
    int countBefore3 = count;
    int calculatedBefore3 = calculatedCount;
    generateCombinations(startNode, endNode, targetSignalIndices, targetSignalCount,
                        current, 0, 3, 0, outRoutes, &count, maxRoutes, &calculatedCount);
    fprintf(stderr, "3個の信号を通る経路: %d本生成 (試行: %d回)\n", 
            count - countBefore3, calculatedCount - calculatedBefore3);
    
    fprintf(stderr, "全網羅経路計算完了: 合計%d本生成 (総試行回数: %d回)\n", count, calculatedCount);
    fprintf(stderr, "注意: 全%d本の経路について、findRouteThroughSignals内で移動時間+信号待ち時間が計算されています\n", count);
    fprintf(stderr, "最短経路を選ぶ際、全%d本のtotalTimeSecondsを比較します\n", count);
    
    return count;
}

/* ---------- 経路の結合 ---------- */

// res に DijkstraResult の path を後ろから順に追加
bool appendSegment(RouteResult *res, const DijkstraResult *seg) {
    for (int i = 0; i < seg->pathLength; i++) {
        if (res->edgeCount >= MAX_PATH_LENGTH) return false;
        res->edges[res->edgeCount++] = seg->path[i];
    }
    return true;
}

/* ---------- JSON 出力 ---------- */

void printJSON(const RouteResult *routes, int routeCount) {
    printf("[\n");
    for (int i = 0; i < routeCount; i++) {
        const RouteResult *r = &routes[i];

        printf("  {\n");
        // userPref: edge を "from-to.geojson" の複数行で
        printf("    \"userPref\": \"");
        for (int j = 0; j < r->edgeCount; j++) {
            EdgeData *e = &edgeDataArray[r->edges[j]];
            int nf, nt;
            normalizeEdgeKey(e->from, e->to, &nf, &nt);
            printf("%d-%d.geojson", nf, nt);
            if (j < r->edgeCount - 1) printf("\\n");
        }
        printf("\",\n");

        printf("    \"signalEdgeIdx\": %d,\n", r->signalEdgeIdx);
        printf("    \"totalDistance\": %.2f,\n", r->totalDistance);
        printf("    \"totalTime\": %.2f,\n", r->totalTimeSeconds / 60.0);
        
        // 待ち時間を計算（信号エッジの期待待ち時間の合計）
        double totalWaitTime = 0.0;
        if (r->routeType == 2) {
            // 最短全網羅経路（赤）の場合、期待待ち時間を計算
            for (int j = 0; j < r->edgeCount; j++) {
                int idx = r->edges[j];
                if (idx >= 0 && idx < edgeDataCount) {
                    EdgeData *e = &edgeDataArray[idx];
                    if (e->isSignal) {
                        totalWaitTime += e->signalExpected;  // 分
                    }
                }
            }
        }
        printf("    \"totalWaitTime\": %.2f,\n", totalWaitTime);
        
        printf("    \"routeType\": %d,\n", r->routeType);
        printf("    \"hasSignal\": %d\n", r->hasSignal);
        printf("  }");
        if (i < routeCount - 1) printf(",");
        printf("\n");
    }
    printf("]\n");
}

/* ---------- メイン ---------- */

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <start_node> <end_node> <walking_speed>\n", argv[0]);
        return 1;
    }

    int    startNode = atoi(argv[1]);
    int    endNode   = atoi(argv[2]);
    double ws        = atof(argv[3]);
    if (ws > 0.0) walkingSpeed = ws;

    if (startNode < 1 || startNode >= MAX_NODES ||
        endNode   < 1 || endNode   >= MAX_NODES) {
        fprintf(stderr, "Error: invalid node number\n");
        return 1;
    }

    initGraph();
    loadGraphFromResult("result.csv");
    loadRouteData("oomiya_route_inf_4.csv");
    fprintf(stderr, "Loading signal data...\n");
    loadSignalData("signal_inf.csv");
    fprintf(stderr, "Loaded %d signals total\n", signalCount);
    fprintf(stderr, "Loading node positions...\n");
    loadNodePositions();
    fprintf(stderr, "Node positions loaded\n");

    // 経路を保存する配列
    RouteResult routes[MAX_SIGNALS * 4 + 20];  // 全網羅経路を含むため余裕を持たせる
    int routeCount = 0;
    
    RouteResult baseTime1Route;  // 基準時刻1（信号を避けた最短経路、方角±60度制約あり）
    RouteResult baseTime2Route;  // 基準時刻2（信号を通る最短経路、待ち時間0、方角制約なし）
    bool hasBaseTime1Route = false;
    bool hasBaseTime2Route = false;
    
    // スタートからゴールの方角を計算（基準時刻1用のみ）
    double targetBearing = 0.0;
    if (nodePositions[startNode].lat != 0.0 && nodePositions[endNode].lat != 0.0) {
        targetBearing = calculateBearing(
            nodePositions[startNode].lat, nodePositions[startNode].lon,
            nodePositions[endNode].lat, nodePositions[endNode].lon
        );
        fprintf(stderr, "スタート→ゴールの方角: %.2f度（基準時刻1の探索に使用）\n", targetBearing);
    } else {
        fprintf(stderr, "Warning: ノード位置情報が読み込まれていません。方角制約なしで探索します。\n");
    }
    
    // ========== 第一段階：基準時刻1（信号を避けた最短経路、方角±60度制約あり） ==========
    fprintf(stderr, "\n=== 第一段階：基準時刻1（信号を避けた最短経路、方角±60度制約あり）を計算 ===\n");
    hasBaseTime1Route = calculateBaseTime1(startNode, endNode, targetBearing, &baseTime1Route);
    if (hasBaseTime1Route) {
        fprintf(stderr,
                "基準時刻1確定: edges=%d, distance=%.2f m, time=%.2f sec (%.2f min), hasSignal=%d\n",
                baseTime1Route.edgeCount, baseTime1Route.totalDistance,
                baseTime1Route.totalTimeSeconds, baseTime1Route.totalTimeSeconds / 60.0,
                baseTime1Route.hasSignal);
    } else {
        fprintf(stderr, "警告: 基準時刻1の経路が見つかりませんでした。この場合、基準時刻2のみが表示されます。\n");
    }
    
    // ========== 第二段階：基準時刻2（信号を通る最短経路、待ち時間0、方角制約なし） ==========
    fprintf(stderr, "\n=== 第二段階：基準時刻2（信号を通る最短経路、待ち時間0、方角制約なし）を計算 ===\n");
    hasBaseTime2Route = calculateBaseTime2(startNode, endNode, &baseTime2Route);
    if (hasBaseTime2Route) {
        fprintf(stderr,
                "基準時刻2確定: edges=%d, distance=%.2f m, time=%.2f sec (%.2f min), hasSignal=%d\n",
                baseTime2Route.edgeCount, baseTime2Route.totalDistance,
                baseTime2Route.totalTimeSeconds, baseTime2Route.totalTimeSeconds / 60.0,
                baseTime2Route.hasSignal);
    } else {
        fprintf(stderr, "基準時刻2の経路が見つかりませんでした。\n");
    }
    
    // ========== 表示条件に基づいて経路を分類 ==========
    fprintf(stderr, "\n=== 基準時刻を比較して表示条件を決定 ===\n");
    
    // 基準時刻1と基準時刻2の時間を比較
    double baseTime1Seconds = hasBaseTime1Route ? baseTime1Route.totalTimeSeconds : INF;
    double baseTime2Seconds = hasBaseTime2Route ? baseTime2Route.totalTimeSeconds : INF;
    
    fprintf(stderr, "基準時刻1: %.2f sec, 基準時刻2: %.2f sec\n",
            baseTime1Seconds, baseTime2Seconds);
    
    // 基準時刻1が見つからない場合の処理
    if (!hasBaseTime1Route) {
        fprintf(stderr, "基準時刻1が見つからないため、基準時刻2を緑で表示します。\n");
        
        // ========== 第三段階：全網羅（指定信号の1,2,3個の組み合わせを通る経路、待ち時間期待値） ==========
        // 基準時刻1が見つからない場合も全網羅経路を計算
        fprintf(stderr, "\n=== 第三段階：全網羅（指定信号の1,2,3個の組み合わせを通る経路、待ち時間期待値）を探索 ===\n");
        RouteResult allEnumRoutes[MAX_SIGNALS * 4 + 20];  // 全網羅経路を一時保存
        int allEnumRouteCount = calculateAllEnumRoutes(startNode, endNode, signalCount, allEnumRoutes, MAX_SIGNALS * 4 + 20);
        fprintf(stderr, "全網羅経路: %d本生成\n", allEnumRouteCount);
        
        // 基準時刻2を緑で追加
        if (hasBaseTime2Route) {
            baseTime2Route.routeType = 1;  // 緑
            routes[routeCount++] = baseTime2Route;
        }
        
        // 全網羅経路の中で、実際の信号待ち時間を含めた総時間が最短の経路を見つける（赤色で表示）
        fprintf(stderr, "\n=== 全%d本の経路から最短経路を選出（全経路のtotalTimeSecondsを比較） ===\n", allEnumRouteCount);
        int bestEnumRouteIdx = -1;
        double bestEnumRouteTime = INF;
        int checkedCount = 0;  // 実際にチェックした経路数
        
        for (int i = 0; i < allEnumRouteCount; i++) {
            RouteResult *r = &allEnumRoutes[i];
            
            // 基準時刻2と重複していない経路のみを対象とする
            bool isBaseTime2 = false;
            
            if (hasBaseTime2Route && r->edgeCount == baseTime2Route.edgeCount) {
                bool same = true;
                for (int j = 0; j < r->edgeCount; j++) {
                    if (r->edges[j] != baseTime2Route.edges[j]) {
                        same = false;
                        break;
                    }
                }
                if (same) isBaseTime2 = true;
            }
            
            // 重複していない経路で、総時間が最短のものを探す
            // 注意: r->totalTimeSecondsは既にfindRouteThroughSignals内で
            // calcRouteMetricsWithWaitTime(..., true)により移動時間+信号待ち時間が計算されている
            if (!isBaseTime2) {
                checkedCount++;
                // 最初の5件、最後の5件、最短経路候補が更新された時のみログ出力
                if (i < 5 || i >= allEnumRouteCount - 5 || r->totalTimeSeconds < bestEnumRouteTime) {
                    // 検証のため再計算
                    double waitTimeSeconds = 0.0;
                    double moveTimeSeconds = 0.0;
                    for (int j = 0; j < r->edgeCount; j++) {
                        int idx = r->edges[j];
                        if (idx >= 0 && idx < edgeDataCount) {
                            EdgeData *e = &edgeDataArray[idx];
                            double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * e->gradient);
                            if (adjustedSpeed > 0.0) {
                                moveTimeSeconds += (e->distance / adjustedSpeed) * 60.0;  // 秒
                            }
                            if (e->isSignal) {
                                waitTimeSeconds += e->signalExpected * 60.0;  // 秒
                            }
                        }
                    }
                    if (i < 5) {
                        fprintf(stderr, "  経路[%d]: 移動時間=%.2f秒, 信号待ち時間=%.2f秒, 合計=%.2f秒 (totalTimeSeconds=%.2f秒) ✓\n",
                                i, moveTimeSeconds, waitTimeSeconds, moveTimeSeconds + waitTimeSeconds, r->totalTimeSeconds);
                    } else if (i >= allEnumRouteCount - 5) {
                        fprintf(stderr, "  経路[%d]: 移動時間=%.2f秒, 信号待ち時間=%.2f秒, 合計=%.2f秒 (totalTimeSeconds=%.2f秒) ✓\n",
                                i, moveTimeSeconds, waitTimeSeconds, moveTimeSeconds + waitTimeSeconds, r->totalTimeSeconds);
                    } else if (r->totalTimeSeconds < bestEnumRouteTime) {
                        fprintf(stderr, "  経路[%d]: 新たな最短候補! totalTimeSeconds=%.2f秒 (移動時間+信号待ち時間) ✓\n",
                                i, r->totalTimeSeconds);
                    }
                }
                
                if (r->totalTimeSeconds < bestEnumRouteTime) {
                    bestEnumRouteTime = r->totalTimeSeconds;
                    bestEnumRouteIdx = i;
                }
            }
        }
        fprintf(stderr, "全%d本の経路をチェックしました（重複除外後: %d本）\n", allEnumRouteCount, checkedCount);
        
        // 最短の全網羅経路のみを赤色で追加（1本のみ）
        if (bestEnumRouteIdx >= 0) {
            allEnumRoutes[bestEnumRouteIdx].routeType = 2;  // 赤
            routes[routeCount++] = allEnumRoutes[bestEnumRouteIdx];
            fprintf(stderr, "最短全網羅経路（赤）: edges=%d, distance=%.2f m, time=%.2f sec (%.2f min)\n",
                    allEnumRoutes[bestEnumRouteIdx].edgeCount,
                    allEnumRoutes[bestEnumRouteIdx].totalDistance,
                    allEnumRoutes[bestEnumRouteIdx].totalTimeSeconds,
                    allEnumRoutes[bestEnumRouteIdx].totalTimeSeconds / 60.0);
        }
    }
    // 基準時刻1 < 基準時刻2 の場合
    else if (baseTime1Seconds < baseTime2Seconds) {
        fprintf(stderr, "基準時刻1 < 基準時刻2: 基準時刻1（緑）のみを表示（全網羅経路は計算しない）\n");
        
        // 基準時刻1を緑で追加（全網羅経路は計算・表示しない）
        baseTime1Route.routeType = 1;  // 緑
        routes[routeCount++] = baseTime1Route;
    }
    // 基準時刻1 >= 基準時刻2 の場合
    else {
        // ========== 第三段階：全網羅（指定信号の1,2,3個の組み合わせを通る経路、待ち時間期待値） ==========
        // 基準時刻1 >= 基準時刻2 の場合のみ全網羅経路を計算
        fprintf(stderr, "\n=== 第三段階：全網羅（指定信号の1,2,3個の組み合わせを通る経路、待ち時間期待値）を探索 ===\n");
        RouteResult allEnumRoutes[MAX_SIGNALS * 4 + 20];  // 全網羅経路を一時保存
        int allEnumRouteCount = calculateAllEnumRoutes(startNode, endNode, signalCount, allEnumRoutes, MAX_SIGNALS * 4 + 20);
        fprintf(stderr, "全網羅経路: %d本生成\n", allEnumRouteCount);
        
        fprintf(stderr, "\n=== 表示条件に基づいて経路を分類 ===\n");
        fprintf(stderr, "基準時刻1 >= 基準時刻2: 基準時刻1を緑、基準時刻2を青、最短全網羅を赤で表示\n");
        
        // 基準時刻1を緑で追加
        baseTime1Route.routeType = 1;  // 緑
        routes[routeCount++] = baseTime1Route;
        
        // 基準時刻2を青で追加
        if (hasBaseTime2Route) {
            baseTime2Route.routeType = 0;  // 青
            routes[routeCount++] = baseTime2Route;
        }
        
        // 全網羅経路の中で、実際の信号待ち時間を含めた総時間が最短の経路を見つける（赤色で表示）
        fprintf(stderr, "\n=== 全%d本の経路から最短経路を選出（全経路のtotalTimeSecondsを比較） ===\n", allEnumRouteCount);
        int bestEnumRouteIdx = -1;
        double bestEnumRouteTime = INF;
        int checkedCount = 0;  // 実際にチェックした経路数
        
        for (int i = 0; i < allEnumRouteCount; i++) {
            RouteResult *r = &allEnumRoutes[i];
            
            // 基準時刻1/2と重複していない経路のみを対象とする
            bool isBaseTime1 = false;
            bool isBaseTime2 = false;
            
            if (r->edgeCount == baseTime1Route.edgeCount) {
                bool same = true;
                for (int j = 0; j < r->edgeCount; j++) {
                    if (r->edges[j] != baseTime1Route.edges[j]) {
                        same = false;
                        break;
                    }
                }
                if (same) isBaseTime1 = true;
            }
            
            if (hasBaseTime2Route && r->edgeCount == baseTime2Route.edgeCount) {
                bool same = true;
                for (int j = 0; j < r->edgeCount; j++) {
                    if (r->edges[j] != baseTime2Route.edges[j]) {
                        same = false;
                        break;
                    }
                }
                if (same) isBaseTime2 = true;
            }
            
            // 重複していない経路で、総時間が最短のものを探す
            // 注意: r->totalTimeSecondsは既にfindRouteThroughSignals内で
            // calcRouteMetricsWithWaitTime(..., true)により移動時間+信号待ち時間が計算されている
            if (!isBaseTime1 && !isBaseTime2) {
                checkedCount++;
                // 最初の5件、最後の5件、最短経路候補が更新された時のみログ出力
                if (i < 5 || i >= allEnumRouteCount - 5 || r->totalTimeSeconds < bestEnumRouteTime) {
                    // 検証のため再計算
                    double waitTimeSeconds = 0.0;
                    double moveTimeSeconds = 0.0;
                    for (int j = 0; j < r->edgeCount; j++) {
                        int idx = r->edges[j];
                        if (idx >= 0 && idx < edgeDataCount) {
                            EdgeData *e = &edgeDataArray[idx];
                            double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * e->gradient);
                            if (adjustedSpeed > 0.0) {
                                moveTimeSeconds += (e->distance / adjustedSpeed) * 60.0;  // 秒
                            }
                            if (e->isSignal) {
                                waitTimeSeconds += e->signalExpected * 60.0;  // 秒
                            }
                        }
                    }
                    if (i < 5) {
                        fprintf(stderr, "  経路[%d]: 移動時間=%.2f秒, 信号待ち時間=%.2f秒, 合計=%.2f秒 (totalTimeSeconds=%.2f秒) ✓\n",
                                i, moveTimeSeconds, waitTimeSeconds, moveTimeSeconds + waitTimeSeconds, r->totalTimeSeconds);
                    } else if (i >= allEnumRouteCount - 5) {
                        fprintf(stderr, "  経路[%d]: 移動時間=%.2f秒, 信号待ち時間=%.2f秒, 合計=%.2f秒 (totalTimeSeconds=%.2f秒) ✓\n",
                                i, moveTimeSeconds, waitTimeSeconds, moveTimeSeconds + waitTimeSeconds, r->totalTimeSeconds);
                    } else if (r->totalTimeSeconds < bestEnumRouteTime) {
                        fprintf(stderr, "  経路[%d]: 新たな最短候補! totalTimeSeconds=%.2f秒 (移動時間+信号待ち時間) ✓\n",
                                i, r->totalTimeSeconds);
                    }
                }
                
                if (r->totalTimeSeconds < bestEnumRouteTime) {
                    bestEnumRouteTime = r->totalTimeSeconds;
                    bestEnumRouteIdx = i;
                }
            }
        }
        fprintf(stderr, "全%d本の経路をチェックしました（重複除外後: %d本）\n", allEnumRouteCount, checkedCount);
        
        // 最短の全網羅経路のみを赤色で追加（1本のみ）
        if (bestEnumRouteIdx >= 0) {
            allEnumRoutes[bestEnumRouteIdx].routeType = 2;  // 赤
            routes[routeCount++] = allEnumRoutes[bestEnumRouteIdx];
            fprintf(stderr, "最短全網羅経路（赤）: edges=%d, distance=%.2f m, time=%.2f sec (%.2f min)\n",
                    allEnumRoutes[bestEnumRouteIdx].edgeCount,
                    allEnumRoutes[bestEnumRouteIdx].totalDistance,
                    allEnumRoutes[bestEnumRouteIdx].totalTimeSeconds,
                    allEnumRoutes[bestEnumRouteIdx].totalTimeSeconds / 60.0);
        }
    }
    
    fprintf(stderr, "\n最終出力: %d本の経路\n", routeCount);
    int greenCount = 0, blueCount = 0, redCount = 0;
    for (int i = 0; i < routeCount; i++) {
        if (routes[i].routeType == 0) blueCount++;
        else if (routes[i].routeType == 1) greenCount++;
        else if (routes[i].routeType == 2) redCount++;
    }
    fprintf(stderr, "- 青（基準時刻2）: %d本\n", blueCount);
    fprintf(stderr, "- 緑（基準時刻1）: %d本\n", greenCount);
    fprintf(stderr, "- 赤（最短全網羅）: %d本\n", redCount);
    
    printJSON(routes, routeCount);

    return 0;
}
