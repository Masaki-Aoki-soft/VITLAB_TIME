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
}

export interface CSVRow {
  node1: number;
  node2: number;
  distance: number;
  [key: string]: any;
}

