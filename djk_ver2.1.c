//全ての辺が非負数であれば問題なし、全ての辺に一律1000を足していたが辺の数変動によりコストが変わってしまう、ベルマンフォード法を使うべし
#include<stdio.h>
#include<stdlib.h>
#include<float.h>
#include<limits.h>
#include<time.h>

#define MAX_NODES 250//20241119時点大宮241の頂点，追加可能
#define INF DBL_MAX

typedef struct {
    int node;
    double weight;
} Edge;
//2024_09_10時点対象経路では1つの頂点から出る辺の最大数6のためedges[7]，変更、問題あり→修正完了
typedef struct {
    Edge edges[7];
    int edge_count;
} Graph;

//保存するグラフ，始点からの最短距離，ある交差点から１つ前の頂点（最短のもの），訪問の有無（関数内部でも使用するため）
Graph graph[MAX_NODES];
double distances[MAX_NODES];
int previous[MAX_NODES];
int visited[MAX_NODES];

//グラフに辺を追加、片方の辺追加に変更
void add_edge(int from, int to, int weight) {
    if (graph[from].edge_count >= 7) {
    printf("Error: Too many edges from node %d.\n", from);
    exit(1);
    }
    graph[from].edges[graph[from].edge_count].node = to;
    graph[from].edges[graph[from].edge_count].weight = weight;
    graph[from].edge_count++;
}
//最短のノードを算出
int extract_min(int num_nodes) {
    double min_distance = INF;
    int min_index = -1;

    //未訪問&&最小距離以下
    for (int i = 0; i < num_nodes; i++) {
        if (!visited[i] && distances[i] < min_distance) {
            min_distance = distances[i];
            min_index = i;
        }
    }

    return min_index;
}

void dijkstra(int start_node, int num_nodes) {
    // 初期化
    for (int i = 0; i < num_nodes; i++) {
        distances[i] = INF;
        previous[i] = -1;
        visited[i] = 0;
    }

    distances[start_node] = 0;  // 開始ノードの距離は0

    for (int i = 0; i < num_nodes - 1; i++) {
        int u = extract_min(num_nodes);  // 最短距離のノードを取得
        if (u == -1) break;
        visited[u] = 1;

        // 隣接ノードの距離を更新
        for (int j = 0; j < graph[u].edge_count; j++) {
            Edge edge = graph[u].edges[j];
            int v = edge.node;
            double weight = edge.weight;
            //訪れてない&&距離が限界値ではない&&合計が
            if (!visited[v] && distances[u] != INF && distances[u] + weight < distances[v]) {
                distances[v] = distances[u] + weight;
                previous[v] = u;
            }
        }
    }
}

void print_path(int node) {
    if (previous[node] == -1) {
        printf("%d", node);
        return;
    }
    print_path(previous[node]);
    printf(" -> %d", node);
}

void write_path(int node,FILE *file) {
    if (previous[node] == -1)return;
    write_path(previous[node],file);

    if(previous[node]<node)fprintf(file,"%d-%d.geojson\n",previous[node],node);
    else fprintf(file,"%d-%d.geojson\n",node,previous[node]);
}

int main(int argc,char *argv[]) {
    clock_t start_clock,end_clock;
    start_clock = clock();
    int start_node = atoi(argv[1]);  // 開始ノード
    int end_node = atoi(argv[2]);    // 終了ノード

    printf("start:%d,end:%d\n",start_node,end_node);

//printf("%d\n",argc);
    if(argc != 3){
        printf("始点と終点の指定をしてください ex) ./djk 1 241\n");
        exit(1);
    }

    if( (start_node<1 || 241<start_node) || (end_node<1 || 241<end_node)){
        printf("正しい値を入力してください。東大宮周辺:1~241\n");
        exit(1);
    }

    
    FILE *file = fopen("result.csv", "r");
    //FILE *file = fopen("result_2.csv", "r");
    //FILE *file = fopen("result_3.csv", "r");
    if (file == NULL) {
        printf("Error: Could not open file.\n");
        return 1;
    }

    int from, to;
    double weight;
    int num_nodes = 0;
    //int count_data = 0;//データ確認用

    // ファイルから辺の情報を読み込み 問題なし
    while (fscanf(file, "%d,%d,%lf", &from, &to, &weight) != EOF) {
        //printf("%d,%d,%lf\n", from, to, weight);
        //count_data++;
        add_edge(from, to, weight);

        if (from > num_nodes) num_nodes = from;
        if (to > num_nodes) num_nodes = to;
    }
    //確認用
    //printf("%d\n",count_data);

    fclose(file);
    num_nodes++;  // ノードの数は最大交差点番号+1

    // ダイクストラ法の実行
    dijkstra(start_node, num_nodes);


    //結果のファイル
    //file = fopen("result.txt", "w");
    file = fopen("result2.txt", "w");
    if (file == NULL) {
    printf("Error: result.txt cannot open\n");
    exit(1);
    }
    //最短経路のgeojsonファイル名を書き込む
    write_path(end_node,file);
    fclose(file);

    // 結果の描画
    /*printf("Shortest path from %d to %d:\n", start_node, end_node);
    print_path(end_node);
    printf("\nDistance: %d\n", distances[end_node]);
    */
   end_clock = clock();
   printf("clock:%f\n",(double)(end_clock-start_clock)/CLOCKS_PER_SEC);
    return 0;
}
