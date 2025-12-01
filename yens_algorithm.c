/* シンプル版: signal_inf.csv の信号を使って
 * スタート→信号→ゴールの経路を網羅的に列挙する
 * - イェンのアルゴリズム・基準1/2 等はすべて削除
 * - 信号待ち時間も考慮せず、「距離＋勾配による歩行時間」のみを使う
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>

#define MAX_NODES       300
#define MAX_EDGES       1000
#define MAX_PATH_LENGTH 200
#define MAX_SIGNALS     50   // signal_inf.csv 側の最大信号数を想定
#define MAX_SIGNAL_GROUPS 20  // 交差点グループの最大数
#define MAX_EDGES_PER_GROUP 4  // 1つの交差点あたりの最大信号エッジ数

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
} EdgeData;

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
    int signalEdgeIdx;           // どの信号か
    int edges[MAX_PATH_LENGTH];  // 経路のエッジ列
    int edgeCount;
    double totalDistance;        // m
    double totalTimeSeconds;     // 秒
} RouteResult;

// 信号グループ（交差点ごと）
typedef struct {
    int edgeIndices[MAX_EDGES_PER_GROUP];  // この交差点に属する信号エッジのインデックス
    int edgeCount;                          // エッジ数（通常4本）
    int nodes[MAX_EDGES_PER_GROUP];        // この交差点に含まれるノード集合
    int nodeCount;                          // ノード数
} SignalGroup;

/* ---------- グローバル ---------- */

GraphNode graph[MAX_NODES];
EdgeData  edgeDataArray[MAX_EDGES];
int       edgeDataCount = 0;

int   signalEdges[MAX_SIGNALS];
int   signalCount = 0;

SignalGroup signalGroups[MAX_SIGNAL_GROUPS];
int         signalGroupCount = 0;

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

/* ---------- ダイクストラ（信号制限なし・待ち時間なし） ---------- */

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
            int v = graph[u].edges[i].node;
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

void calcRouteMetrics(const int *edgeIdxs, int edgeCount,
                      double *outDist, double *outTimeSec) {
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
    }

    *outDist    = totalDist;
    *outTimeSec = totalTime;
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

        // 信号フラグ
        edgeDataArray[edgeIdx].isSignal = 1;

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

/* ---------- 信号グループ化 ---------- */

// 2つのノード集合が同じ交差点に属するかチェック
bool shareCommonNodes(int *nodes1, int count1, int *nodes2, int count2) {
    for (int i = 0; i < count1; i++) {
        for (int j = 0; j < count2; j++) {
            if (nodes1[i] == nodes2[j]) return true;
        }
    }
    return false;
}

// 信号エッジを交差点ごとにグループ化
void groupSignalsByIntersection(void) {
    signalGroupCount = 0;
    bool used[MAX_SIGNALS] = {false};

    for (int i = 0; i < signalCount; i++) {
        if (used[i]) continue;

        // 新しいグループを作成
        SignalGroup *group = &signalGroups[signalGroupCount];
        group->edgeCount = 0;
        group->nodeCount = 0;

        EdgeData *e1 = &edgeDataArray[signalEdges[i]];
        int nodes1[2] = {e1->from, e1->to};

        // このエッジをグループに追加
        group->edgeIndices[group->edgeCount++] = signalEdges[i];
        used[i] = true;

        // このエッジのノードをグループのノード集合に追加
        for (int k = 0; k < 2; k++) {
            bool found = false;
            for (int m = 0; m < group->nodeCount; m++) {
                if (group->nodes[m] == nodes1[k]) {
                    found = true;
                    break;
                }
            }
            if (!found && group->nodeCount < MAX_EDGES_PER_GROUP) {
                group->nodes[group->nodeCount++] = nodes1[k];
            }
        }

        // 同じ交差点に属する他の信号エッジを探す
        for (int j = i + 1; j < signalCount; j++) {
            if (used[j]) continue;

            EdgeData *e2 = &edgeDataArray[signalEdges[j]];
            int nodes2[2] = {e2->from, e2->to};

            // 共通ノードがあるかチェック
            if (shareCommonNodes(nodes1, 2, nodes2, 2) ||
                shareCommonNodes(group->nodes, group->nodeCount, nodes2, 2)) {
                // 同じグループに追加
                if (group->edgeCount < MAX_EDGES_PER_GROUP) {
                    group->edgeIndices[group->edgeCount++] = signalEdges[j];
                    used[j] = true;

                    // ノードを追加
                    for (int k = 0; k < 2; k++) {
                        bool found = false;
                        for (int m = 0; m < group->nodeCount; m++) {
                            if (group->nodes[m] == nodes2[k]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found && group->nodeCount < MAX_EDGES_PER_GROUP) {
                            group->nodes[group->nodeCount++] = nodes2[k];
                        }
                    }
                }
            }
        }

        signalGroupCount++;
        if (signalGroupCount >= MAX_SIGNAL_GROUPS) break;
    }

    fprintf(stderr, "Grouped signals into %d intersection groups\n", signalGroupCount);
    for (int i = 0; i < signalGroupCount; i++) {
        fprintf(stderr, "  Group %d: %d signals (nodes: ", i + 1, signalGroups[i].edgeCount);
        for (int j = 0; j < signalGroups[i].nodeCount; j++) {
            fprintf(stderr, "%d ", signalGroups[i].nodes[j]);
        }
        fprintf(stderr, ")\n");
    }
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

// 許可された信号エッジかチェック
bool isAllowedSignal(int edgeIdx, int *allowedEdges, int allowedCount) {
    for (int i = 0; i < allowedCount; i++) {
        if (allowedEdges[i] == edgeIdx) return true;
    }
    return false;
}

// 複数の信号エッジを順に通る経路を生成（許可された信号のみ）
bool buildRouteThroughSignals(RouteResult *r, int startNode, int endNode,
                               int *signalEdgeIndices, int signalCount,
                               int *allowedEdges, int allowedCount) {
    r->edgeCount = 0;
    int currentNode = startNode;

    for (int i = 0; i < signalCount; i++) {
        int edgeIdx = signalEdgeIndices[i];
        
        // 許可された信号かチェック
        if (!isAllowedSignal(edgeIdx, allowedEdges, allowedCount)) {
            return false;  // 許可されていない信号を通る経路は除外
        }
        
        EdgeData *sig = &edgeDataArray[edgeIdx];
        int sFrom = sig->from;
        int sTo   = sig->to;

        // 現在のノードから信号エッジの一端へ
        DijkstraResult seg = dijkstra(currentNode, sFrom);
        if (seg.cost >= INF) {
            // もう一方の端を試す
            seg = dijkstra(currentNode, sTo);
            if (seg.cost >= INF) return false;
            if (!appendSegment(r, &seg)) return false;
            // 信号エッジを追加
            if (r->edgeCount >= MAX_PATH_LENGTH) return false;
            r->edges[r->edgeCount++] = edgeIdx;
            currentNode = sFrom;  // 反対側の端へ
        } else {
            if (!appendSegment(r, &seg)) return false;
            // 信号エッジを追加
            if (r->edgeCount >= MAX_PATH_LENGTH) return false;
            r->edges[r->edgeCount++] = edgeIdx;
            currentNode = sTo;
        }
    }

    // 最後の信号からゴールへ
    DijkstraResult finalSeg = dijkstra(currentNode, endNode);
    if (finalSeg.cost >= INF) return false;
    if (!appendSegment(r, &finalSeg)) return false;

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
        printf("    \"totalTime\": %.2f\n", r->totalTimeSeconds);
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

    // 指定された信号エッジのみを使用
    int allowedSignalEdges[MAX_SIGNALS];
    int allowedSignalCount = 0;
    
    // 許可された信号エッジのリスト（from-to形式）
    struct {
        int from;
        int to;
    } allowedSignals[] = {
        {66, 211}, {210, 212}, {211, 212}, {66, 210},
        {25, 196}, {196, 197}, {195, 197}, {25, 195},
        {199, 200}, {198, 200}, {26, 198}, {26, 199}
    };
    int numAllowedSignals = sizeof(allowedSignals) / sizeof(allowedSignals[0]);
    
    fprintf(stderr, "\n=== Filtering allowed signals ===\n");
    for (int i = 0; i < numAllowedSignals; i++) {
        int edgeIdx = findEdgeIndex(allowedSignals[i].from, allowedSignals[i].to);
        if (edgeIdx >= 0) {
            allowedSignalEdges[allowedSignalCount++] = edgeIdx;
            fprintf(stderr, "Allowed signal: edge %d (%d-%d)\n", 
                    edgeIdx, allowedSignals[i].from, allowedSignals[i].to);
        } else {
            fprintf(stderr, "Warning: signal edge %d-%d not found in graph\n",
                    allowedSignals[i].from, allowedSignals[i].to);
        }
    }
    fprintf(stderr, "Total allowed signals: %d\n", allowedSignalCount);
    
    if (allowedSignalCount == 0) {
        fprintf(stderr, "Error: No allowed signals found!\n");
        return 1;
    }
    
    // 信号を交差点ごとにグループ化（許可された信号のみ）
    // まず、許可された信号のみをsignalEdgesに設定
    signalCount = allowedSignalCount;
    for (int i = 0; i < allowedSignalCount; i++) {
        signalEdges[i] = allowedSignalEdges[i];
    }
    groupSignalsByIntersection();

    // 基準時間1: 信号を通らない最短経路
    fprintf(stderr, "\n=== Calculating reference times ===\n");
    DijkstraResult shortestPath = dijkstra(startNode, endNode);
    double referenceTime1 = shortestPath.cost;  // 最短経路の時間（秒）
    fprintf(stderr, "Reference Time 1 (shortest path, no signals): %.2f sec\n", referenceTime1);

    // 基準時間2: 1つの信号を通る最短経路の時間（許可された信号のみ）
    double referenceTime2 = INF;
    for (int i = 0; i < allowedSignalCount; i++) {
        int edgeIdx = allowedSignalEdges[i];
        EdgeData *sig = &edgeDataArray[edgeIdx];
        int sFrom = sig->from;
        int sTo   = sig->to;

        // パターンA: Start → sFrom →(信号)→ sTo → Goal
        DijkstraResult seg1 = dijkstra(startNode, sFrom);
        DijkstraResult seg2 = dijkstra(sTo, endNode);
        if (seg1.cost < INF && seg2.cost < INF) {
            double totalTime = seg1.cost + getEdgeTimeSeconds(sFrom, sTo) + seg2.cost;
            if (totalTime < referenceTime2) referenceTime2 = totalTime;
        }

        // パターンB: Start → sTo →(信号)→ sFrom → Goal
        seg1 = dijkstra(startNode, sTo);
        seg2 = dijkstra(sFrom, endNode);
        if (seg1.cost < INF && seg2.cost < INF) {
            double totalTime = seg1.cost + getEdgeTimeSeconds(sTo, sFrom) + seg2.cost;
            if (totalTime < referenceTime2) referenceTime2 = totalTime;
        }
    }
    if (referenceTime2 >= INF) referenceTime2 = referenceTime1 * 1.5;  // フォールバック
    fprintf(stderr, "Reference Time 2 (shortest path with 1 signal): %.2f sec\n", referenceTime2);

    // フィルタリングの閾値: 基準時間2の1.5倍を超える経路は除外
    double timeThreshold = referenceTime2 * 1.5;
    fprintf(stderr, "Time threshold for filtering: %.2f sec (%.1fx of reference time 2)\n", 
            timeThreshold, 1.5);

    // 経路を生成（最大数を増やす）
    RouteResult routes[MAX_SIGNALS * 20];  // 十分なサイズを確保
    int routeCount = 0;

    fprintf(stderr, "\n=== Generating routes ===\n");

    // パターン1: 1つの信号を通る経路（許可された信号のみ）
    fprintf(stderr, "\n[Pattern 1] Routes through 1 signal:\n");
    for (int i = 0; i < allowedSignalCount; i++) {
        int edgeIdx = allowedSignalEdges[i];
        EdgeData *sig = &edgeDataArray[edgeIdx];
        int sFrom = sig->from;
        int sTo   = sig->to;

        // パターンA: Start → sFrom →(信号)→ sTo → Goal
        DijkstraResult seg1 = dijkstra(startNode, sFrom);
        DijkstraResult seg2 = dijkstra(sTo, endNode);
        if (seg1.cost < INF && seg2.cost < INF) {
            RouteResult r;
            r.signalEdgeIdx = edgeIdx;
            r.edgeCount = 0;
            if (appendSegment(&r, &seg1)) {
                if (r.edgeCount < MAX_PATH_LENGTH)
                    r.edges[r.edgeCount++] = edgeIdx;
                if (appendSegment(&r, &seg2)) {
                    calcRouteMetrics(r.edges, r.edgeCount,
                                     &r.totalDistance, &r.totalTimeSeconds);
                    // 時間閾値でフィルタリング
                    if (r.totalTimeSeconds <= timeThreshold) {
                        routes[routeCount++] = r;
                    }
                }
            }
        }

        // パターンB: Start → sTo →(信号)→ sFrom → Goal
        seg1 = dijkstra(startNode, sTo);
        seg2 = dijkstra(sFrom, endNode);
        if (seg1.cost < INF && seg2.cost < INF) {
            RouteResult r;
            r.signalEdgeIdx = edgeIdx;
            r.edgeCount = 0;
            if (appendSegment(&r, &seg1)) {
                if (r.edgeCount < MAX_PATH_LENGTH)
                    r.edges[r.edgeCount++] = edgeIdx;
                if (appendSegment(&r, &seg2)) {
                    calcRouteMetrics(r.edges, r.edgeCount,
                                     &r.totalDistance, &r.totalTimeSeconds);
                    // 時間閾値でフィルタリング
                    if (r.totalTimeSeconds <= timeThreshold) {
                        routes[routeCount++] = r;
                    }
                }
            }
        }
    }
    fprintf(stderr, "  Generated %d routes through 1 signal\n", routeCount);

    // パターン2: 2つの信号を通る経路（許可された信号のみ、異なる2つ）
    fprintf(stderr, "\n[Pattern 2] Routes through 2 signals:\n");
    int pattern2Start = routeCount;
    for (int i1 = 0; i1 < allowedSignalCount; i1++) {
        for (int i2 = i1 + 1; i2 < allowedSignalCount; i2++) {
            int edgeIdx1 = allowedSignalEdges[i1];
            int edgeIdx2 = allowedSignalEdges[i2];
            int signals[2] = {edgeIdx1, edgeIdx2};

            RouteResult r;
            r.signalEdgeIdx = edgeIdx1;  // 最初の信号を記録
            if (buildRouteThroughSignals(&r, startNode, endNode, signals, 2,
                                         allowedSignalEdges, allowedSignalCount)) {
                calcRouteMetrics(r.edges, r.edgeCount,
                                 &r.totalDistance, &r.totalTimeSeconds);
                // 時間閾値でフィルタリング
                if (r.totalTimeSeconds <= timeThreshold) {
                    routes[routeCount++] = r;
                    if (routeCount >= MAX_SIGNALS * 20) goto route_limit;
                }
            }
        }
    }
    fprintf(stderr, "  Generated %d routes through 2 signals\n", routeCount - pattern2Start);

    // パターン3: 全ての許可された信号を通る経路（許可された信号のみ）
    fprintf(stderr, "\n[Pattern 3] Routes through all allowed signals:\n");
    int pattern3Start = routeCount;
    if (allowedSignalCount > 0 && allowedSignalCount <= 12) {  // 許可された信号数が12個以下なら全組み合わせを試す
        int selectedEdges[12];  // 最大12個
        
        // 許可された信号の全組み合わせを生成（再帰的ではなく、ネストしたループで）
        // 3個以上の信号を通る経路を生成
        if (allowedSignalCount >= 3) {
            for (int i1 = 0; i1 < allowedSignalCount && routeCount < MAX_SIGNALS * 20; i1++) {
                for (int i2 = i1 + 1; i2 < allowedSignalCount && routeCount < MAX_SIGNALS * 20; i2++) {
                    for (int i3 = i2 + 1; i3 < allowedSignalCount && routeCount < MAX_SIGNALS * 20; i3++) {
                        selectedEdges[0] = allowedSignalEdges[i1];
                        selectedEdges[1] = allowedSignalEdges[i2];
                        selectedEdges[2] = allowedSignalEdges[i3];
                        RouteResult r;
                        r.signalEdgeIdx = selectedEdges[0];
                        if (buildRouteThroughSignals(&r, startNode, endNode, selectedEdges, 3,
                                                      allowedSignalEdges, allowedSignalCount)) {
                            calcRouteMetrics(r.edges, r.edgeCount, &r.totalDistance, &r.totalTimeSeconds);
                            if (r.totalTimeSeconds <= timeThreshold) {
                                routes[routeCount++] = r;
                            }
                        }
                    }
                }
            }
        }
    } else if (allowedSignalCount > 12) {
        fprintf(stderr, "  Skipped: too many allowed signals (%d) for exhaustive search\n", allowedSignalCount);
    }
    fprintf(stderr, "  Generated %d routes through all signals\n", routeCount - pattern3Start);

route_limit:
    fprintf(stderr, "\nGenerated %d routes in total.\n", routeCount);
    printJSON(routes, routeCount);

    return 0;
}