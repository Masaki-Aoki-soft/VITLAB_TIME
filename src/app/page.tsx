'use client';

// SSRを無効化（windowオブジェクトを使用するため）
export const dynamic = 'force-dynamic';

import { useState, useEffect, useRef, useCallback } from 'react';
import dynamicImport from 'next/dynamic';
import toast from 'react-hot-toast';
import RouteInfo from '@/components/RouteInfo';
import WeightSlider from '@/components/WeightSlider';
import { RouteInfo as RouteInfoType, RouteResult, CSVRow } from '@/lib/types';
import { client } from '@/lib/HonoClient';

// Leafletマップコンポーネントを動的インポート（SSRを無効化）
const MapComponent = dynamicImport(() => import('@/components/MapComponent'), {
    ssr: false,
});

export default function Home() {
    // State管理
    const [isLoading, setIsLoading] = useState(false);
    const [route1, setRoute1] = useState<RouteInfoType | null>(null);
    const [route2, setRoute2] = useState<RouteInfoType | null>(null);
    const [savedRoute, setSavedRoute] = useState<RouteInfoType | null>(null);
    const [bestEnumRoute, setBestEnumRoute] = useState<RouteInfoType | null>(null);
    const [routeData, setRouteData] = useState<CSVRow[]>([]);
    const [foundRoutes, setFoundRoutes] = useState<RouteResult[]>([]);
    const [currentRouteIndex, setCurrentRouteIndex] = useState(0);
    const [savedRoutesList, setSavedRoutesList] = useState<string[]>([]);
    const [selectedSavedRoute, setSelectedSavedRoute] = useState('');

    // 重み設定のState
    const [weights, setWeights] = useState({
        weight0: 0,
        weight1: 0,
        weight2: 100,
        weight3: -100,
        weight4: 0,
        weight5: 0,
        weight6: 0,
        weight7: 0,
        weight8: 0,
        weight9: 0,
        weight10: 0,
        weight11: 0,
        weight12: 0,
    });

    const [params, setParams] = useState({
        param1: '1',
        param2: '246',
        walkingSpeed: '80',
    });

    const mapRef = useRef<any>(null);
    const [routeLayers, setRouteLayers] = useState<
        Array<{ data: any; style: any; routeInfo?: RouteResult }>
    >([]);
    const [dataLayers, setDataLayers] = useState<
        Array<{ data: any; style: any; popup?: string; edgeName?: string }>
    >([]);
    // 194-195 および他の対象経路用スライダーマーカー（青・赤すべて）をまとめて管理
    const slider194_195MarkersRef = useRef<any[]>([]);
    const slider194_195ValuesRef = useRef<Map<string, number>>(new Map());
    const slider194_195VisibleRef = useRef<boolean>(false);
    const slider194_195CreatingRef = useRef<boolean>(false); // 作成中フラグ
    const slider194_195TypeRef = useRef<'blue' | 'red'>('blue'); // 表示中のスライダータイプ
    const [slider194_195Type, setSlider194_195Type] = useState<'blue' | 'red'>('blue'); // 表示中のスライダータイプ（UI更新用）
    const slider194_195ClickCountRef = useRef<number>(0); // クリック回数をカウント
    const slider194_195RouteLayerRef = useRef<L.GeoJSON | null>(null); // 194-195の経路レイヤー（紫）
    const zoomTimeoutRef = useRef<NodeJS.Timeout | null>(null); // ズーム時のデバウンス用
    const isRedrawingRef = useRef<boolean>(false); // 再描画中かどうかのフラグ
    const [selectedRouteInfo, setSelectedRouteInfo] = useState<RouteResult | null>(null);
    const [startMarker, setStartMarker] = useState<{
        position: [number, number];
        nodeId: string;
        startNode: string;
        endNode: string;
    } | null>(null);
    const [endMarker, setEndMarker] = useState<{
        position: [number, number];
        nodeId: string;
    } | null>(null);
    const [showIntersectionPins, setShowIntersectionPins] = useState(false);
    const [intersectionPins, setIntersectionPins] = useState<
        Array<{
            position: [number, number];
            nodeId: string;
        }>
    >([]);
    const [pinSelectionState, setPinSelectionState] = useState<'none' | 'start' | 'end'>('none');
    const geojsonFeaturesRef = useRef<any[]>([]);
    const geojsonFileNamesRef = useRef<Array<string>>([]);
    const otherLayersRef = useRef<any[]>([]);

    // CSVデータの読み込み
    const loadCSVData = async () => {
        if (routeData.length > 0) return routeData;
        try {
            // PapaParseを動的にインポート
            const PapaModule = await import('papaparse');
            const Papa = PapaModule.default;

            const response = await client.api.main_server_route.csvData.$get();
            const csvText = await response.text();
            const parsed = Papa.parse<CSVRow>(csvText, {
                header: true,
                dynamicTyping: true,
                skipEmptyLines: true,
            });
            setRouteData(parsed.data);
            return parsed.data;
        } catch (error) {
            console.error('CSVデータのロードに失敗:', error);
            return [];
        }
    };

    // 保存済みルート一覧の読み込み
    const loadSavedRoutesList = async () => {
        try {
            const response = await client.api.main_server_route.listSavedRoutes.$get();
            const files = await response.json();
            if (Array.isArray(files)) {
                setSavedRoutesList(files);
            } else {
                console.error('保存済みルート一覧の取得に失敗:', files);
            }
        } catch (error) {
            console.error('保存済みルート一覧の取得に失敗:', error);
        }
    };

    // 経路計算
    const calc = async (): Promise<RouteResult[]> => {
        try {
            const response = await client.api.main_server_route.calc.$post({
                json: {
                    param1: params.param1,
                    param2: params.param2,
                    weight0: weights.weight0.toString(),
                    weight1: weights.weight1.toString(),
                    weight2: weights.weight2.toString(),
                    weight3: weights.weight3.toString(),
                    weight4: weights.weight4.toString(),
                    weight5: weights.weight5.toString(),
                    weight6: weights.weight6.toString(),
                    weight7: weights.weight7.toString(),
                    weight8: weights.weight8.toString(),
                    weight9: weights.weight9.toString(),
                    weight10: weights.weight10.toString(),
                    weight11: weights.weight11.toString(),
                    weight12: weights.weight12.toString(),
                    walkingSpeed: params.walkingSpeed,
                },
            });

            if (!response.ok) {
                const err = await response.json();
                if (err && typeof err === 'object' && 'error' in err) {
                    throw new Error(err.error || `HTTP error! status: ${response.status}`);
                }
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const output: any = await response.json();
            if (Array.isArray(output)) {
                console.log('計算完了:', output);
                return output;
            } else if (output && typeof output === 'object' && 'error' in output) {
                throw new Error((output as { error: string }).error || '経路計算に失敗しました');
            }
            throw new Error('予期しないレスポンス形式です');
        } catch (error: any) {
            console.error('経路計算エラー:', error);
            throw error;
        }
    };

    // 経路を検索
    const loadRouteFromCSV = async () => {
        // 最初にマップ上の経路などを全てクリア
        setRouteLayers([]);
        setDataLayers([]);
        setStartMarker(null);
        otherLayersRef.current.forEach((layer) => {
            if (mapRef.current) {
                mapRef.current.removeLayer(layer);
            }
        });
        otherLayersRef.current = [];
        setRoute1(null);
        setRoute2(null);
        setSavedRoute(null);

        setIsLoading(true);
        toast.promise(
            new Promise<string>(async (resolve, reject) => {
                try {
                    await loadCSVData();
                    const routes = await calc();
                    setFoundRoutes(routes);
                    setCurrentRouteIndex(0);

                    if (routes && routes.length > 0) {
                        await drawAllRoutes(routes);
                        resolve(`${routes.length}件の経路が見つかり、全て表示しました。`);
                    } else {
                        reject('経路が見つかりませんでした。');
                    }
                } catch (error: any) {
                    console.error('ルートの読み込みまたは計算中にエラーが発生しました:', error);
                    reject(error.message || 'エラーが発生しました');
                } finally {
                    setIsLoading(false);
                }
            }),
            {
                loading: '経路を検索中...',
                success: (message: string) => message,
                error: (message: string) => message,
            }
        );
    };

    // 全ての経路を描画
    const drawAllRoutes = async (routes: RouteResult[]) => {
        if (!routes || routes.length === 0) return;

        const geojsonFolder = 'oomiya_line/';

        // 新しいロジックに基づく分類：
        // routeType=0（青）: 基準時刻2（基準時刻1 >= 基準時刻2の場合のみ表示）
        // routeType=1（緑）: 基準時刻1
        // routeType=2（赤）: 最短全網羅経路（信号待ち時間を含めた総時間が最短、1本のみ）
        let baseTime1Route: RouteResult | null = null; // 基準時刻1（緑）- routeType=1
        let baseTime2Route: RouteResult | null = null; // 基準時刻2（青）- routeType=0
        let bestEnumRoute: RouteResult | null = null; // 最短全網羅経路（赤）- routeType=2

        // 重複する経路を除外
        const seenRoutes = new Set<string>();
        for (const route of routes) {
            const routeKey = route.userPref.trim();
            const routeType = route.routeType ?? 2; // デフォルトは全網羅

            // 重複チェック（同じuserPrefは1回だけ追加）
            if (seenRoutes.has(routeKey)) {
                continue;
            }
            seenRoutes.add(routeKey);

            if (routeType === 0) {
                // 基準時刻2（青）- 最初の1本のみ
                if (!baseTime2Route) {
                    baseTime2Route = route;
                }
            } else if (routeType === 1) {
                // 基準時刻1（緑）- 最初の1本のみ
                if (!baseTime1Route) {
                    baseTime1Route = route;
                }
            } else if (routeType === 2) {
                // 最短全網羅経路（赤）- 最初の1本のみ
                if (!bestEnumRoute) {
                    bestEnumRoute = route;
                }
            }
        }

        // 基準時刻1（緑）の経路を設定
        if (baseTime1Route) {
            setRoute1({
                totalDistance: baseTime1Route.totalDistance,
                totalTime: baseTime1Route.totalTime,
                totalWaitTime: baseTime1Route.totalWaitTime || 0,
            });
        }

        // 基準時刻2（青）の経路を設定
        if (baseTime2Route) {
            setRoute2({
                totalDistance: baseTime2Route.totalDistance,
                totalTime: baseTime2Route.totalTime,
                totalWaitTime: baseTime2Route.totalWaitTime || 0,
            });
        }

        // 最短全網羅経路（赤）をbestEnumRouteとして設定
        if (bestEnumRoute) {
            setBestEnumRoute({
                totalDistance: bestEnumRoute.totalDistance,
                totalTime: bestEnumRoute.totalTime,
                totalWaitTime: bestEnumRoute.totalWaitTime || 0,
            });
        } else {
            // bestEnumRouteがない場合は、クリア
            setBestEnumRoute(null);
        }

        // 経路を描画（react-leaflet用にstateに追加）
        const newLayers: Array<{ data: any; style: any; routeInfo?: RouteResult }> = [];

        // 基準時刻1（緑）の経路を描画（1本のみ）
        const greenColor = '#2ed573';
        if (baseTime1Route) {
            const segments = baseTime1Route.userPref
                .split('\n')
                .filter((line) => line.trim() !== '');
            for (const filename of segments) {
                try {
                    const filePath = `/api/main_server_route/static/${geojsonFolder}${filename.trim()}`;
                    const response = await fetch(filePath);
                    if (!response.ok) continue;
                    const data = await response.json();
                    newLayers.push({
                        data,
                        style: {
                            color: greenColor,
                            weight: 8,
                            opacity: 0.8,
                        },
                        routeInfo: baseTime1Route,
                    });
                } catch (err) {
                    console.warn(`Error loading ${filename}:`, err);
                }
            }
        }

        // 基準時刻2（青）の経路を描画（1本のみ）
        const blueColor = '#3742fa';
        if (baseTime2Route) {
            const segments = baseTime2Route.userPref
                .split('\n')
                .filter((line) => line.trim() !== '');
            for (const filename of segments) {
                try {
                    const filePath = `/api/main_server_route/static/${geojsonFolder}${filename.trim()}`;
                    const response = await fetch(filePath);
                    if (!response.ok) continue;
                    const data = await response.json();
                    newLayers.push({
                        data,
                        style: {
                            color: blueColor,
                            weight: 8,
                            opacity: 0.8,
                        },
                        routeInfo: baseTime2Route,
                    });
                } catch (err) {
                    console.warn(`Error loading ${filename}:`, err);
                }
            }
        }

        // 最短全網羅経路（赤）を描画（1本のみ）
        const redColor = '#ff4757';
        if (bestEnumRoute) {
            const segments = bestEnumRoute.userPref
                .split('\n')
                .filter((line) => line.trim() !== '');
            for (const filename of segments) {
                try {
                    const filePath = `/api/main_server_route/static/${geojsonFolder}${filename.trim()}`;
                    const response = await fetch(filePath);
                    if (!response.ok) continue;
                    const data = await response.json();
                    newLayers.push({
                        data,
                        style: {
                            color: redColor,
                            weight: 8,
                            opacity: 0.8,
                        },
                        routeInfo: bestEnumRoute,
                    });
                } catch (err) {
                    console.warn(`Error loading ${filename}:`, err);
                }
            }
        }

        setRouteLayers(newLayers);
    };

    // 全経路を表示（第三段階の全網羅経路を全て黄色で表示）
    const loadRouteFromCSV2 = async () => {
        if (!foundRoutes || foundRoutes.length === 0) {
            toast.error('まず「経路を検索」してください。');
            return;
        }

        // routeType=3（全網羅経路）のみをフィルタリング
        const allEnumRoutes = foundRoutes.filter((route) => route.routeType === 3);

        if (allEnumRoutes.length === 0) {
            toast.error('全網羅経路が見つかりませんでした。');
            return;
        }

        setIsLoading(true);

        toast.promise(
            new Promise<string>(async (resolve, reject) => {
                try {
                    const geojsonFolder = 'oomiya_line/';
                    const yellowColor = '#ffd700'; // 黄色
                    const weight = 6;

                    const newLayers: Array<{ data: any; style: any; routeInfo?: RouteResult }> = [];

                    // 既存の経路レイヤーを保持（青・緑・赤）
                    const existingLayers = routeLayers;
                    newLayers.push(...existingLayers);

                    // 全網羅経路（routeType=3）を全て黄色で追加
                    for (const route of allEnumRoutes) {
                        const segments = route.userPref
                            .split('\n')
                            .filter((line) => line.trim() !== '');

                        for (const filename of segments) {
                            try {
                                const filePath = `/api/main_server_route/static/${geojsonFolder}${filename.trim()}`;
                                const response = await fetch(filePath);
                                if (!response.ok) continue;
                                const data = await response.json();
                                newLayers.push({
                                    data,
                                    style: {
                                        color: yellowColor,
                                        weight: weight,
                                        opacity: 0.7,
                                    },
                                    routeInfo: route,
                                });
                            } catch (err) {
                                console.warn(`Error loading ${filename}:`, err);
                            }
                        }
                    }
                    setRouteLayers(newLayers);

                    resolve(`${allEnumRoutes.length}本の全網羅経路を黄色で表示しました`);
                } catch (error: any) {
                    console.error('全経路の表示中にエラーが発生しました:', error);
                    reject(error.message || 'エラーが発生しました');
                } finally {
                    setIsLoading(false);
                }
            }),
            {
                loading: '全網羅経路を表示中...',
                success: (message: string) => message,
                error: (message: string) => message,
            }
        );
    };

    // データ表示関数
    const displayDataFromCSV = async (dataType: string, color: string) => {
        await loadCSVData();
        const geojsonFolder = 'oomiya_line/';
        const newLayers: Array<{ data: any; style: any; popup?: string; edgeName?: string }> = [];

        if (dataType === 'signal') {
            let foundCount = 0;
            const promises: Promise<void>[] = [];

            routeData.forEach((row) => {
                if (row.signal == 1) {
                    foundCount++;
                    const edgeName = `${row.node1}-${row.node2}`;
                    const filename = `${edgeName}.geojson`;

                    const filePath = `/api/main_server_route/static/${geojsonFolder}${filename}`;
                    const promise = fetch(filePath)
                        .then((response) => {
                            if (!response.ok) return null;
                            return response.json();
                        })
                        .then((data) => {
                            if (data) {
                                newLayers.push({
                                    data,
                                    style: { color: color, weight: 15, opacity: 0.5 },
                                    popup: `信号区間: ${edgeName}<br>クリックで待ち時間を計算`,
                                    edgeName,
                                });
                            }
                        })
                        .catch(() => {});
                    promises.push(promise);
                }
            });

            await Promise.all(promises);
            if (newLayers.length > 0) {
                setDataLayers((prev) => [...prev, ...newLayers]);
            }
            if (foundCount === 0) {
                alert(`'${dataType}' に該当するデータが見つかりませんでした。`);
            }
        } else {
            let foundCount = 0;
            const promises: Promise<void>[] = [];

            routeData.forEach((row) => {
                let conditionMet = false;

                switch (dataType) {
                    case 'sidewalk':
                        if (row.sidewalk == 1) conditionMet = true;
                        break;
                    case 'nature':
                        if (row.nature == 1) conditionMet = true;
                        break;
                    case 'road_width1':
                        if (row.road_width == 1) conditionMet = true;
                        break;
                    case 'road_width2':
                        if (row.road_width == 2) conditionMet = true;
                        break;
                    case 'road_width3':
                        if (row.road_width >= 3) conditionMet = true;
                        break;
                    case 'park':
                        if (row.park >= 1) conditionMet = true;
                        break;
                    case 'illumination':
                        if (row.illumination >= 1) conditionMet = true;
                        break;
                    case 'crosswalk':
                        if (row.crosswalk >= 1) conditionMet = true;
                        break;
                    case 'garbage':
                        if (row.garbage >= 1) conditionMet = true;
                        break;
                    case 'toilet':
                        if (row.toilet == 1) conditionMet = true;
                        break;
                }

                if (conditionMet) {
                    foundCount++;
                    const filename = `${row.node1}-${row.node2}.geojson`;

                    const filePath = `/api/main_server_route/static/${geojsonFolder}${filename}`;
                    const promise = fetch(filePath)
                        .then((response) => {
                            if (!response.ok) return null;
                            return response.json();
                        })
                        .then((data) => {
                            if (data) {
                                newLayers.push({
                                    data,
                                    style: { color: color, weight: 15, opacity: 0.3 },
                                });
                            }
                        })
                        .catch(() => {});
                    promises.push(promise);
                }
            });

            await Promise.all(promises);
            if (newLayers.length > 0) {
                setDataLayers((prev) => [...prev, ...newLayers]);
            }
            if (foundCount === 0) {
                alert(`'${dataType}' に該当するデータが見つかりませんでした。`);
            }
        }
    };

    // 対象範囲を表示
    const viewAREA = async () => {
        try {
            const response = await fetch('/api/main_server_route/static/RE_AREA.geojson');
            if (!response.ok)
                throw new Error(`Failed to fetch GeoJSON file: ${response.statusText}`);
            const geojsonData = await response.json();
            setDataLayers((prev) => [
                ...prev,
                {
                    data: geojsonData,
                    style: { color: 'black', weight: 8, opacity: 0.5 },
                },
            ]);
        } catch (error) {
            console.error('Error loading GeoJSON:', error);
            alert('GeoJSONファイルの読み込みに失敗しました。');
        }
    };

    // クリア
    const clearGeoJSON = () => {
        setRouteLayers([]);
        setDataLayers([]);
        setStartMarker(null);
        setEndMarker(null);
        setSelectedRouteInfo(null);
        otherLayersRef.current.forEach((layer) => {
            if (mapRef.current) {
                mapRef.current.removeLayer(layer);
            }
        });
        otherLayersRef.current = [];
        setRoute1(null);
        setRoute2(null);
        setSavedRoute(null);
        setBestEnumRoute(null);
        setFoundRoutes([]);
        setCurrentRouteIndex(0);

        // 194-195のスライダーと紫色の経路を削除
        const map = mapRef.current;
        if (map) {
            // 194-195のスライダーマーカーを削除
            slider194_195MarkersRef.current.forEach((marker) => {
                try {
                    if (map.hasLayer(marker)) {
                        map.removeLayer(marker);
                    }
                    // クリーンアップ関数があれば実行
                    if ((marker as any)._cleanupEvents) {
                        (marker as any)._cleanupEvents();
                    }
                } catch (e) {
                    console.warn('スライダーマーカーの削除でエラー:', e);
                }
            });
            slider194_195MarkersRef.current = [];

            // 194-195の紫色の経路レイヤーを削除
            if (slider194_195RouteLayerRef.current) {
                try {
                    const routeLayer = slider194_195RouteLayerRef.current;
                    if (map.hasLayer(routeLayer)) {
                        map.removeLayer(routeLayer);
                    }
                } catch (e) {
                    console.warn('経路レイヤーの削除でエラー:', e);
                }
                slider194_195RouteLayerRef.current = null;
            }

            // 切り替えボタンを削除
            const toggleButton = document.getElementById('194-195-toggle-button');
            if (toggleButton) {
                toggleButton.remove();
            }

            // スライダーの値をリセット
            slider194_195ValuesRef.current.clear();
            slider194_195VisibleRef.current = false;
            slider194_195TypeRef.current = 'blue';
            setSlider194_195Type('blue');
        }
    };

    // パラメータ変更時にマーカーを更新
    useEffect(() => {
        if (startMarker) {
            setStartMarker({
                ...startMarker,
                startNode: params.param1,
                endNode: params.param2,
            });
        }
    }, [params.param1, params.param2]);

    // ピンを配置するボタンの処理
    const handleShowPins = () => {
        if (showIntersectionPins) {
            // ピンを非表示
            setShowIntersectionPins(false);
            setIntersectionPins([]);
            setPinSelectionState('none');
        } else {
            // 全ての交差点のピンを表示
            const pins: Array<{ position: [number, number]; nodeId: string }> = [];
            geojsonFeaturesRef.current.forEach((feature, index) => {
                if (index < geojsonFileNamesRef.current.length) {
                    const fileName = geojsonFileNamesRef.current[index];
                    if (fileName) {
                        const nodeId = fileName.replace('.geojson', '');
                        pins.push({
                            position: [
                                feature.geometry.coordinates[1],
                                feature.geometry.coordinates[0],
                            ],
                            nodeId: nodeId,
                        });
                    }
                }
            });
            setIntersectionPins(pins);
            setShowIntersectionPins(true);
            setPinSelectionState('start');
        }
    };

    // 交差点ピンをクリックしたときの処理
    const handleIntersectionPinClick = (nodeId: string, position: [number, number]) => {
        if (pinSelectionState === 'start') {
            // 始点を設定
            setParams({ ...params, param1: nodeId });
            setStartMarker({
                position: position,
                nodeId: nodeId,
                startNode: nodeId,
                endNode: params.param2,
            });
            setPinSelectionState('end');
        } else if (pinSelectionState === 'end') {
            // 終点を設定
            setParams({ ...params, param2: nodeId });
            setEndMarker({
                position: position,
                nodeId: nodeId,
            });
            // 全てのピンを非表示
            setShowIntersectionPins(false);
            setIntersectionPins([]);
            setPinSelectionState('none');
        }
    };

    // マップクリック時の処理（廃止 - 何もしない）
    const handleMapClick = async (lat: number, lng: number) => {
        // 機能を廃止 - 何もしない
    };

    // 194-195 など対象経路にスライダーを表示する関数（page.tsx内で直接管理）
    const showSliderOn194_195 = useCallback(async (isRedrawing: boolean = false) => {
        // ブラウザ環境でのみ実行
        if (typeof window === 'undefined') return;

        // 再描画時かどうかを設定
        isRedrawingRef.current = isRedrawing;

        // 既に作成中の場合は、少し待ってから再試行
        if (slider194_195CreatingRef.current) {
            // 100ms後に再試行（ズーム中などで連続呼び出しされる場合に対応）
            setTimeout(() => {
                if (!slider194_195CreatingRef.current && slider194_195VisibleRef.current) {
                    showSliderOn194_195();
                }
            }, 100);
            return;
        }
        slider194_195CreatingRef.current = true;

        try {
            const map = mapRef.current;
            if (!map) {
                slider194_195CreatingRef.current = false;
                return;
            }

            // Leafletを動的にインポート
            const L = await import('leaflet');
            const maxValue = 47; // 48段階（0-47）

            // 既存の全スライダーマーカーを削除（重複防止）
            slider194_195MarkersRef.current.forEach((marker) => {
                try {
                    if ((marker as any)._cleanupEvents) {
                        (marker as any)._cleanupEvents();
                    }
                    if (map.hasLayer(marker)) {
                        map.removeLayer(marker);
                    }
                } catch {
                    // 既に削除されている場合は無視
                }
            });
            slider194_195MarkersRef.current = [];

            // 既存の194-195経路レイヤーを削除（※再描画時は残す）
            if (slider194_195RouteLayerRef.current && !isRedrawingRef.current) {
                try {
                    if (map.hasLayer(slider194_195RouteLayerRef.current)) {
                        map.removeLayer(slider194_195RouteLayerRef.current);
                    }
                } catch {
                    // 既に削除されている場合は無視
                }
                slider194_195RouteLayerRef.current = null;
            }

            // 既存の DOM（スライダー input 要素・切り替えボタン）を全削除
            const sliderInputs = document.querySelectorAll('[id$="-slider-1"], [id$="-slider-2"]');
            sliderInputs.forEach((el) => {
                el.parentElement?.remove();
            });
            const existingToggleButton = document.getElementById('194-195-toggle-button');
            if (existingToggleButton) {
                existingToggleButton.remove();
            }

            // 少し待ってから新しいスライダーを作成（DOMのクリーンアップを確実に）
            await new Promise((resolve) => setTimeout(resolve, 50));

            // スライダーを出す対象経路一覧
            const edges = ['194-195', '22-25', '192-194', '18-22', '26-195', '197-199'];

            const allBlueMarkers: any[] = [];
            const allRedMarkers: any[] = [];

            for (const edgeId of edges) {
                // 各 edgeId ごとに GeoJSON を読み込む
                const geojsonPath = `/api/main_server_route/static/oomiya_line/${edgeId}.geojson`;
                const response = await fetch(geojsonPath);
                if (!response.ok) {
                    console.error(`${edgeId} のGeoJSONを読み込めませんでした`);
                    continue;
                }
                const geojsonData = await response.json();

                const geoJsonLayer = L.default.geoJSON(geojsonData);
                const features = geoJsonLayer.getLayers() as any[];
                if (features.length === 0) continue;

                const feature = features[0];
                const latlngs = feature.getLatLngs() as any[];
                if (latlngs.length < 2) continue;

                const startPoint = latlngs[0];
                const endPoint = latlngs[latlngs.length - 1];

                // 経路の方向と長さを計算
                const startPixel = map.latLngToContainerPoint(startPoint);
                const endPixel = map.latLngToContainerPoint(endPoint);
                const dx = endPixel.x - startPixel.x;
                const dy = endPixel.y - startPixel.y;
                const angle = Math.atan2(dy, dx) * (180 / Math.PI);
                const pixelLength = Math.sqrt(dx * dx + dy * dy);

                // 経路の中心点付近にスライダーを配置
                const centerPoint = L.default.latLng(
                    (startPoint.lat + endPoint.lat) / 2,
                    (startPoint.lng + endPoint.lng) / 2
                );

                const perpendicularAngle = angle + 90;
                // 22-25 と 18-22 のスライダーだけ、通常より上側へずらす
                const offsetPixels = edgeId === '22-25' || edgeId === '18-22' ? -10 : 15; // 経路から少し離す（視認性のため）
                const offsetAngleRad = (perpendicularAngle * Math.PI) / 180;
                const centerPixel = map.latLngToContainerPoint(centerPoint);

                // 26-195 のスライダーを左にずらすための追加オフセット
                const leftOffsetPixels = edgeId === '26-195' ? -20 : 0;
                const leftOffsetAngleRad = (angle * Math.PI) / 180; // 経路の角度に対して左側（-90度方向）

                const offsetPixel = L.default.point(
                    centerPixel.x +
                        Math.cos(offsetAngleRad) * offsetPixels +
                        Math.cos(leftOffsetAngleRad - Math.PI / 2) * leftOffsetPixels,
                    centerPixel.y +
                        Math.sin(offsetAngleRad) * offsetPixels +
                        Math.sin(leftOffsetAngleRad - Math.PI / 2) * leftOffsetPixels
                );
                const sliderPosition = map.containerPointToLatLng(offsetPixel);

                // スライダーの値（1つ目と2つ目で独立）
                const slider1Id = `${edgeId}-slider-1`;
                const slider2Id = `${edgeId}-slider-2`;
                const slider1Key = `${edgeId}-slider1`;
                const slider2Key = `${edgeId}-slider2`;
                const currentValue1 = slider194_195ValuesRef.current.get(slider1Key) ?? 0;
                const currentValue2 = slider194_195ValuesRef.current.get(slider2Key) ?? 0;

                const sliderWidth = pixelLength;

                // 1つ目のスライダー（青と白）
                const slider1DivIcon = L.default.divIcon({
                    className: 'route-weight-slider',
                    html: `
                    <div style="
                        transform: rotate(${angle}deg);
                        transform-origin: center center;
                        pointer-events: auto;
                        display: flex;
                        align-items: center;
                        justify-content: center;
                    ">
                        <input
                            type="range"
                            id="${slider1Id}"
                            value="${currentValue1}"
                            min="0"
                            max="${maxValue}"
                            step="1"
                            style="
                                width: ${sliderWidth}px;
                                height: 6px;
                                background: #3b82f6;
                                border-radius: 3px;
                                outline: none;
                                cursor: pointer;
                                -webkit-appearance: none;
                                appearance: none;
                                pointer-events: auto;
                                margin: 0;
                            "
                        />
                        <style>
                            #${slider1Id}::-webkit-slider-thumb {
                                -webkit-appearance: none;
                                appearance: none;
                                width: 16px;
                                height: 16px;
                                background: white;
                                border: 2px solid #3b82f6;
                                border-radius: 50%;
                                cursor: pointer;
                                box-shadow: 0 2px 4px rgba(0,0,0,0.3);
                                opacity: 0;
                                transition: opacity 0.2s;
                            }
                            #${slider1Id}::-moz-range-thumb {
                                width: 16px;
                                height: 16px;
                                background: white;
                                border: 2px solid #3b82f6;
                                border-radius: 50%;
                                cursor: pointer;
                                box-shadow: 0 2px 4px rgba(0,0,0,0.3);
                                opacity: 0;
                                transition: opacity 0.2s;
                            }
                            #${slider1Id}.thumb-visible::-webkit-slider-thumb {
                                opacity: 1;
                            }
                            #${slider1Id}.thumb-visible::-moz-range-thumb {
                                opacity: 1;
                            }
                            #${slider1Id}::-webkit-slider-runnable-track {
                                background: #3b82f6;
                                height: 6px;
                                border-radius: 3px;
                            }
                            #${slider1Id}::-moz-range-track {
                                background: #3b82f6;
                                height: 6px;
                                border-radius: 3px;
                            }
                        </style>
                    </div>
                `,
                    iconSize: [sliderWidth, 20],
                    iconAnchor: [sliderWidth / 2, 10],
                });

                // 2つ目のスライダー（赤と白）
                const slider2DivIcon = L.default.divIcon({
                    className: 'route-weight-slider',
                    html: `
                    <div style="
                        transform: rotate(${angle}deg);
                        transform-origin: center center;
                        pointer-events: auto;
                        display: flex;
                        align-items: center;
                        justify-content: center;
                    ">
                        <input
                            type="range"
                            id="${slider2Id}"
                            value="${currentValue2}"
                            min="0"
                            max="${maxValue}"
                            step="1"
                            style="
                                width: ${sliderWidth}px;
                                height: 6px;
                                background: #ef4444;
                                border-radius: 3px;
                                outline: none;
                                cursor: pointer;
                                -webkit-appearance: none;
                                appearance: none;
                                pointer-events: auto;
                                margin: 0;
                            "
                        />
                        <style>
                            #${slider2Id}::-webkit-slider-thumb {
                                -webkit-appearance: none;
                                appearance: none;
                                width: 16px;
                                height: 16px;
                                background: white;
                                border: 2px solid #ef4444;
                                border-radius: 50%;
                                cursor: pointer;
                                box-shadow: 0 2px 4px rgba(0,0,0,0.3);
                                opacity: 0;
                                transition: opacity 0.2s;
                            }
                            #${slider2Id}::-moz-range-thumb {
                                width: 16px;
                                height: 16px;
                                background: white;
                                border: 2px solid #ef4444;
                                border-radius: 50%;
                                cursor: pointer;
                                box-shadow: 0 2px 4px rgba(0,0,0,0.3);
                                opacity: 0;
                                transition: opacity 0.2s;
                            }
                            #${slider2Id}.thumb-visible::-webkit-slider-thumb {
                                opacity: 1;
                            }
                            #${slider2Id}.thumb-visible::-moz-range-thumb {
                                opacity: 1;
                            }
                            #${slider2Id}::-webkit-slider-runnable-track {
                                background: #ef4444;
                                height: 6px;
                                border-radius: 3px;
                            }
                            #${slider2Id}::-moz-range-track {
                                background: #ef4444;
                                height: 6px;
                                border-radius: 3px;
                            }
                        </style>
                    </div>
                `,
                    iconSize: [sliderWidth, 20],
                    iconAnchor: [sliderWidth / 2, 10],
                });

                // マーカーを作成（各 edgeId ごとに同じ位置に配置）
                const slider1Marker = L.default.marker(sliderPosition, {
                    icon: slider1DivIcon,
                    interactive: true,
                });

                const slider2Marker = L.default.marker(sliderPosition, {
                    icon: slider2DivIcon,
                    interactive: true,
                });

                // マーカーのクリックイベントを無効化
                [slider1Marker, slider2Marker].forEach((marker) => {
                    marker.on('click', (e) => {
                        L.default.DomEvent.stopPropagation(e);
                    });
                    marker.on('dblclick', (e) => {
                        L.default.DomEvent.stopPropagation(e);
                    });
                });

                // 1つ目のスライダーのイベントリスナー（青）
                slider1Marker.on('add', () => {
                    setTimeout(() => {
                        const sliderElement = document.getElementById(
                            slider1Id
                        ) as HTMLInputElement;
                        if (!sliderElement) return;

                        let isDragging = false;

                        const disableMapDragging = () => {
                            if (map.dragging) {
                                map.dragging.disable();
                            }
                        };

                        const enableMapDragging = () => {
                            if (map.dragging && !isDragging) {
                                map.dragging.enable();
                            }
                        };

                        const handleMouseDown = (e: MouseEvent) => {
                            isDragging = true;
                            disableMapDragging();
                            // スライダーのハンドルを表示
                            sliderElement.classList.add('thumb-visible');
                            // クリック時に現在の値で経路を表示
                            const currentValue = parseInt(sliderElement.value);
                            handleSlider194_195Change(edgeId, currentValue, 1);
                            e.stopPropagation();
                            e.stopImmediatePropagation();
                        };

                        const handleMouseMove = (e: MouseEvent) => {
                            if (isDragging) {
                                disableMapDragging();
                                e.stopPropagation();
                                e.stopImmediatePropagation();
                            }
                        };

                        const handleMouseUp = () => {
                            isDragging = false;
                            enableMapDragging();
                        };

                        const handleMouseLeave = () => {
                            isDragging = false;
                            enableMapDragging();
                        };

                        // 既存のイベントリスナーを削除してから追加（重複防止）
                        sliderElement.removeEventListener('mousedown', handleMouseDown);
                        sliderElement.removeEventListener('mousemove', handleMouseMove);
                        sliderElement.removeEventListener('mouseup', handleMouseUp);
                        sliderElement.removeEventListener('mouseleave', handleMouseLeave);

                        sliderElement.addEventListener('mousedown', handleMouseDown, true);
                        sliderElement.addEventListener('mousemove', handleMouseMove, true);
                        sliderElement.addEventListener('mouseup', handleMouseUp, true);
                        sliderElement.addEventListener('mouseleave', handleMouseLeave, true);

                        // グローバルイベントでマップのドラッグを確実に無効化
                        const globalMouseMove = (e: MouseEvent) => {
                            if (isDragging) {
                                disableMapDragging();
                                e.stopPropagation();
                            }
                        };
                        const globalMouseUp = () => {
                            isDragging = false;
                            enableMapDragging();
                        };

                        // グローバルイベントリスナーを追加
                        document.addEventListener('mousemove', globalMouseMove, true);
                        document.addEventListener('mouseup', globalMouseUp, true);

                        // クリーンアップ関数を保存（マーカーが削除される際に呼び出す）
                        (slider1Marker as any)._cleanupEvents = () => {
                            document.removeEventListener('mousemove', globalMouseMove, true);
                            document.removeEventListener('mouseup', globalMouseUp, true);
                            sliderElement.removeEventListener('mousedown', handleMouseDown, true);
                            sliderElement.removeEventListener('mousemove', handleMouseMove, true);
                            sliderElement.removeEventListener('mouseup', handleMouseUp, true);
                            sliderElement.removeEventListener('mouseleave', handleMouseLeave, true);
                        };

                        sliderElement.addEventListener('input', (e) => {
                            e.stopPropagation();
                            const value = parseInt((e.target as HTMLInputElement).value);
                            slider194_195ValuesRef.current.set(slider1Key, value);
                            handleSlider194_195Change(edgeId, value, 1);
                        });

                        sliderElement.addEventListener('change', (e) => {
                            e.stopPropagation();
                            const value = parseInt((e.target as HTMLInputElement).value);
                            slider194_195ValuesRef.current.set(slider1Key, value);
                            handleSlider194_195Change(edgeId, value, 1);
                        });
                    }, 100);
                });

                // 2つ目のスライダーのイベントリスナー（赤）
                slider2Marker.on('add', () => {
                    setTimeout(() => {
                        const sliderElement = document.getElementById(
                            slider2Id
                        ) as HTMLInputElement;
                        if (!sliderElement) return;

                        let isDragging = false;

                        const disableMapDragging = () => {
                            if (map.dragging) {
                                map.dragging.disable();
                            }
                        };

                        const enableMapDragging = () => {
                            if (map.dragging && !isDragging) {
                                map.dragging.enable();
                            }
                        };

                        const handleMouseDown = (e: MouseEvent) => {
                            isDragging = true;
                            disableMapDragging();
                            // スライダーのハンドルを表示
                            sliderElement.classList.add('thumb-visible');
                            // クリック時に現在の値で経路を表示
                            const currentValue = parseInt(sliderElement.value);
                            handleSlider194_195Change(edgeId, currentValue, 2);
                            e.stopPropagation();
                            e.stopImmediatePropagation();
                        };

                        const handleMouseMove = (e: MouseEvent) => {
                            if (isDragging) {
                                disableMapDragging();
                                e.stopPropagation();
                                e.stopImmediatePropagation();
                            }
                        };

                        const handleMouseUp = () => {
                            isDragging = false;
                            enableMapDragging();
                        };

                        const handleMouseLeave = () => {
                            isDragging = false;
                            enableMapDragging();
                        };

                        // 既存のイベントリスナーを削除してから追加（重複防止）
                        sliderElement.removeEventListener('mousedown', handleMouseDown);
                        sliderElement.removeEventListener('mousemove', handleMouseMove);
                        sliderElement.removeEventListener('mouseup', handleMouseUp);
                        sliderElement.removeEventListener('mouseleave', handleMouseLeave);

                        sliderElement.addEventListener('mousedown', handleMouseDown, true);
                        sliderElement.addEventListener('mousemove', handleMouseMove, true);
                        sliderElement.addEventListener('mouseup', handleMouseUp, true);
                        sliderElement.addEventListener('mouseleave', handleMouseLeave, true);

                        // グローバルイベントでマップのドラッグを確実に無効化
                        const globalMouseMove = (e: MouseEvent) => {
                            if (isDragging) {
                                disableMapDragging();
                                e.stopPropagation();
                            }
                        };
                        const globalMouseUp = () => {
                            isDragging = false;
                            enableMapDragging();
                        };

                        // グローバルイベントリスナーを追加
                        document.addEventListener('mousemove', globalMouseMove, true);
                        document.addEventListener('mouseup', globalMouseUp, true);

                        // クリーンアップ関数を保存（マーカーが削除される際に呼び出す）
                        (slider2Marker as any)._cleanupEvents = () => {
                            document.removeEventListener('mousemove', globalMouseMove, true);
                            document.removeEventListener('mouseup', globalMouseUp, true);
                            sliderElement.removeEventListener('mousedown', handleMouseDown, true);
                            sliderElement.removeEventListener('mousemove', handleMouseMove, true);
                            sliderElement.removeEventListener('mouseup', handleMouseUp, true);
                            sliderElement.removeEventListener('mouseleave', handleMouseLeave, true);
                        };

                        sliderElement.addEventListener('input', (e) => {
                            e.stopPropagation();
                            const value = parseInt((e.target as HTMLInputElement).value);
                            slider194_195ValuesRef.current.set(slider2Key, value);
                            handleSlider194_195Change(edgeId, value, 2);
                        });

                        sliderElement.addEventListener('change', (e) => {
                            e.stopPropagation();
                            const value = parseInt((e.target as HTMLInputElement).value);
                            slider194_195ValuesRef.current.set(slider2Key, value);
                            handleSlider194_195Change(edgeId, value, 2);
                        });
                    }, 100);
                });

                allBlueMarkers.push(slider1Marker);
                allRedMarkers.push(slider2Marker);
                slider194_195MarkersRef.current.push(slider1Marker, slider2Marker);
            }

            // 現在の色状態を保持（再描画時も状態を維持）
            const currentType = slider194_195TypeRef.current || 'blue';
            slider194_195VisibleRef.current = true;
            slider194_195TypeRef.current = currentType;
            setSlider194_195Type(currentType);

            // 現在の状態に応じてスライダーを表示
            if (currentType === 'red') {
                allRedMarkers.forEach((m) => m.addTo(map));
            } else {
                allBlueMarkers.forEach((m) => m.addTo(map));
            }

            // 初期値の経路は表示しない（スライダーを動かしてから表示）

            // 左上に切り替えボタンを追加
            const toggleButtonId = '194-195-toggle-button';
            const toggleButtonContainer = document.createElement('div');
            toggleButtonContainer.id = toggleButtonId;
            toggleButtonContainer.style.cssText = `
                position: absolute;
                top: 10px;
                left: 50px;
                z-index: 1000;
                pointer-events: auto;
            `;

            const toggleButton = document.createElement('button');
            // 現在の状態に応じてボタンのテキストと色を設定
            toggleButton.textContent = currentType === 'blue' ? '赤色に切り替え' : '青色に切り替え';
            toggleButton.style.cssText = `
                background: ${currentType === 'blue' ? '#ef4444' : '#3b82f6'};
                color: white;
                border: none;
                border-radius: 4px;
                padding: 8px 16px;
                font-size: 12px;
                cursor: pointer;
                box-shadow: 0 2px 4px rgba(0,0,0,0.3);
                font-weight: bold;
            `;
            toggleButton.onmouseover = () => {
                toggleButton.style.background = '#dc2626';
            };
            toggleButton.onmouseout = () => {
                toggleButton.style.background =
                    slider194_195TypeRef.current === 'blue' ? '#ef4444' : '#3b82f6';
            };

            // ボタンのクリックイベント（全スライダーを青⇔赤で一括切り替え）
            toggleButton.addEventListener('click', (e) => {
                e.stopPropagation();
                e.preventDefault();

                if (slider194_195TypeRef.current === 'blue') {
                    // 青色から赤色に切り替え（全経路）
                    slider194_195MarkersRef.current.forEach((marker: any) => {
                        try {
                            if (
                                map.hasLayer(marker) &&
                                (marker as any).options.icon === undefined
                            ) {
                                map.removeLayer(marker);
                            }
                        } catch {
                            /* ignore */
                        }
                    });
                    allBlueMarkers.forEach((marker) => {
                        try {
                            if (map.hasLayer(marker)) {
                                map.removeLayer(marker);
                            }
                        } catch {
                            /* ignore */
                        }
                    });
                    allRedMarkers.forEach((marker) => {
                        try {
                            if (!map.hasLayer(marker)) {
                                marker.addTo(map);
                            }
                        } catch {
                            /* ignore */
                        }
                    });
                    slider194_195TypeRef.current = 'red';
                    setSlider194_195Type('red');
                    toggleButton.textContent = '青色に切り替え';
                    toggleButton.style.background = '#3b82f6';

                    // 経路は表示しない（スライダーを動かしてから表示）
                    // 既存の 194-195 の経路を削除
                    if (slider194_195RouteLayerRef.current) {
                        try {
                            const routeLayer = slider194_195RouteLayerRef.current;
                            if (map.hasLayer(routeLayer)) {
                                map.removeLayer(routeLayer);
                            }
                        } catch (e) {
                            console.warn('経路レイヤーの削除でエラー:', e);
                        }
                        slider194_195RouteLayerRef.current = null;
                    }
                } else {
                    // 赤色から青色に切り替え（全経路）
                    allRedMarkers.forEach((marker) => {
                        try {
                            if (map.hasLayer(marker)) {
                                map.removeLayer(marker);
                            }
                        } catch {
                            /* ignore */
                        }
                    });
                    allBlueMarkers.forEach((marker) => {
                        try {
                            if (!map.hasLayer(marker)) {
                                marker.addTo(map);
                            }
                        } catch {
                            /* ignore */
                        }
                    });
                    slider194_195TypeRef.current = 'blue';
                    setSlider194_195Type('blue');
                    toggleButton.textContent = '赤色に切り替え';
                    toggleButton.style.background = '#ef4444';

                    // 経路は表示しない（スライダーを動かしてから表示）
                    // 既存の 194-195 の経路を削除
                    if (slider194_195RouteLayerRef.current) {
                        try {
                            const routeLayer = slider194_195RouteLayerRef.current;
                            if (map.hasLayer(routeLayer)) {
                                map.removeLayer(routeLayer);
                            }
                        } catch (e) {
                            console.warn('経路レイヤーの削除でエラー:', e);
                        }
                        slider194_195RouteLayerRef.current = null;
                    }
                }
            });

            toggleButtonContainer.appendChild(toggleButton);

            // マップコンテナにボタンを追加
            const mapContainer = map.getContainer();
            if (mapContainer) {
                // 既存のボタンを削除
                const existingButton = document.getElementById(toggleButtonId);
                if (existingButton) {
                    existingButton.remove();
                }
                mapContainer.appendChild(toggleButtonContainer);
            }
        } catch (error) {
            console.error('194-195のスライダー表示エラー:', error);
            slider194_195VisibleRef.current = false;
        } finally {
            slider194_195CreatingRef.current = false;
            isRedrawingRef.current = false;
        }
    }, []);

    // 194-195 のスライダー変更時の処理
    // 他の経路（22-25 など）は、現状は UI のみで紫の経路は描画しない
    const handleSlider194_195Change = async (
        edgeId: string,
        distance: number,
        sliderType: number
    ) => {
        const map = mapRef.current;
        if (!map) {
            console.warn('マップが初期化されていません');
            return;
        }

        console.log(
            `[スライダー変更] edgeId: ${edgeId}, distance: ${distance}, sliderType: ${sliderType}`
        );

        // 194-195 以外は、現在は経路描画なし（将来の拡張用）
        if (edgeId !== '194-195') {
            return;
        }

        // スライダー値からファイル番号を計算（0→48、1→47、...、47→1）
        const fileNumber = 48 - Math.floor(distance);
        const sliderTypeName = sliderType === 1 ? 'blue' : 'red';
        const directory = sliderType === 1 ? '194-195_green' : '194-195_red';
        const fileName = `${fileNumber}.txt`;

        console.log(
            `[ファイル読み込み] ファイル番号: ${fileNumber}, ディレクトリ: ${directory}, ファイル名: ${fileName}`
        );

        try {
            // 既存の194-195経路レイヤーを確実に削除
            if (slider194_195RouteLayerRef.current) {
                try {
                    const oldLayer = slider194_195RouteLayerRef.current;
                    if (map.hasLayer(oldLayer)) {
                        map.removeLayer(oldLayer);
                        console.log('[既存レイヤー削除] レイヤーを削除しました');
                    }
                } catch (e) {
                    console.warn('既存レイヤーの削除でエラー:', e);
                }
                slider194_195RouteLayerRef.current = null;
            }

            // テキストファイルを読み込む
            const textFilePath = `/api/main_server_route/static/${directory}/${fileName}`;
            console.log(`[テキストファイル読み込み] ${textFilePath}`);
            const textResponse = await fetch(textFilePath);
            if (!textResponse.ok) {
                let errorData: any = {};
                try {
                    errorData = await textResponse.json();
                } catch {
                    errorData = { error: await textResponse.text().catch(() => '') };
                }
                console.error(
                    `ファイルが見つかりません: ${textFilePath}, status: ${textResponse.status}`,
                    errorData
                );
                return;
            }

            const textContent = await textResponse.text();
            console.log(`[テキスト内容] ${textContent.substring(0, 100)}...`);
            const geojsonFileNames = textContent
                .split('\n')
                .map((line) => line.trim())
                .filter((line) => line !== '' && line.endsWith('.geojson'));

            console.log(`[GeoJSONファイル数] ${geojsonFileNames.length}個`);

            // Leafletを動的にインポート
            const L = await import('leaflet');

            // 全てのGeoJSONファイルを読み込んで結合
            const geojsonFeatures: any[] = [];
            const geojsonFolder = 'oomiya_line/';

            for (const geojsonFileName of geojsonFileNames) {
                try {
                    const geojsonPath = `/api/main_server_route/static/${geojsonFolder}${geojsonFileName}`;
                    console.log(`[GeoJSON読み込み] ${geojsonPath}`);
                    const geojsonResponse = await fetch(geojsonPath);
                    if (!geojsonResponse.ok) {
                        const errorData = await geojsonResponse
                            .json()
                            .catch(() => ({ error: 'Unknown error' }));
                        console.warn(
                            `GeoJSONファイルが見つかりません: ${geojsonPath}, status: ${geojsonResponse.status}`,
                            errorData
                        );
                        continue;
                    }
                    const geojsonData = await geojsonResponse.json();
                    if (geojsonData.type === 'Feature') {
                        geojsonFeatures.push(geojsonData);
                        console.log(`[GeoJSON追加] ${geojsonFileName} (Feature)`);
                    } else if (geojsonData.type === 'FeatureCollection') {
                        geojsonFeatures.push(...geojsonData.features);
                        console.log(
                            `[GeoJSON追加] ${geojsonFileName} (FeatureCollection, ${geojsonData.features.length} features)`
                        );
                    }
                } catch (err) {
                    console.warn(`Error loading ${geojsonFileName}:`, err);
                }
            }

            console.log(`[読み込んだFeature数] ${geojsonFeatures.length}個`);

            if (geojsonFeatures.length > 0) {
                // 念のため、既存のレイヤーが残っていないか再度確認して削除
                if (slider194_195RouteLayerRef.current) {
                    try {
                        const oldLayer = slider194_195RouteLayerRef.current;
                        if (map.hasLayer(oldLayer)) {
                            map.removeLayer(oldLayer);
                            console.log('[既存レイヤー削除（再確認）] レイヤーを削除しました');
                        }
                    } catch (e) {
                        console.warn('既存レイヤーの削除（再確認）でエラー:', e);
                    }
                    slider194_195RouteLayerRef.current = null;
                }

                // GeoJSONレイヤーを作成（紫色で表示）
                const routeLayer = L.default.geoJSON(geojsonFeatures, {
                    style: {
                        color: '#9333ea', // 紫色
                        weight: 8,
                        opacity: 0.8,
                    },
                });

                routeLayer.addTo(map);
                slider194_195RouteLayerRef.current = routeLayer;
                console.log('[経路レイヤー追加完了]');
            } else {
                console.warn('[警告] 読み込んだFeatureが0個です');
            }
        } catch (error: any) {
            console.error('経路読み込みエラー:', error);
            console.error('エラー詳細:', error.message, error.stack);
        }
    };

    // ズーム変更時の処理はMapUpdaterコンポーネント内で行う

    // 初期化
    useEffect(() => {
        loadCSVData();
        loadSavedRoutesList();

        // ノードGeoJSONをロード
        const pointPromises = [];
        for (let i = 1; i <= 300; i++) {
            const fileName = `${i}.geojson`;
            const filePath = `/api/main_server_route/static/oomiya_point/${fileName}`;
            pointPromises.push(
                fetch(filePath)
                    .then((response) => {
                        if (response.ok) {
                            return response.json().then((geojsonData) => {
                                geojsonFeaturesRef.current.push(geojsonData);
                                geojsonFileNamesRef.current.push(fileName);
                            });
                        }
                    })
                    .catch(() => {
                        /* 存在しないファイルは無視 */
                    })
            );
        }
        Promise.all(pointPromises).then(() => {
            console.log(`${geojsonFeaturesRef.current.length} points loaded.`);
        });
    }, []);

    return (
        <div className="gradient-bg min-h-screen">
            <div
                className={`min-h-screen p-4 ${isLoading ? 'pointer-events-none opacity-50' : ''}`}
            >
                <div className="max-w-8xl mx-auto space-y-6">
                    {/* 表示データと経路情報 */}
                    <div className="grid grid-cols-2 gap-4">
                        <div className="glass-effect rounded-xl p-6">
                            <h3 className="text-white font-semibold text-lg mb-4 flex items-center gap-2">
                                <i className="fas fa-eye text-yellow-300"></i>
                                表示データ
                            </h3>
                            <div className="grid grid-cols-6 gap-2 auto-rows-fr">
                                <button
                                    onClick={viewAREA}
                                    disabled={isLoading}
                                    className="bg-gray-600 hover:bg-gray-500 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    対象範囲
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('sidewalk', 'red')}
                                    disabled={isLoading}
                                    className="bg-red-600 hover:bg-red-500 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    歩道
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('signal', 'blue')}
                                    disabled={isLoading}
                                    className="bg-blue-600 hover:bg-blue-500 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    信号
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('road_width1', 'purple')}
                                    disabled={isLoading}
                                    className="bg-purple-600 hover:bg-purple-500 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    道路幅1
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('road_width2', 'brown')}
                                    disabled={isLoading}
                                    className="bg-yellow-700 hover:bg-yellow-600 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    道路幅2
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('road_width3', 'darkgreen')}
                                    disabled={isLoading}
                                    className="bg-green-700 hover:bg-green-600 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    道路幅3
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('illumination', 'orange')}
                                    disabled={isLoading}
                                    className="bg-orange-600 hover:bg-orange-500 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    照明
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('nature', 'green')}
                                    disabled={isLoading}
                                    className="bg-green-600 hover:bg-green-500 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    自然
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('park', 'red')}
                                    disabled={isLoading}
                                    className="bg-red-700 hover:bg-red-600 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    公園
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('garbage', 'orange')}
                                    disabled={isLoading}
                                    className="bg-orange-500 hover:bg-orange-400 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    ごみ
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('toilet', '#ff1493')}
                                    disabled={isLoading}
                                    className="bg-pink-600 hover:bg-pink-500 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    トイレ
                                </button>
                                <button
                                    onClick={() => displayDataFromCSV('crosswalk', 'red')}
                                    disabled={isLoading}
                                    className="bg-indigo-600 hover:bg-indigo-500 text-white text-xs py-2 px-2 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    横断歩道
                                </button>
                            </div>
                        </div>
                        <div className="col-span-1 flex flex-col">
                            <div className="glass-effect rounded-xl p-4 flex-grow">
                                <h3 className="text-white font-semibold mb-3 flex items-center gap-2">
                                    <i className="fas fa-info-circle text-yellow-300"></i>
                                    経路情報
                                </h3>
                                <RouteInfo
                                    route1={route1}
                                    route2={route2}
                                    savedRoute={savedRoute}
                                    bestEnumRoute={bestEnumRoute}
                                />
                            </div>
                        </div>
                    </div>

                    {/* メインコンテンツ */}
                    <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
                        {/* 左側パネル */}
                        <div className="lg:col-span-3 flex flex-col gap-6">
                            {/* 地点設定 */}
                            <div className="glass-effect rounded-xl p-6">
                                <h3 className="text-white font-semibold text-lg mb-4 flex items-center gap-2">
                                    <i className="fas fa-map-marker-alt text-yellow-300"></i>
                                    地点設定
                                </h3>
                                <div className="space-y-3">
                                    <button
                                        onClick={handleShowPins}
                                        disabled={isLoading}
                                        className={`w-full py-2 px-4 rounded-lg transition-all duration-200 hover:scale-105 flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed ${
                                            showIntersectionPins
                                                ? 'bg-red-500 hover:bg-red-600 text-white'
                                                : 'bg-blue-500 hover:bg-blue-600 text-white'
                                        }`}
                                    >
                                        <i className="fas fa-map-pin"></i>
                                        {showIntersectionPins ? 'ピンを非表示' : 'ピンを配置する'}
                                    </button>
                                    {showIntersectionPins && (
                                        <div className="text-white/80 text-xs text-center">
                                            {pinSelectionState === 'start' &&
                                                '始点を選択してください'}
                                            {pinSelectionState === 'end' &&
                                                '終点を選択してください'}
                                        </div>
                                    )}
                                    <div className="space-x-2 flex">
                                        <div>
                                            <label className="block text-white/90 text-sm font-medium mb-2">
                                                <i className="fas fa-play text-green-400 mr-1"></i>
                                                始点 (1-246)
                                            </label>
                                            <input
                                                type="text"
                                                value={params.param1}
                                                onChange={(e) =>
                                                    setParams({ ...params, param1: e.target.value })
                                                }
                                                disabled={isLoading}
                                                className="w-full pl-2 rounded-lg bg-white/20 text-white border border-white/30 placeholder-white/50 focus:border-yellow-300 focus:outline-none transition-all duration-200 disabled:opacity-50 disabled:cursor-not-allowed"
                                                placeholder="始点ノード番号"
                                            />
                                        </div>
                                        <div>
                                            <label className="block text-white/90 text-sm font-medium mb-2">
                                                <i className="fas fa-stop text-red-400 mr-1"></i>{' '}
                                                終点 (1-246)
                                            </label>
                                            <input
                                                type="text"
                                                value={params.param2}
                                                onChange={(e) =>
                                                    setParams({ ...params, param2: e.target.value })
                                                }
                                                disabled={isLoading}
                                                className="w-full pl-2 rounded-lg bg-white/20 text-white border border-white/30 placeholder-white/50 focus:border-yellow-300 focus:outline-none transition-all duration-200 disabled:opacity-50 disabled:cursor-not-allowed"
                                                placeholder="終点ノード番号"
                                            />
                                        </div>
                                    </div>
                                </div>
                            </div>

                            {/* 歩行速度設定 */}
                            <div className="glass-effect rounded-xl p-6">
                                <h3 className="text-white font-semibold text-lg mb-4 flex items-center gap-2">
                                    <i className="fas fa-walking text-yellow-300"></i>
                                    歩行速度設定
                                </h3>
                                <div>
                                    <label className="block text-white/90 text-sm font-medium mb-2">
                                        速度 (m/分)
                                    </label>
                                    <input
                                        type="number"
                                        value={params.walkingSpeed}
                                        onChange={(e) =>
                                            setParams({ ...params, walkingSpeed: e.target.value })
                                        }
                                        disabled={isLoading}
                                        className="w-full pl-2 rounded-lg bg-white/20 text-white border border-white/30 placeholder-white/50 focus:border-yellow-300 focus:outline-none transition-all duration-200 disabled:opacity-50 disabled:cursor-not-allowed"
                                        placeholder="例: 80"
                                    />
                                </div>
                            </div>

                            {/* ルート保存・参照 */}
                            <div className="glass-effect rounded-xl p-4">
                                <h3 className="text-white font-semibold mb-3 flex items-center gap-2">
                                    <i className="fas fa-save text-yellow-300"></i>
                                    ルート保存・参照
                                </h3>
                                <div className="space-y-3">
                                    <div className="grid grid-cols-2 gap-2">
                                        <button
                                            onClick={() => {}}
                                            disabled={isLoading}
                                            className="bg-green-500 hover:bg-green-600 text-white text-xs py-2 px-3 rounded-lg transition-all duration-200 hover:scale-105 flex items-center justify-center gap-1 disabled:opacity-50 disabled:cursor-not-allowed"
                                        >
                                            <i className="fas fa-download"></i>経路1
                                        </button>
                                        <button
                                            onClick={() => {}}
                                            disabled={isLoading}
                                            className="bg-green-500 hover:bg-green-600 text-white text-xs py-2 px-3 rounded-lg transition-all duration-200 hover:scale-105 flex items-center justify-center gap-1 disabled:opacity-50 disabled:cursor-not-allowed"
                                        >
                                            <i className="fas fa-download"></i>経路2
                                        </button>
                                    </div>
                                    <div className="flex gap-2">
                                        <select
                                            value={selectedSavedRoute}
                                            onChange={(e) => setSelectedSavedRoute(e.target.value)}
                                            disabled={isLoading}
                                            className="flex-1 text-xs p-2 rounded-lg bg-white/20 text-white border border-white/30 focus:border-yellow-300 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed"
                                        >
                                            <option>読み込み中...</option>
                                            {savedRoutesList.map((file) => (
                                                <option key={file} value={file}>
                                                    {file}
                                                </option>
                                            ))}
                                        </select>
                                        <button
                                            onClick={() => {}}
                                            disabled={isLoading}
                                            className="bg-blue-500 hover:bg-blue-600 text-white text-xs py-2 px-3 rounded-lg transition-all duration-200 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
                                        >
                                            <i className="fas fa-upload"></i>
                                        </button>
                                    </div>
                                </div>
                            </div>

                            {/* 勾配制限 */}
                            <div className="glass-effect rounded-xl p-3">
                                <h3 className="text-white font-semibold text-lg mb-1 flex items-center gap-2">
                                    <i className="fas fa-mountain text-yellow-300"></i>
                                    勾配制限
                                </h3>
                                <div className="space-y-1">
                                    <div>
                                        <label className="block text-white/90 text-sm font-medium">
                                            上りの最大勾配 (m/m)
                                        </label>
                                        <div className="flex items-center gap-3">
                                            <input
                                                type="range"
                                                value={weights.weight2}
                                                min="0"
                                                max="100"
                                                step="0.1"
                                                onChange={(e) =>
                                                    setWeights({
                                                        ...weights,
                                                        weight2: parseFloat(e.target.value),
                                                    })
                                                }
                                                disabled={isLoading}
                                                className="range-slider flex-1 h-2 bg-white/20 rounded-full appearance-none cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
                                            />
                                            <input
                                                type="text"
                                                value={weights.weight2}
                                                onChange={(e) =>
                                                    setWeights({
                                                        ...weights,
                                                        weight2: parseFloat(e.target.value) || 0,
                                                    })
                                                }
                                                disabled={isLoading}
                                                className="w-16 px-2 py-1 text-xs text-center rounded bg-white/20 text-white border border-white/30 focus:border-yellow-300 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed"
                                            />
                                        </div>
                                    </div>
                                    <div>
                                        <label className="block text-white/90 text-sm font-medium">
                                            下りの最大勾配 (m/m)
                                        </label>
                                        <div className="flex items-center gap-3">
                                            <input
                                                type="range"
                                                value={weights.weight3}
                                                min="-100"
                                                max="0"
                                                step="0.1"
                                                onChange={(e) =>
                                                    setWeights({
                                                        ...weights,
                                                        weight3: parseFloat(e.target.value),
                                                    })
                                                }
                                                disabled={isLoading}
                                                className="range-slider flex-1 h-2 bg-white/20 rounded-full appearance-none cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
                                            />
                                            <input
                                                type="text"
                                                value={weights.weight3}
                                                onChange={(e) =>
                                                    setWeights({
                                                        ...weights,
                                                        weight3: parseFloat(e.target.value) || 0,
                                                    })
                                                }
                                                disabled={isLoading}
                                                className="w-16 px-2 py-1 text-xs text-center rounded bg-white/20 text-white border border-white/30 focus:border-yellow-300 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed"
                                            />
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>

                        {/* 中央：マップ */}
                        <div className="lg:col-span-6">
                            <MapComponent
                                mapRef={mapRef}
                                routeLayers={routeLayers}
                                dataLayers={dataLayers}
                                startMarker={startMarker}
                                endMarker={endMarker}
                                onMapClick={handleMapClick}
                                onRouteClick={setSelectedRouteInfo}
                                weights={weights}
                                onWeightChange={(weightId, value) => {
                                    const weightKey = weightId as keyof typeof weights;
                                    setWeights({ ...weights, [weightKey]: value });
                                }}
                                intersectionPins={showIntersectionPins ? intersectionPins : []}
                                onIntersectionPinClick={handleIntersectionPinClick}
                                pinSelectionState={pinSelectionState}
                                onSlider194_195Change={handleSlider194_195Change}
                                onShowSlider194_195={showSliderOn194_195}
                                slider194_195VisibleRef={slider194_195VisibleRef}
                                slider194_195CreatingRef={slider194_195CreatingRef}
                            />
                            <div className="glass-effect rounded-xl p-4 flex flex-col">
                                <h3 className="text-white font-semibold mb-3 flex items-center gap-2">
                                    <i className="fas fa-play-circle text-yellow-300"></i>
                                    計算実行
                                </h3>
                                <div className="grid grid-cols-3 gap-2">
                                    <button
                                        onClick={loadRouteFromCSV}
                                        disabled={isLoading}
                                        className="w-full bg-red-500 hover:bg-red-600 text-white text-sm py-2 rounded-lg transition-all duration-200 hover:scale-105 flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed"
                                    >
                                        <i className="fas fa-route"></i>経路を検索
                                    </button>
                                    <button
                                        onClick={loadRouteFromCSV2}
                                        disabled={isLoading}
                                        className="w-full bg-yellow-500 hover:bg-yellow-600 text-white text-sm py-2 rounded-lg transition-all duration-200 hover:scale-105 flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed"
                                    >
                                        <i className="fas fa-route"></i>全経路を表示
                                    </button>
                                    <button
                                        onClick={clearGeoJSON}
                                        disabled={isLoading}
                                        className="w-full bg-gray-500 hover:bg-gray-600 text-white text-sm py-2 rounded-lg transition-all duration-200 hover:scale-105 flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed"
                                    >
                                        <i className="fas fa-trash"></i>クリア
                                    </button>
                                </div>
                            </div>
                        </div>

                        {/* 右側：重み設定 */}
                        <div className="glass-effect rounded-xl p-4 lg:col-span-3">
                            <h3 className="text-white font-semibold text-xl mb-4 flex items-center gap-2">
                                <i className="fas fa-sliders-h text-yellow-300"></i>
                                重み設定
                            </h3>
                            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-1 xl:grid-cols-2 gap-2">
                                <WeightSlider
                                    id="weight0"
                                    label="距離 (+)"
                                    value={weights.weight0}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) => setWeights({ ...weights, weight0: value })}
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight1"
                                    label="勾配 (+)"
                                    value={weights.weight1}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) => setWeights({ ...weights, weight1: value })}
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight4"
                                    label="歩道 (-)"
                                    value={weights.weight4}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) => setWeights({ ...weights, weight4: value })}
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight5"
                                    label="信号 (+)"
                                    value={weights.weight5}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) => setWeights({ ...weights, weight5: value })}
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight6"
                                    label="道路幅 (+)"
                                    value={weights.weight6}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) => setWeights({ ...weights, weight6: value })}
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight7"
                                    label="照明 (-)"
                                    value={weights.weight7}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) => setWeights({ ...weights, weight7: value })}
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight8"
                                    label="自然 (-)"
                                    value={weights.weight8}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) => setWeights({ ...weights, weight8: value })}
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight9"
                                    label="公園 (-)"
                                    value={weights.weight9}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) => setWeights({ ...weights, weight9: value })}
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight10"
                                    label="ごみ集積場 (+)"
                                    value={weights.weight10}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) =>
                                        setWeights({ ...weights, weight10: value })
                                    }
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight11"
                                    label="公衆トイレ (-)"
                                    value={weights.weight11}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) =>
                                        setWeights({ ...weights, weight11: value })
                                    }
                                    disabled={isLoading}
                                />
                                <WeightSlider
                                    id="weight12"
                                    label="横断歩道 (-)"
                                    value={weights.weight12}
                                    min={-100}
                                    max={100}
                                    step={0.1}
                                    onChange={(value) =>
                                        setWeights({ ...weights, weight12: value })
                                    }
                                    disabled={isLoading}
                                />
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}
