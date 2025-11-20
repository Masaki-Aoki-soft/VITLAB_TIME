'use client';

import { useEffect, useRef } from 'react';
import { MapContainer, TileLayer, GeoJSON, useMap, useMapEvents, Popup, Marker } from 'react-leaflet';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import type { GeoJSON as GeoJSONType } from 'geojson';

// Leafletのデフォルトアイコンの問題を修正
if (typeof window !== 'undefined') {
  delete (L.Icon.Default.prototype as any)._getIconUrl;
  L.Icon.Default.mergeOptions({
    iconRetinaUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon-2x.png',
    iconUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon.png',
    shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-shadow.png',
  });
}

// 青いマーカーアイコンを取得する関数
const getBlueIcon = (): L.Icon | null => {
  if (typeof window === 'undefined') return null;
  return new L.Icon({
    iconUrl: 'https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-blue.png',
    shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/0.7.7/images/marker-shadow.png',
    iconSize: [25, 41],
    iconAnchor: [12, 41],
    popupAnchor: [1, -34],
    shadowSize: [41, 41],
  });
};

interface MapComponentProps {
  mapRef: React.MutableRefObject<L.Map | null>;
  onMapClick?: (lat: number, lng: number) => void;
  routeLayers?: Array<{ data: GeoJSONType; style: L.PathOptions }>;
  dataLayers?: Array<{ data: GeoJSONType; style: L.PathOptions; popup?: string; edgeName?: string }>;
  startMarker?: { position: [number, number]; nodeId: string; startNode: string; endNode: string } | null;
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
function MapUpdater({ mapRef }: { mapRef: React.MutableRefObject<L.Map | null> }) {
  const map = useMap();

  useEffect(() => {
    mapRef.current = map;
    // マップサイズの更新
    setTimeout(() => {
      map.invalidateSize();
    }, 100);
  }, [map, mapRef]);

  return null;
}

// 自動的にポップアップを開くマーカーコンポーネント
function AutoOpenMarker({ position, icon, startNode, endNode }: { position: [number, number]; icon: L.Icon | null; startNode: string; endNode: string }) {
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
        }
      }}
    >
      <Popup>
        <div>
          <div><strong>始点:</strong> {startNode}</div>
          <div><strong>終点:</strong> {endNode}</div>
        </div>
      </Popup>
    </Marker>
  );
}

export default function MapComponent({ mapRef, onMapClick, routeLayers = [], dataLayers = [], startMarker }: MapComponentProps) {
  return (
    <div className="bg-white rounded-2xl overflow-hidden card-shadow h-[375px] mb-3">
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
        <MapUpdater mapRef={mapRef} />
        {onMapClick && <MapEvents onMapClick={onMapClick} />}
        {routeLayers.map((layer, index) => (
          <GeoJSON key={`route-${index}`} data={layer.data} style={layer.style} />
        ))}
        {dataLayers.map((layer, index) => (
          <GeoJSON
            key={`data-${index}`}
            data={layer.data}
            style={layer.style}
            onEachFeature={(feature, layerInstance) => {
              if (layer.popup) {
                layerInstance.bindPopup(layer.popup);
              }
              layerInstance.on({
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
          <AutoOpenMarker 
            key={`${startMarker.nodeId}-${startMarker.position[0]}-${startMarker.position[1]}`}
            position={startMarker.position} 
            icon={getBlueIcon()}
            startNode={startMarker.startNode}
            endNode={startMarker.endNode}
          />
        )}
      </MapContainer>
    </div>
  );
}
