/* SPFAアルゴリズム */

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>
#include <stdbool.h>

#define MAX_NODES 300 // 最大のノード数を指定
#define INF DBL_MAX

// 
typedef struct {
    int items[MAX_NODES];
    int front;
    int rear;
} Queue;

void init_queue(Queue* q) {
    q->front = -1;
    q->rear = -1;
}

bool is_queue_empty(Queue* q) {
    return q->rear == -1;
}

void enqueue(Queue* q, int value) {
    if (q->rear == MAX_NODES - 1) {
        // printf("Queue is full\n");
    } else {
        if (q->front == -1) {
            q->front = 0;
        }
        q->rear++;
        q->items[q->rear] = value;
    }
}

int dequeue(Queue* q) {
    int item;
    if (is_queue_empty(q)) {
        // printf("Queue is empty\n");
        item = -1;
    } else {
        item = q->items[q->front];
        q->front++;
        if (q->front > q->rear) {
            // printf("Resetting queue\n");
            init_queue(q);
        }
    }
    return item;
}

// Graph structures
typedef struct {
    int node;
    double weight;
} Edge;

typedef struct {
    Edge edges[7];
    int edge_count;
} Graph;

Graph graph[MAX_NODES];
double distances[MAX_NODES];
int previous[MAX_NODES];

void add_edge(int from, int to, double weight) {
    if (graph[from].edge_count >= 7) {
        printf("Error: Too many edges from node %d.\n", from);
        exit(1);
    }
    graph[from].edges[graph[from].edge_count].node = to;
    graph[from].edges[graph[from].edge_count].weight = weight;
    graph[from].edge_count++;
}

void spfa(int start_node, int num_nodes) {
    bool in_queue[MAX_NODES] = {false};
    int count[MAX_NODES] = {0};
    Queue q;

    for (int i = 0; i < num_nodes; i++) {
        distances[i] = INF;
        previous[i] = -1;
    }

    distances[start_node] = 0;
    init_queue(&q);
    enqueue(&q, start_node);
    in_queue[start_node] = true;

    while (!is_queue_empty(&q)) {
        int u = dequeue(&q);
        in_queue[u] = false;

        if (count[u]++ > num_nodes) {
            printf("負の値が検出されました!\n");
            return;
        }

        for (int i = 0; i < graph[u].edge_count; i++) {
            Edge edge = graph[u].edges[i];
            int v = edge.node;
            double weight = edge.weight;

            if (distances[u] != INF && distances[u] + weight < distances[v]) {
                distances[v] = distances[u] + weight;
                previous[v] = u;
                if (!in_queue[v]) {
                    enqueue(&q, v);
                    in_queue[v] = true;
                }
            }
        }
    }
}

void write_path(int node, FILE *file) {
    if (previous[node] == -1) return;
    write_path(previous[node], file);

    if (previous[node] < node) {
        fprintf(file, "%d-%d.geojson\n", previous[node], node);
    } else {
        fprintf(file, "%d-%d.geojson\n", node, previous[node]);
    }
}

int main(int argc, char *argv[]) {
    clock_t start_clock, end_clock;
    start_clock = clock();

    if (argc != 3 && argc != 5) {
        printf("Usage: %s <start_node> <end_node> [blocked_node1] [blocked_node2]\n", argv[0]);
        exit(1);
    }

    int start_node = atoi(argv[1]);
    int end_node = atoi(argv[2]);
    int blocked_node1 = -1;
    int blocked_node2 = -1;

    if (argc == 5) {
        blocked_node1 = atoi(argv[3]);
        blocked_node2 = atoi(argv[4]);
    }

    printf("start:%d, end:%d\n", start_node, end_node);

    if ((start_node < 1 || MAX_NODES <= start_node) || (end_node < 1 || MAX_NODES <= end_node)) {
        printf("Please enter valid node numbers (1 to %d)\n", MAX_NODES - 1);
        exit(1);
    }

    // result.csvの読み込み
    FILE *file = fopen("result.csv", "r");
    if (file == NULL) {
        printf("Error: result.csvが開けません\n");
        return 1;
    }

    int from, to;
    double weight;
    int num_nodes = 0;

    while (fscanf(file, "%d,%d,%lf", &from, &to, &weight) != EOF) {
        if ((from == blocked_node1 && to == blocked_node2) || (from == blocked_node2 && to == blocked_node1)) {
            continue; // Skip the blocked edge
        }
        add_edge(from, to, weight);
        if (from > num_nodes) num_nodes = from;
        if (to > num_nodes) num_nodes = to;
    }
    fclose(file);
    num_nodes++;

    // spfaでの計算
    spfa(start_node, num_nodes);

    // result.txtの読み込み
    FILE *outfile = fopen("result2.txt", "w");
    if (outfile == NULL) {
        printf("エラー: result2.txt が開けません\n");
        exit(1);
    }
    write_path(end_node, outfile);
    fclose(outfile);

    end_clock = clock();
    printf("計算時間:%f\n", (double)(end_clock - start_clock) / CLOCKS_PER_SEC);
    return 0;
}
