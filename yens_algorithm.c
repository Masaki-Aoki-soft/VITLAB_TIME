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
        printf("    \"totalWaitTime\": 0.0,\n");
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

    // 経路を保存する配列（最短経路 + 信号経路 + 信号回避経路）
    RouteResult routes[MAX_SIGNALS * 2 + 10];  // 余裕を持たせる
    RouteResult allRoutes[MAX_SIGNALS * 2 + 10];  // 全経路を一時保存
    int routeCount = 0;
    int allRouteCount = 0;

    // 1. 最短経路（信号を考慮しない）を生成
    fprintf(stderr, "\n=== 最短経路を計算 ===\n");
    DijkstraResult shortestPath = dijkstra(startNode, endNode);
    RouteResult baseTime1Route;
    bool hasBaseTime1Route = false;
    
    if (shortestPath.cost < INF) {
        RouteResult r;
        r.signalEdgeIdx = -1;
        r.edgeCount = 0;
        r.routeType = 2;  // まずはその他として分類（後で基準時刻1に変更）
        
        // 経路に信号が含まれるかチェック
        r.hasSignal = 0;
        for (int i = 0; i < shortestPath.pathLength; i++) {
            int edgeIdx = shortestPath.path[i];
            if (edgeIdx >= 0 && edgeIdx < edgeDataCount) {
                if (edgeDataArray[edgeIdx].isSignal) {
                    r.hasSignal = 1;
                    r.signalEdgeIdx = edgeIdx;
                    break;
                }
            }
        }
        
        if (appendSegment(&r, &shortestPath)) {
            calcRouteMetrics(r.edges, r.edgeCount,
                             &r.totalDistance, &r.totalTimeSeconds);
            baseTime1Route = r;  // 基準時刻1の経路として保存
            hasBaseTime1Route = true;
            allRoutes[allRouteCount++] = r;
            fprintf(stderr,
                    "最短経路: edges=%d, time=%.2f sec, hasSignal=%d\n",
                    r.edgeCount, r.totalTimeSeconds, r.hasSignal);
        }
    }

    // 2. 信号を含む経路を生成（全てその他として分類）
    int useSignals = signalCount;
    if (useSignals > 12) useSignals = 12;  // 「12個分」だけを見る場合

    for (int i = 0; i < useSignals; i++) {
        int edgeIdx = signalEdges[i];
        EdgeData *sig = &edgeDataArray[edgeIdx];
        int sFrom = sig->from;
        int sTo   = sig->to;

        fprintf(stderr,
                "\n=== Signal %d (edgeIdx=%d, %d-%d) ===\n",
                i + 1, edgeIdx, sFrom, sTo);

        /* パターンA: Start → sFrom →(信号エッジ)→ sTo → Goal */
        {
            DijkstraResult seg1 = dijkstra(startNode, sFrom);
            DijkstraResult seg2 = dijkstra(sTo, endNode);

            if (seg1.cost < INF && seg2.cost < INF) {
                RouteResult r;
                r.signalEdgeIdx = edgeIdx;
                r.edgeCount      = 0;
                r.routeType = 2;  // その他（赤）
                r.hasSignal = 1;  // 信号を含む

                if (appendSegment(&r, &seg1)) {
                    // 信号エッジ自身を1本挿入
                    if (r.edgeCount < MAX_PATH_LENGTH)
                        r.edges[r.edgeCount++] = edgeIdx;

                    if (appendSegment(&r, &seg2)) {
                        calcRouteMetrics(r.edges, r.edgeCount,
                                         &r.totalDistance, &r.totalTimeSeconds);
                        allRoutes[allRouteCount++] = r;
                        fprintf(stderr,
                                "Pattern A: path found (edges=%d, time=%.2f sec)\n",
                                r.edgeCount, r.totalTimeSeconds);
                    }
                }
            } else {
                fprintf(stderr,
                        "Pattern A: no path (Start->%d or %d->Goal unreachable)\n",
                        sFrom, sTo);
            }
        }

        /* パターンB: Start → sTo →(信号エッジ)→ sFrom → Goal */
        {
            DijkstraResult seg1 = dijkstra(startNode, sTo);
            DijkstraResult seg2 = dijkstra(sFrom, endNode);

            if (seg1.cost < INF && seg2.cost < INF) {
                RouteResult r;
                r.signalEdgeIdx = edgeIdx;
                r.edgeCount      = 0;
                r.routeType = 2;  // その他（赤）
                r.hasSignal = 1;  // 信号を含む

                if (appendSegment(&r, &seg1)) {
                    if (r.edgeCount < MAX_PATH_LENGTH)
                        r.edges[r.edgeCount++] = edgeIdx;

                    if (appendSegment(&r, &seg2)) {
                        calcRouteMetrics(r.edges, r.edgeCount,
                                         &r.totalDistance, &r.totalTimeSeconds);
                        allRoutes[allRouteCount++] = r;
                        fprintf(stderr,
                                "Pattern B: path found (edges=%d, time=%.2f sec)\n",
                                r.edgeCount, r.totalTimeSeconds);
                    }
                }
            } else {
                fprintf(stderr,
                        "Pattern B: no path (Start->%d or %d->Goal unreachable)\n",
                        sTo, sFrom);
            }

        }
    }

    // 3. 信号を避けた経路を探索（基準時刻2用 - 最短経路のみ）
    fprintf(stderr, "\n=== 信号を避けた経路を計算（基準時刻2） ===\n");
    RouteResult baseTime2Route;
    bool hasBaseTime2Route = false;
    
    // 最短経路に信号が含まれている場合、その信号を避けた最短経路を探索
    if (hasBaseTime1Route && baseTime1Route.hasSignal == 1) {
        // 最短経路に信号がある場合：基準時刻1（青）= 最短経路、基準時刻2（緑）= 信号回避経路
        int firstRouteSignalIdx = baseTime1Route.signalEdgeIdx;
        if (firstRouteSignalIdx >= 0) {
            // 最短経路に含まれる信号を避けた最短経路を探索
            DijkstraResult avoidPath = dijkstraAvoidSignal(startNode, endNode, firstRouteSignalIdx);
            if (avoidPath.cost < INF) {
                RouteResult r;
                r.signalEdgeIdx = -1;
                r.edgeCount = 0;
                r.routeType = 1;  // 基準時刻2（緑）
                r.hasSignal = 0;  // 信号を含まない
                
                if (appendSegment(&r, &avoidPath)) {
                    calcRouteMetrics(r.edges, r.edgeCount,
                                     &r.totalDistance, &r.totalTimeSeconds);
                    baseTime2Route = r;
                    hasBaseTime2Route = true;
                    fprintf(stderr,
                            "基準時刻2（信号回避最短経路）: edges=%d, time=%.2f sec\n",
                            r.edgeCount, r.totalTimeSeconds);
                }
            }
        }
    } else if (hasBaseTime1Route && baseTime1Route.hasSignal == 0) {
        // 最短経路に信号がない場合：基準時刻1（青）は表示せず、基準時刻2（緑）= 最短経路
        baseTime2Route = baseTime1Route;
        baseTime2Route.routeType = 1;  // 基準時刻2（緑）
        hasBaseTime2Route = true;
        hasBaseTime1Route = false;  // 基準時刻1（青）は表示しない
        fprintf(stderr,
                "最短経路に信号なし: 基準時刻2（緑）として最短経路を使用\n");
    }
    
    // 4. 経路を分類して最終出力用に整理
    fprintf(stderr, "\n=== 経路を分類 ===\n");
    
    // 基準時刻1の経路を追加（青）- 最短経路に信号がある場合のみ
    if (hasBaseTime1Route) {
        baseTime1Route.routeType = 0;  // 基準時刻1（青）
        routes[routeCount++] = baseTime1Route;
        fprintf(stderr, "基準時刻1（青）: 1本追加\n");
    }
    
    // 基準時刻2の経路を追加（緑）
    if (hasBaseTime2Route) {
        routes[routeCount++] = baseTime2Route;
        fprintf(stderr, "基準時刻2（緑）: 1本追加\n");
    }
    
    // その他の経路を追加（赤）
    for (int i = 0; i < allRouteCount; i++) {
        // 基準時刻1と基準時刻2の経路は除外
        bool isBaseTime1 = false;
        bool isBaseTime2 = false;
        
        // 基準時刻1の経路との比較（hasBaseTime1Routeがfalseでも比較は行う）
        if (allRoutes[i].edgeCount == baseTime1Route.edgeCount) {
            bool same = true;
            for (int j = 0; j < allRoutes[i].edgeCount; j++) {
                if (allRoutes[i].edges[j] != baseTime1Route.edges[j]) {
                    same = false;
                    break;
                }
            }
            if (same) isBaseTime1 = true;
        }
        
        // 基準時刻2の経路との比較（hasBaseTime2Routeがfalseでも比較は行う）
        if (hasBaseTime2Route && allRoutes[i].edgeCount == baseTime2Route.edgeCount) {
            bool same = true;
            for (int j = 0; j < allRoutes[i].edgeCount; j++) {
                if (allRoutes[i].edges[j] != baseTime2Route.edges[j]) {
                    same = false;
                    break;
                }
            }
            if (same) isBaseTime2 = true;
        }
        
        // 基準時刻1または基準時刻2に該当する経路は除外
        // 最短経路に信号がない場合、baseTime1RouteとbaseTime2Routeは同じ経路なので、どちらかのチェックで除外される
        if (!isBaseTime1 && !isBaseTime2) {
            allRoutes[i].routeType = 2;  // その他（赤）
            routes[routeCount++] = allRoutes[i];
        } else {
            // 除外された経路をログ出力
            fprintf(stderr, "経路%dを除外（基準時刻1/2と重複）\n", i);
        }
    }
    
    fprintf(stderr, "\nGenerated %d routes in total.\n", routeCount);
    fprintf(stderr, "- 基準時刻1（青）: %d本\n", hasBaseTime1Route ? 1 : 0);
    fprintf(stderr, "- 基準時刻2（緑）: %d本\n", hasBaseTime2Route ? 1 : 0);
    fprintf(stderr, "- その他（赤）: %d本\n", routeCount - (hasBaseTime1Route ? 1 : 0) - (hasBaseTime2Route ? 1 : 0));
    
    printJSON(routes, routeCount);

    return 0;
}
