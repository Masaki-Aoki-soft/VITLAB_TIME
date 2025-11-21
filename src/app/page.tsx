'use client';

// SSRを無効化（windowオブジェクトを使用するため）
export const dynamic = 'force-dynamic';

import { useState, useEffect, useRef } from 'react';
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
    const [routeLayers, setRouteLayers] = useState<Array<{ data: any; style: any }>>([]);
    const [dataLayers, setDataLayers] = useState<
        Array<{ data: any; style: any; popup?: string; edgeName?: string }>
    >([]);
    const [startMarker, setStartMarker] = useState<{
        position: [number, number];
        nodeId: string;
        startNode: string;
        endNode: string;
    } | null>(null);
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

        const redColor = '#ff4757';
        const geojsonFolder = 'oomiya_line/';

        // 重複する経路を除外
        const uniqueRoutes: RouteResult[] = [];
        const seenRoutes = new Set<string>();
        for (const route of routes) {
            const routeKey = route.userPref.trim();
            if (!seenRoutes.has(routeKey)) {
                seenRoutes.add(routeKey);
                uniqueRoutes.push(route);
            }
        }

        if (uniqueRoutes.length > 0) {
            setRoute1({
                totalDistance: uniqueRoutes[0].totalDistance,
                totalTime: uniqueRoutes[0].totalTime,
                totalWaitTime: uniqueRoutes[0].totalWaitTime,
            });
        }

        // 経路を描画（react-leaflet用にstateに追加）
        const newLayers: Array<{ data: any; style: any }> = [];
        for (const route of uniqueRoutes) {
            const segments = route.userPref.split('\n').filter((line) => line.trim() !== '');

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
                            weight: 5,
                            opacity: 0.6,
                        },
                    });
                } catch (err) {
                    console.warn(`Error loading ${filename}:`, err);
                }
            }
        }
        setRouteLayers(newLayers);
    };

    // 次の経路を表示
    const loadRouteFromCSV2 = async () => {
        if (!foundRoutes || foundRoutes.length === 0) {
            toast.error('まず「経路を検索」してください。');
            return;
        }

        const nextIndex = (currentRouteIndex + 1) % foundRoutes.length;
        setCurrentRouteIndex(nextIndex);
        setIsLoading(true);

        toast.promise(
            new Promise<string>(async (resolve, reject) => {
                try {
                    const route = foundRoutes[nextIndex];
                    const routeId = (nextIndex % 2) + 1;
                    const color = routeId === 1 ? '#ff4757' : '#3742fa';
                    const weight = routeId === 1 ? 10 : 6;

                    const segments = route.userPref
                        .split('\n')
                        .filter((line) => line.trim() !== '');
                    const geojsonFolder = 'oomiya_line/';

                    const newLayers: Array<{ data: any; style: any }> = [];
                    for (const filename of segments) {
                        try {
                            const filePath = `/api/main_server_route/static/${geojsonFolder}${filename.trim()}`;
                            const response = await fetch(filePath);
                            if (!response.ok) continue;
                            const data = await response.json();
                            newLayers.push({
                                data,
                                style: {
                                    color: color,
                                    weight: weight,
                                    opacity: 1.0,
                                },
                            });
                        } catch (err) {
                            console.warn(`Error loading ${filename}:`, err);
                        }
                    }
                    setRouteLayers(newLayers);

                    if (routeId === 1) {
                        setRoute1({
                            totalDistance: route.totalDistance,
                            totalTime: route.totalTime,
                            totalWaitTime: route.totalWaitTime,
                        });
                    } else {
                        setRoute2({
                            totalDistance: route.totalDistance,
                            totalTime: route.totalTime,
                            totalWaitTime: route.totalWaitTime,
                        });
                    }
                    resolve(`経路 ${nextIndex + 1} を表示しました`);
                } catch (error: any) {
                    console.error('次の経路の表示中にエラーが発生しました:', error);
                    reject(error.message || 'エラーが発生しました');
                } finally {
                    setIsLoading(false);
                }
            }),
            {
                loading: `経路 ${nextIndex + 1} を表示中...`,
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
        otherLayersRef.current.forEach((layer) => {
            if (mapRef.current) {
                mapRef.current.removeLayer(layer);
            }
        });
        otherLayersRef.current = [];
        setRoute1(null);
        setRoute2(null);
        setSavedRoute(null);
        setFoundRoutes([]);
        setCurrentRouteIndex(0);
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

    // マップクリック時の処理
    const handleMapClick = async (lat: number, lng: number) => {
        if (geojsonFeaturesRef.current.length === 0) {
            alert('GeoJSONデータがまだ読み込まれていません。');
            return;
        }

        // @turf/turfを動的にインポート
        const turfModule = await import('@turf/turf');
        const turf = turfModule;

        const clickedPoint = turf.point([lng, lat]);
        let nearestPoint: any = null;
        let nearestFileIndex: number = -1;
        let minDistance = Infinity;

        geojsonFeaturesRef.current.forEach((feature, index) => {
            const currentPoint = turf.point(feature.geometry.coordinates);
            const distance = turf.distance(clickedPoint, currentPoint, {
                units: 'kilometers',
            });
            if (distance < minDistance) {
                minDistance = distance;
                nearestPoint = feature;
                nearestFileIndex = index;
            }
        });

        if (
            nearestPoint &&
            nearestFileIndex >= 0 &&
            nearestFileIndex < geojsonFileNamesRef.current.length
        ) {
            const fileName = geojsonFileNamesRef.current[nearestFileIndex];
            if (fileName) {
                const nodeId = fileName.replace('.geojson', '');
                const newParams = { ...params, param1: nodeId };
                setStartMarker({
                    position: [
                        nearestPoint.geometry.coordinates[1],
                        nearestPoint.geometry.coordinates[0],
                    ],
                    nodeId: nodeId,
                    startNode: nodeId,
                    endNode: newParams.param2,
                });
                setParams(newParams);
            }
        }
    };

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
                            <div className="grid grid-cols-6 gap-2">
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
                        <div className="xl:col-span-1 flex flex-col">
                            <div className="glass-effect rounded-xl p-4 flex-grow">
                                <h3 className="text-white font-semibold mb-3 flex items-center gap-2">
                                    <i className="fas fa-info-circle text-yellow-300"></i>
                                    経路情報
                                </h3>
                                <RouteInfo
                                    route1={route1}
                                    route2={route2}
                                    savedRoute={savedRoute}
                                />
                            </div>
                        </div>
                    </div>

                    {/* メインコンテンツ */}
                    <div className="grid grid-cols-1 lg:grid-cols-9 gap-6">
                        {/* 左側パネル */}
                        <div className="xl:col-span-2 flex flex-col gap-6">
                            {/* 地点設定 */}
                            <div className="glass-effect rounded-xl p-6">
                                <h3 className="text-white font-semibold text-lg mb-4 flex items-center gap-2">
                                    <i className="fas fa-map-marker-alt text-yellow-300"></i>
                                    地点設定
                                </h3>
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
                                            <i className="fas fa-stop text-red-400 mr-1"></i> 終点
                                            (1-246)
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
                        <div className="xl:col-span-4">
                            <MapComponent
                                mapRef={mapRef}
                                routeLayers={routeLayers}
                                dataLayers={dataLayers}
                                startMarker={startMarker}
                                onMapClick={handleMapClick}
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
                                        className="w-full bg-blue-500 hover:bg-blue-600 text-white text-sm py-2 rounded-lg transition-all duration-200 hover:scale-105 flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed"
                                    >
                                        <i className="fas fa-route"></i>次の経路を表示
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
                        <div className="glass-effect rounded-xl p-4 col-span-3">
                            <h3 className="text-white font-semibold text-xl mb-4 flex items-center gap-2">
                                <i className="fas fa-sliders-h text-yellow-300"></i>
                                重み設定
                            </h3>
                            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-2 xl:grid-cols-2 gap-2">
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
