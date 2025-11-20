'use client';

import { formatTime } from '@/lib/utils';
import { RouteInfo as RouteInfoType } from '@/lib/types';

interface RouteInfoProps {
  route1?: RouteInfoType | null;
  route2?: RouteInfoType | null;
  savedRoute?: RouteInfoType | null;
}

export default function RouteInfo({ route1, route2, savedRoute }: RouteInfoProps) {
  let content = '';
  let hasInfo = false;

  if (route1) {
    hasInfo = true;
    const waitTimeSeconds1 = route1.totalWaitTime
      ? Math.round(route1.totalWaitTime * 60)
      : 0;
    content += `
      <div class="bg-red-500/20 border border-red-300/30 rounded-lg p-2">
        <h4 class="text-red-200 font-semibold mb-2 flex items-center gap-2">
          <i class="fas fa-route"></i>経路1 (赤)
        </h4>
        <div class="grid grid-cols-3 gap-2 text-sm">
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">総距離</div>
            <div class="text-white font-semibold">${route1.totalDistance.toFixed(0)} m</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">所要時間</div>
            <div class="text-white font-semibold">${formatTime(route1.totalTime)}</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">待ち時間</div>
            <div class="text-white font-semibold">${waitTimeSeconds1}秒</div>
          </div>
        </div>
      </div>
    `;
  }

  if (route2) {
    hasInfo = true;
    const waitTimeSeconds2 = route2.totalWaitTime
      ? Math.round(route2.totalWaitTime * 60)
      : 0;
    content += `
      <div class="bg-blue-500/20 border border-blue-300/30 rounded-lg p-2">
        <h4 class="text-blue-200 font-semibold mb-2 flex items-center gap-2">
          <i class="fas fa-route"></i>経路2 (青)
        </h4>
        <div class="grid grid-cols-3 gap-2 text-sm">
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">総距離</div>
            <div class="text-white font-semibold">${route2.totalDistance.toFixed(0)} m</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">所要時間</div>
            <div class="text-white font-semibold">${formatTime(route2.totalTime)}</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">待ち時間</div>
            <div class="text-white font-semibold">${waitTimeSeconds2}秒</div>
          </div>
        </div>
      </div>
    `;
  }

  if (savedRoute) {
    hasInfo = true;
    content += `
      <div class="bg-green-500/20 border border-green-300/30 rounded-lg p-2">
        <h4 class="text-green-200 font-semibold mb-2 flex items-center gap-2">
          <i class="fas fa-route"></i>呼び出し経路 (緑)
        </h4>
        <div class="grid grid-cols-2 gap-2 text-sm">
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">総距離</div>
            <div class="text-white font-semibold">${savedRoute.totalDistance.toFixed(0)} m</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">所要時間</div>
            <div class="text-white font-semibold">${formatTime(savedRoute.totalTime)}</div>
          </div>
        </div>
      </div>
    `;
  }

  if (!hasInfo) {
    content =
      '<div class="text-white/70 text-center italic text-sm">経路を検索すると詳細情報が表示されます</div>';
  }

  return (
    <div
      id="route-info"
      className="flex gap-3 text-white/90 text-sm flex-wrap"
      dangerouslySetInnerHTML={{ __html: content }}
    />
  );
}

