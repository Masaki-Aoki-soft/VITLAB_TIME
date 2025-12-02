export interface RouteInfo {
    totalDistance: number;
    totalTime: number;
    totalWaitTime?: number;
}

export interface Route {
    info: RouteInfo | null;
    segments: string[];
    layers: any[];
}

export interface RouteResult {
    totalDistance: number;
    totalTime: number;
    totalWaitTime: number;
    userPref: string;
    routeType?: number;  // 0: 基準時刻1（青）, 1: 基準時刻2（緑）
    hasSignal?: number;  // 0: 信号なし, 1: 信号あり
    signalEdgeIdx?: number;  // 信号エッジのインデックス
}

export interface CSVRow {
    node1: number;
    node2: number;
    distance: number;
    [key: string]: any;
}
