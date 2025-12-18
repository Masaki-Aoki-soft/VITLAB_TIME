'use client';

import { useEffect, useRef, useState, useCallback } from 'react';
import {
    MapContainer,
    TileLayer,
    GeoJSON,
    useMap,
    useMapEvents,
    Popup,
    Marker,
} from 'react-leaflet';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import type { GeoJSON as GeoJSONType } from 'geojson';
import type { RouteResult } from '@/lib/types';

// Leafletのデフォルトアイコンの問題を修正
if (typeof window !== 'undefined') {
    delete (L.Icon.Default.prototype as any)._getIconUrl;
    L.Icon.Default.mergeOptions({
        iconRetinaUrl:
            'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon-2x.png',
        iconUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon.png',
        shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-shadow.png',
    });
}

// 青いマーカーアイコンを取得する関数
const getBlueIcon = (): L.Icon | null => {
    if (typeof window === 'undefined') return null;
    return new L.Icon({
        iconUrl:
            'https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-blue.png',
        shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/0.7.7/images/marker-shadow.png',
        iconSize: [25, 41],
        iconAnchor: [12, 41],
        popupAnchor: [1, -34],
        shadowSize: [41, 41],
    });
};

// 赤いマーカーアイコンを取得する関数
const getRedIcon = (): L.Icon | null => {
    if (typeof window === 'undefined') return null;
    return new L.Icon({
        iconUrl:
            'https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-red.png',
        shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/0.7.7/images/marker-shadow.png',
        iconSize: [25, 41],
        iconAnchor: [12, 41],
        popupAnchor: [1, -34],
        shadowSize: [41, 41],
    });
};

interface WeightSettings {
    weight0: number;
    weight1: number;
    weight2: number;
    weight3: number;
    weight4: number;
    weight5: number;
    weight6: number;
    weight7: number;
    weight8: number;
    weight9: number;
    weight10: number;
    weight11: number;
    weight12: number;
}

interface IntersectionPin {
    position: [number, number];
    nodeId: string;
}

interface MapComponentProps {
    mapRef: React.MutableRefObject<L.Map | null>;
    onMapClick?: (lat: number, lng: number) => void;
    routeLayers?: Array<{ data: GeoJSONType; style: L.PathOptions; routeInfo?: RouteResult }>;
    dataLayers?: Array<{
        data: GeoJSONType;
        style: L.PathOptions;
        popup?: string;
        edgeName?: string;
    }>;
    startMarker?: {
        position: [number, number];
        nodeId: string;
        startNode: string;
        endNode: string;
    } | null;
    endMarker?: {
        position: [number, number];
        nodeId: string;
    } | null;
    onRouteClick?: (routeInfo: RouteResult | null) => void;
    weights?: WeightSettings;
    onWeightChange?: (weightId: string, value: number) => void;
    intersectionPins?: IntersectionPin[];
    onIntersectionPinClick?: (nodeId: string, position: [number, number]) => void;
    pinSelectionState?: 'none' | 'start' | 'end';
    // 194-195 専用のスライダー用コールバック（現状は未使用・page.tsx 側で直接処理）
    onSlider194_195Change?: (edgeId: string, distance: number, sliderType: number) => void;
    onShowSlider194_195?: (isRedrawing?: boolean) => void;
    slider194_195VisibleRef?: React.MutableRefObject<boolean>;
    slider194_195CreatingRef?: React.MutableRefObject<boolean>;
}

// マップイベントを処理するコンポーネント
function MapEvents({ onMapClick }: { onMapClick?: (lat: number, lng: number) => void }) {
    useMapEvents({
        click: (e) => {
            if (onMapClick) {
                onMapClick(e.latlng.lat, e.latlng.lng);
            }
        },
    });
    return null;
}

// マップインスタンスを更新するコンポーネント
function MapUpdater({
    mapRef,
    onShowSlider194_195,
    slider194_195VisibleRef,
    slider194_195CreatingRef,
}: {
    mapRef: React.MutableRefObject<L.Map | null>;
    onShowSlider194_195?: (isRedrawing?: boolean) => void;
    slider194_195VisibleRef?: React.MutableRefObject<boolean>;
    slider194_195CreatingRef?: React.MutableRefObject<boolean>;
}) {
    const map = useMap();

    useEffect(() => {
        mapRef.current = map;
        // マップサイズの更新
        setTimeout(() => {
            map.invalidateSize();
        }, 100);
    }, [map, mapRef]);

    // ズーム変更時にスライダーを再描画
    useEffect(() => {
        if (!onShowSlider194_195 || !slider194_195VisibleRef || !slider194_195CreatingRef) return;

        const handleZoomEnd = () => {
            if (slider194_195VisibleRef.current && !slider194_195CreatingRef.current) {
                // ズーム終了後に再描画
                setTimeout(() => {
                    if (slider194_195VisibleRef.current && !slider194_195CreatingRef.current) {
                        onShowSlider194_195(true); // 再描画時なのでtrueを渡す
                    }
                }, 100);
            }
        };

        const handleMoveEnd = () => {
            if (slider194_195VisibleRef.current && !slider194_195CreatingRef.current) {
                // 移動終了後に再描画
                setTimeout(() => {
                    if (slider194_195VisibleRef.current && !slider194_195CreatingRef.current) {
                        onShowSlider194_195(true); // 再描画時なのでtrueを渡す
                    }
                }, 100);
            }
        };

        map.on('zoomend', handleZoomEnd);
        map.on('moveend', handleMoveEnd);

        return () => {
            map.off('zoomend', handleZoomEnd);
            map.off('moveend', handleMoveEnd);
        };
    }, [map, onShowSlider194_195, slider194_195VisibleRef, slider194_195CreatingRef]);

    return null;
}

// 自動的にポップアップを開くマーカーコンポーネント（始点用）
function AutoOpenStartMarker({
    position,
    icon,
    startNode,
    endNode,
}: {
    position: [number, number];
    icon: L.Icon | null;
    startNode: string;
    endNode: string;
}) {
    const markerRef = useRef<L.Marker | null>(null);

    useEffect(() => {
        // マーカーが更新されたときにポップアップを開く
        if (markerRef.current) {
            setTimeout(() => {
                markerRef.current?.openPopup();
            }, 100);
        }
    }, [position, startNode, endNode]);

    return (
        <Marker
            position={position}
            icon={icon || undefined}
            ref={markerRef}
            eventHandlers={{
                add: (e) => {
                    // マーカーが追加されたときにポップアップを開く
                    setTimeout(() => {
                        e.target.openPopup();
                    }, 100);
                },
            }}
        >
            <Popup autoPan={false}>
                <div>
                    <div>
                        <strong>始点:</strong> {startNode}
                    </div>
                    <div>
                        <strong>終点:</strong> {endNode}
                    </div>
                </div>
            </Popup>
        </Marker>
    );
}

// 終点用のマーカーコンポーネント
function EndMarker({
    position,
    icon,
    nodeId,
}: {
    position: [number, number];
    icon: L.Icon | null;
    nodeId: string;
}) {
    return (
        <Marker position={position} icon={icon || undefined}>
            <Popup autoPan={false}>
                <div>
                    <div>
                        <strong>終点:</strong> {nodeId}
                    </div>
                </div>
            </Popup>
        </Marker>
    );
}

export default function MapComponent({
    mapRef,
    onMapClick,
    routeLayers = [],
    dataLayers = [],
    startMarker,
    endMarker,
    onRouteClick,
    weights,
    onWeightChange,
    intersectionPins = [],
    onIntersectionPinClick,
    pinSelectionState = 'none',
    onSlider194_195Change,
    onShowSlider194_195,
    slider194_195VisibleRef,
    slider194_195CreatingRef,
}: MapComponentProps) {
    const [parameterMarkers, setParameterMarkers] = useState<L.Marker[]>([]);
    const parameterMarkersRef = useRef<L.Marker[]>([]);
    const routeParameterValuesRef = useRef<Map<string, number>>(new Map()); // 各REごとの独立したパラメータ値
    const currentRouteInfoRef = useRef<RouteResult | undefined>(undefined);
    const currentLayerRef = useRef<L.GeoJSON | null>(null);

    // Leafletイベントの元DOMイベントを停止（型安全に扱うためのヘルパー）
    const stopLeafletEvent = (evt: L.LeafletEvent) => {
        const origEvent = (evt as any).originalEvent as Event | undefined;
        if (origEvent) {
            L.DomEvent.stop(origEvent);
        }
    };

    useEffect(() => {
        console.log('MapComponent rendered', {
            routeLayers: routeLayers.length,
            dataLayers: dataLayers.length,
            startMarker,
        });
    }, [routeLayers, dataLayers, startMarker]);

    // 経路レイヤーが空になったときにスライダーをクリア
    useEffect(() => {
        const map = mapRef.current;
        if (routeLayers.length === 0 && map) {
            // 全てのパラメータマーカーを削除
            parameterMarkersRef.current.forEach((marker) => {
                map.removeLayer(marker);
            });
            parameterMarkersRef.current = [];
            setParameterMarkers([]);
            // 経路情報もクリア
            currentRouteInfoRef.current = undefined;
            currentLayerRef.current = null;
        }
    }, [routeLayers.length]);

    // スライダーを再描画する関数
    const redrawSliders = () => {
        if (currentRouteInfoRef.current && currentLayerRef.current) {
            handleRouteClick(currentRouteInfoRef.current, currentLayerRef.current);
        }
        // 194-195のスライダーも再描画（page.tsxで管理）
        if (onShowSlider194_195) {
            onShowSlider194_195();
        }
    };

    // 経路クリック時に重み設定スライダーを表示
    const handleRouteClick = async (routeInfo: RouteResult | undefined, layer: L.GeoJSON) => {
        const map = mapRef.current;
        if (!routeInfo || !map) return;

        // 前回のパラメータマーカーを削除
        parameterMarkersRef.current.forEach((marker) => {
            map.removeLayer(marker);
        });
        parameterMarkersRef.current = [];

        // 経路クリック時は何もしない（194-195のスライダーは信号クリック時にのみ表示）
        if (onRouteClick) {
            onRouteClick(routeInfo);
        }
    };

    // 経路レイヤーが変更されたときにパラメータマーカーをクリア
    useEffect(() => {
        // 経路レイヤーが変更されたとき（削除されたとき）にパラメータマーカーをクリア
        parameterMarkersRef.current.forEach((marker) => {
            if (mapRef.current) {
                mapRef.current.removeLayer(marker);
            }
        });
        parameterMarkersRef.current = [];
        setParameterMarkers([]);

        return () => {
            parameterMarkersRef.current.forEach((marker) => {
                if (mapRef.current) {
                    mapRef.current.removeLayer(marker);
                }
            });
            parameterMarkersRef.current = [];
        };
    }, [routeLayers.length]);

    return (
        <div
            className="bg-white rounded-2xl overflow-hidden card-shadow mb-3"
            style={{ height: '500px', minHeight: '500px' }}
        >
            <MapContainer
                center={[35.95017, 139.64735]}
                zoom={15}
                style={{ height: '100%', width: '100%' }}
                scrollWheelZoom={true}
            >
                <TileLayer
                    url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
                    attribution='© <a href="http://osm.org/copyright">OpenStreetMap</a> contributors, <a href="http://creativecommons.org/licenses/by-sa/2.0/">CC-BY-SA</a>'
                />
                <MapUpdater
                    mapRef={mapRef}
                    onShowSlider194_195={onShowSlider194_195}
                    slider194_195VisibleRef={slider194_195VisibleRef}
                    slider194_195CreatingRef={slider194_195CreatingRef}
                />
                {onMapClick && <MapEvents onMapClick={onMapClick} />}
                {routeLayers.map((layer, index) => (
                    <GeoJSON
                        key={`route-${index}`}
                        data={layer.data}
                        style={layer.style}
                        eventHandlers={{
                            click: (e) => {
                                // マップのクリックイベントを停止
                                L.DomEvent.stopPropagation(e);
                                const routeInfo = layer.routeInfo;
                                if (routeInfo) {
                                    handleRouteClick(routeInfo, e.target);
                                }
                            },
                            mousedown: (e) => {
                                // マップのドラッグを防ぐ
                                L.DomEvent.stopPropagation(e);
                            },
                        }}
                        onEachFeature={(feature, layerInstance) => {
                            // 各feature（RE）のクリックイベントも停止
                            layerInstance.on({
                                click: (e) => {
                                    L.DomEvent.stopPropagation(e);
                                },
                                mousedown: (e) => {
                                    L.DomEvent.stopPropagation(e);
                                },
                            });
                        }}
                    />
                ))}
                {dataLayers.map((layer, index) => (
                    <GeoJSON
                        key={`data-${index}`}
                        data={layer.data}
                        style={layer.style}
                        eventHandlers={{
                            click: async (e) => {
                                // マップのクリックイベントを停止
                                L.DomEvent.stopPropagation(e);

                                // 信号レイヤーで、特定の信号がクリックされた場合
                                if (layer.edgeName) {
                                    const targetEdges = ['25-196', '196-197', '195-197', '25-195'];
                                    const edgeName = layer.edgeName;

                                    // 正規化（順序を考慮）
                                    const normalizedEdge = edgeName.includes('-')
                                        ? edgeName
                                        : edgeName;
                                    const reversedEdge = edgeName.includes('-')
                                        ? edgeName.split('-').reverse().join('-')
                                        : edgeName;

                                    const isTargetSignal = targetEdges.some(
                                        (targetEdge) =>
                                            normalizedEdge === targetEdge ||
                                            reversedEdge === targetEdge
                                    );

                                    if (isTargetSignal && onShowSlider194_195) {
                                        // 194-195のスライダーを表示（page.tsxで管理）
                                        onShowSlider194_195();
                                    }
                                }
                            },
                            mousedown: (e) => {
                                L.DomEvent.stopPropagation(e);
                            },
                        }}
                        onEachFeature={(feature, layerInstance) => {
                            if (layer.popup) {
                                layerInstance.bindPopup(layer.popup);
                            }
                            layerInstance.on({
                                click: async (e) => {
                                    L.DomEvent.stopPropagation(e);

                                    // 信号レイヤーで、特定の信号がクリックされた場合
                                    if (layer.edgeName) {
                                        const targetEdges = [
                                            '25-196',
                                            '196-197',
                                            '195-197',
                                            '25-195',
                                        ];
                                        const edgeName = layer.edgeName;

                                        // 正規化（順序を考慮）
                                        const normalizedEdge = edgeName.includes('-')
                                            ? edgeName
                                            : edgeName;
                                        const reversedEdge = edgeName.includes('-')
                                            ? edgeName.split('-').reverse().join('-')
                                            : edgeName;

                                        const isTargetSignal = targetEdges.some(
                                            (targetEdge) =>
                                                normalizedEdge === targetEdge ||
                                                reversedEdge === targetEdge
                                        );

                                        if (isTargetSignal && onShowSlider194_195) {
                                            // 194-195のスライダーを表示（page.tsxで管理）
                                            onShowSlider194_195();
                                        }
                                    }
                                },
                                mouseover: (e) => {
                                    const target = e.target;
                                    target.setStyle({
                                        opacity: 0.8,
                                        weight: 20,
                                    });
                                    if (layer.popup) {
                                        target.openPopup();
                                    }
                                },
                                mouseout: (e) => {
                                    const target = e.target;
                                    target.setStyle({
                                        opacity: layer.style.opacity || 0.5,
                                        weight: layer.style.weight || 15,
                                    });
                                    if (layer.popup) {
                                        target.closePopup();
                                    }
                                },
                            });
                        }}
                    />
                ))}
                {startMarker && (
                    <AutoOpenStartMarker
                        key={`start-${startMarker.nodeId}-${startMarker.position[0]}-${startMarker.position[1]}`}
                        position={startMarker.position}
                        icon={getBlueIcon()}
                        startNode={startMarker.startNode}
                        endNode={startMarker.endNode}
                    />
                )}
                {endMarker && (
                    <EndMarker
                        key={`end-${endMarker.nodeId}-${endMarker.position[0]}-${endMarker.position[1]}`}
                        position={endMarker.position}
                        icon={getRedIcon()}
                        nodeId={endMarker.nodeId}
                    />
                )}
                {intersectionPins.map((pin, index) => {
                    // ピンの色を選択状態に応じて変更
                    let pinColor = 'blue'; // デフォルトは青
                    if (pinSelectionState === 'start') {
                        pinColor = 'yellow'; // 始点選択待ちは黄
                    } else if (pinSelectionState === 'end') {
                        pinColor = 'yellow'; // 終点選択待ちは黄
                    }

                    const pinIcon =
                        typeof window !== 'undefined'
                            ? new L.Icon({
                                  iconUrl: `https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-${pinColor}.png`,
                                  shadowUrl:
                                      'https://cdnjs.cloudflare.com/ajax/libs/leaflet/0.7.7/images/marker-shadow.png',
                                  iconSize: [25, 41],
                                  iconAnchor: [12, 41],
                                  popupAnchor: [1, -34],
                                  shadowSize: [41, 41],
                              })
                            : null;

                    return (
                        <Marker
                            key={`intersection-${pin.nodeId}-${index}`}
                            position={pin.position}
                            icon={pinIcon || undefined}
                            eventHandlers={{
                                click: () => {
                                    if (onIntersectionPinClick) {
                                        onIntersectionPinClick(pin.nodeId, pin.position);
                                    }
                                },
                            }}
                        >
                            <Popup autoPan={false}>
                                <div>
                                    <div>
                                        <strong>ノードID:</strong> {pin.nodeId}
                                    </div>
                                    {pinSelectionState === 'start' && (
                                        <div className="text-green-400">始点を選択してください</div>
                                    )}
                                    {pinSelectionState === 'end' && (
                                        <div className="text-red-400">終点を選択してください</div>
                                    )}
                                </div>
                            </Popup>
                        </Marker>
                    );
                })}
            </MapContainer>
        </div>
    );
}
