/* 信号待ち時間計算（基準信号を指定した待ち時間計算） */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define MAX_EDGES 1000
#define MAX_PATH_LENGTH 200
#define MAX_LINE_LENGTH 1024
#define K_GRADIENT 0.5

// エッジ情報
typedef struct {
    int from;
    int to;
    double distance;
    double gradient;
    int isSignal;
    int signalCycle;
    int signalGreen;
    double signalPhase;
} EdgeData;

// グローバル変数
EdgeData edgeDataArray[MAX_EDGES];
int edgeDataCount = 0;
double walkingSpeed = 80.0; // m/min

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

// エッジキー文字列からノードを取得
int parseEdgeKey(const char *edgeKey, int *from, int *to) {
    return sscanf(edgeKey, "%d-%d", from, to);
}

// CSVファイルからエッジデータを読み込む
void loadRouteData(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open %s\n", filename);
        return;
    }
    
    char line[MAX_LINE_LENGTH];
    fgets(line, sizeof(line), file); // ヘッダーをスキップ
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        
        int from, to;
        double distance, gradient;
        int isSignal;
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
        
        // 残りのカラムをスキップして信号フラグを取得
        for (int i = 0; i < 3; i++) {
            token = strtok(NULL, ",");
            if (!token) break;
        }
        if (token) {
            isSignal = atoi(token);
        } else {
            isSignal = 0;
        }
        
        if (from > 0 && to > 0) {
            int edgeIdx = findEdgeIndex(from, to);
            if (edgeIdx < 0 && edgeDataCount < MAX_EDGES) {
                edgeIdx = edgeDataCount++;
                edgeDataArray[edgeIdx].from = from;
                edgeDataArray[edgeIdx].to = to;
                edgeDataArray[edgeIdx].signalCycle = 0;
                edgeDataArray[edgeIdx].signalGreen = 0;
                edgeDataArray[edgeIdx].signalPhase = 0.0;
            }
            if (edgeIdx >= 0) {
                edgeDataArray[edgeIdx].distance = distance;
                edgeDataArray[edgeIdx].gradient = gradient;
                edgeDataArray[edgeIdx].isSignal = isSignal;
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
    
    char line[MAX_LINE_LENGTH];
    fgets(line, sizeof(line), file); // ヘッダーをスキップ
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        
        char edgeKey[64];
        int cycle, green;
        double phase;
        
        if (sscanf(line, "%[^,],%d,%d,%lf", edgeKey, &cycle, &green, &phase) == 4) {
            int from, to;
            if (parseEdgeKey(edgeKey, &from, &to) == 2) {
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

// result2.txtから経路を読み込む
int loadRouteFromFile(const char *filename, char routeEdges[][64], int maxEdges) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open %s\n", filename);
        return 0;
    }
    
    int count = 0;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) && count < maxEdges) {
        // .geojsonを削除
        char *geojsonPos = strstr(line, ".geojson");
        if (geojsonPos) {
            *geojsonPos = '\0';
        }
        
        // 改行を削除
        char *newlinePos = strchr(line, '\n');
        if (newlinePos) {
            *newlinePos = '\0';
        }
        
        if (line[0] != '\0') {
            strncpy(routeEdges[count], line, 63);
            routeEdges[count][63] = '\0';
            count++;
        }
    }
    
    fclose(file);
    return count;
}

// 信号待ち時間を計算（基準位相を考慮）
double calculateWaitTimeWithReference(int edgeIdx, double cumulativeTime, double referencePhase) {
    EdgeData *edge = &edgeDataArray[edgeIdx];
    if (!edge->isSignal || edge->signalCycle <= 0) return 0.0;
    
    double phaseDiff = fabs(edge->signalPhase - referencePhase);
    double arrivalTime = cumulativeTime;
    double timeIntoCycle = fmod(
        arrivalTime - phaseDiff + edge->signalCycle,
        edge->signalCycle
    );
    
    if (timeIntoCycle > edge->signalGreen) {
        return edge->signalCycle - timeIntoCycle;
    }
    return 0.0;
}

// メイン処理
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <reference_edge> <walking_speed>\n", argv[0]);
        fprintf(stderr, "Example: %s 1-2 80\n", argv[0]);
        return 1;
    }
    
    char *referenceEdge = argv[1];
    walkingSpeed = atof(argv[2]);
    
    if (walkingSpeed <= 0) {
        fprintf(stderr, "Error: Invalid walking speed\n");
        return 1;
    }
    
    // データを読み込む
    loadRouteData("oomiya_route_inf_4.csv");
    loadSignalData("signal_inf.csv");
    
    // 基準信号の位相を取得
    int refFrom, refTo;
    if (parseEdgeKey(referenceEdge, &refFrom, &refTo) != 2) {
        fprintf(stderr, "Error: Invalid reference edge format\n");
        return 1;
    }
    
    int refEdgeIdx = findEdgeIndex(refFrom, refTo);
    if (refEdgeIdx < 0) {
        fprintf(stderr, "Error: Reference edge not found\n");
        return 1;
    }
    
    EdgeData *refEdge = &edgeDataArray[refEdgeIdx];
    if (!refEdge->isSignal) {
        fprintf(stderr, "Error: Reference edge is not a signal\n");
        return 1;
    }
    
    double referencePhase = refEdge->signalPhase;
    
    // 経路を読み込む
    char routeEdges[MAX_PATH_LENGTH][64];
    int routeEdgeCount = loadRouteFromFile("result2.txt", routeEdges, MAX_PATH_LENGTH);
    
    if (routeEdgeCount == 0) {
        fprintf(stderr, "Error: No route found in result2.txt\n");
        return 1;
    }
    
    // 信号化されたエッジをフィルタリング
    int signalizedEdges[MAX_PATH_LENGTH];
    int signalizedCount = 0;
    for (int i = 0; i < routeEdgeCount; i++) {
        int from, to;
        if (parseEdgeKey(routeEdges[i], &from, &to) == 2) {
            int edgeIdx = findEdgeIndex(from, to);
            if (edgeIdx >= 0 && edgeDataArray[edgeIdx].isSignal) {
                signalizedEdges[signalizedCount++] = edgeIdx;
            }
        }
    }
    
    // シミュレーションを実行
    double totalWaitTime = 0.0;
    double cumulativeTime = 0.0;
    
    for (int i = 0; i < routeEdgeCount; i++) {
        int from, to;
        if (parseEdgeKey(routeEdges[i], &from, &to) != 2) continue;
        
        int edgeIdx = findEdgeIndex(from, to);
        if (edgeIdx < 0) continue;
        
        EdgeData *edge = &edgeDataArray[edgeIdx];
        double adjustedSpeed = walkingSpeed * (1.0 - K_GRADIENT * edge->gradient);
        
        if (adjustedSpeed > 0) {
            double travelTime = edge->distance / adjustedSpeed; // 分
            cumulativeTime += travelTime * 60.0; // 秒に変換
            
            // 信号エッジの場合、待ち時間を計算
            if (edge->isSignal) {
                double waitTime = calculateWaitTimeWithReference(edgeIdx, cumulativeTime, referencePhase);
                totalWaitTime += waitTime;
                cumulativeTime += waitTime;
            }
        }
    }
    
    // JSON形式で結果を出力
    printf("{\"totalWaitTime\": %.6f}\n", totalWaitTime / 60.0);
    
    return 0;
}

