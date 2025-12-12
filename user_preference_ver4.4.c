#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<math.h>
//変更 一律加算　→　最後に最小の重みを足して最小値を0にする
//距離にも重みを追加　
//各条件の計算式の修正版
//負の値も考慮可能
//6つの条件値を加算から減算に変更（条件が有１の場合、優先されるから）
//d依存式をまとめる
//d依存式以外を平均距離/10で規格化
//その他0,1の値はそのまま加算する
#define INPUT_FILE "oomiya_route_inf_4.csv"
#define OUTPUT_FILE "result.csv"
#define MAX_LINE_LENGTH 843 //東大宮のデータ833行+1カラム
#define NUM_COLUMNS 16 //カラムの数、
#define NUM_PRE 13 //ユーザの好み勾配はkの値　距離も含めて13
#define Z_VALUE 5.0 //横断歩道の極大値
#define AVE_DISTANCE 64.35014//平均距離（大宮）
//#define AVE_DISTANCE 51.12988//平均距離（丸山台）
//#define POSITIVE_C 0//変数に変更//1000.0 //十分に大きな正の定数、（大宮_信号のみ-10~10倍の値で最小値-25.49）
#define MAX_COLUM 50


//始点終点と重みの格納
typedef struct {
    int start;
    int end;
    double weight;
} RE;


int main(int argc, char *argv[]) {
    double weights[NUM_PRE];//ユーザの好み、重み
    FILE *inputFile;
    FILE *outputFile;
    double POSITIVE_C =0; //コストの最小値（最小値を0にするため）
    int file_line_length=0;
    RE re[MAX_LINE_LENGTH-1]; //始点　終点　総合コストを格納構造体が必要
    //
    if (argc != (NUM_PRE + 3)) {
        printf("引数の数が合わない 引数%d個\n",NUM_PRE+2);
        return 1;
    }

    //コマンドライン引数（ユーザの好み）
    for (int i = 0; i < NUM_PRE; i++) {
        weights[i] = atof(argv[i + 1]);
        //printf("%s\n",argv[i + 1]);
    }
    //入力および出力ファイルの読み込み
    inputFile = fopen(INPUT_FILE, "r");
    if (inputFile == NULL) {
        perror("エラー：入力ファイル");
        return 1;
    }
    outputFile = fopen(OUTPUT_FILE, "w");
    if (outputFile == NULL) {
        perror("エラー：出力ファイル");
        fclose(inputFile);
        return 1;
    }

    //出力ファイルにヘッダーを書き込む、もともとカラムなし、追加したい場合は、djk.c関連のファイルも変更必要
    //fprintf(outputFile, "node1,node2,processed_result\n");

    char line[MAX_LINE_LENGTH];
    //ファイルの最後まで実行
    while (fgets(line, sizeof(line), inputFile)) {
        char *token;
        double values[NUM_COLUMNS];//１行のデータを格納
        int index = 0;

        //1行をカンマで分割して数値として格納
        token = strtok(line, ",");
        while (token != NULL && index < NUM_COLUMNS) {
            values[index++] = atof(token);
            token = strtok(NULL, ",");
        }
        //カラムの確認
        if (index != NUM_COLUMNS) {
            printf("行の形式が正しくありません: %s\n", line);
            continue;
        }

        //最初の余分な行を除去（カラム）
        if ((int)values[0] == 0 || (int)values[1] == 0){
            //printf("A\n");//出現回数確認用　１回
            continue;
            }
            

        //合計値の計算,最短距離の場合と選択される経路が変わらない
        //double processedDistance = values[2]*10 + POSITIVE_C; // 距離
        double processedDistance = 10*values[2]*weights[0]; //+ POSITIVE_C; // 距離
        //double processedDistance = values[2]*10;
        for (int i = 1; i < NUM_PRE; i++) {
            //勾配のとき
            if(i == 1)processedDistance += (280.5*pow(values[i + 3],5) - 58.7*pow(values[i + 3],4) - 76.8*pow(values[i + 3],3)
                     +51.9*pow(values[i + 3],2) + 19.6*values[i + 3] +2.5) * (values[2]/10) * weights[i];
            //最大勾配
            else if(i == 2){if((values[i+3] >= weights[i]))processedDistance += 5000;}
            //最小勾配
            else if(i == 3){if((values[i+3] <= weights[i]))processedDistance += 5000;}
            //道路幅
            else if(i==6)processedDistance += values[i+3]*weights[i]*(values[2]/10);
            //照明
            else if(i==7)processedDistance -= values[i+3]*weights[i]*(values[2]/10);
            //信号
            else if((i==5) || (i==10))processedDistance += values[i+3]*weights[i]*(AVE_DISTANCE);
            //その他
            else{
                if(values[i+3] == -1)processedDistance -= 5*weights[i]*(AVE_DISTANCE);
                else processedDistance -= values[i+3]*weights[i]*(AVE_DISTANCE);
                }
        }

        if(POSITIVE_C > processedDistance) POSITIVE_C = processedDistance;


        re[file_line_length].start = (int)values[0];
        re[file_line_length].end = (int)values[1];
        re[file_line_length].weight = processedDistance;
        file_line_length++;
        
        //結果を出力ファイルに書き込む
        //fprintf(outputFile, "%d,%d,%.10f\n", (int)values[0], (int)values[1], processedDistance);
    }

    printf("file_line_length:%d\n",file_line_length);
    printf("POSITIVE_C:%f\n",POSITIVE_C);
    //fprintf(outputFile, "POSITIVE_C,%f\n",POSITIVE_C);
    //負の値を最小値の加算によって０にしてファイルに記述
    for(int i=0;i<file_line_length;i++)fprintf(outputFile, "%d,%d,%.10f\n", (int)re[i].start, (int)re[i].end, re[i].weight - POSITIVE_C);
    //for(int i=0;i<file_line_length;i++)fprintf(outputFile, "%d,%d,%.10f\n", (int)re[i].start, (int)re[i].end, re[i].weight - POSITIVE_C);    

    //ファイルを閉じる
    fclose(inputFile);
    fclose(outputFile);
    //確認
    printf("処理が完了しました。結果は '%s' に保存されました。\n", OUTPUT_FILE);

    
    return 0;
}
