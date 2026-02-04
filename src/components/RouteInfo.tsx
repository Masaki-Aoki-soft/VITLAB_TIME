'use client';

import { formatTime } from '@/lib/utils';
import { RouteInfo as RouteInfoType } from '@/lib/types';

interface RouteInfoProps {
    route1?: RouteInfoType | null;
    route2?: RouteInfoType | null;
    savedRoute?: RouteInfoType | null;
    bestEnumRoute?: RouteInfoType | null;
    purpleRoute?: RouteInfoType | null;
}

export default function RouteInfo({
    route1,
    route2,
    savedRoute,
    bestEnumRoute,
    purpleRoute,
}: RouteInfoProps) {
    let content = '';
    let hasInfo = false;

    if (purpleRoute) {
        hasInfo = true;
        const waitTimeSecondsP = purpleRoute.totalWaitTime
            ? Math.round(purpleRoute.totalWaitTime * 60)
            : 0;
        content += `
      <div class="bg-purple-500/20 border border-purple-300/30 rounded-lg p-2">
        <h4 class="text-purple-200 font-semibold mb-2 flex items-center gap-2 text-xs">
          <i class="fas fa-route"></i>選択中の経路 (紫)
        </h4>
        <div class="grid grid-cols-3 gap-1 text-xs">
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">総距離</div>
            <div class="text-white font-semibold text-xs">${purpleRoute.totalDistance.toFixed(
                0
            )}m</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">所要時間</div>
            <div class="text-white font-semibold text-xs">${formatTime(purpleRoute.totalTime)}</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">待ち時間</div>
            <div class="text-white font-semibold text-xs">${waitTimeSecondsP}秒</div>
          </div>
        </div>
      </div>
    `;
    } else {
        if (route1) {
            hasInfo = true;
            const waitTimeSeconds1 = route1.totalWaitTime
                ? Math.round(route1.totalWaitTime * 60)
                : 0;
            content += `
      <div class="bg-green-500/20 border border-green-300/30 rounded-lg p-2">
        <h4 class="text-green-200 font-semibold mb-2 flex items-center gap-2 text-xs">
          <i class="fas fa-route"></i>経路1 (基準時刻1 - 緑)
        </h4>
        <div class="grid grid-cols-3 gap-1 text-xs">
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">総距離</div>
            <div class="text-white font-semibold text-xs">${route1.totalDistance.toFixed(0)}m</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">所要時間</div>
            <div class="text-white font-semibold text-xs">${formatTime(route1.totalTime)}</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">待ち時間</div>
            <div class="text-white font-semibold text-xs">${waitTimeSeconds1}秒</div>
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
            
            // 勾配影響時間の表示
            let gradientInfo = '';
            if (route2.totalGradientDiff !== undefined) {
                 const diff = route2.totalGradientDiff;
                 const sign = diff >= 0 ? '+' : '';
                 gradientInfo = `<span class="ml-1 text-[10px] text-blue-200">(${sign}${diff.toFixed(2)}秒)</span>`;
            }

            content += `
      <div class="bg-blue-500/20 border border-blue-300/30 rounded-lg p-2">
        <h4 class="text-blue-200 font-semibold mb-2 flex items-center gap-2 text-xs">
          <i class="fas fa-route"></i>経路2 (基準時刻2 - 青)
        </h4>
        <div class="grid grid-cols-3 gap-1 text-xs">
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">総距離</div>
            <div class="text-white font-semibold text-xs">${route2.totalDistance.toFixed(0)}m</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">所要時間</div>
            <div class="text-white font-semibold text-xs flex flex-col items-center justify-center">
                <span>${formatTime(route2.totalTime)}</span>
                ${gradientInfo}
            </div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">待ち時間</div>
            <div class="text-white font-semibold text-xs">${waitTimeSeconds2}秒</div>
          </div>
        </div>
      </div>
    `;
        }
    }

    if (bestEnumRoute) {
        hasInfo = true;
        const waitTimeSeconds3 = bestEnumRoute.totalWaitTime ? Math.round(bestEnumRoute.totalWaitTime * 60) : 0;
        // 最短全網羅経路（赤）
        // 所要時間は移動時間 + 信号の待ち時間
        content += `
      <div class="bg-red-500/20 border border-red-300/30 rounded-lg p-2">
        <h4 class="text-red-200 font-semibold mb-2 flex items-center gap-2 text-xs">
          <i class="fas fa-route"></i>最短全網羅 (赤)
        </h4>
        <div class="grid grid-cols-3 gap-1 text-xs">
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">総距離</div>
            <div class="text-white font-semibold text-xs">${bestEnumRoute.totalDistance.toFixed(0)}m</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">所要時間</div>
            <div class="text-white font-semibold text-xs">${formatTime(bestEnumRoute.totalTime)}</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">待ち時間</div>
            <div class="text-white font-semibold text-xs">${waitTimeSeconds3}秒</div>
          </div>
        </div>
      </div>
    `;
    } else if (savedRoute) {
        hasInfo = true;
        // 呼び出し経路（緑）
        content += `
      <div class="bg-green-500/20 border border-green-300/30 rounded-lg p-2">
        <h4 class="text-green-200 font-semibold mb-2 flex items-center gap-2 text-xs">
          <i class="fas fa-route"></i>呼び出し経路 (緑)
        </h4>
        <div class="grid grid-cols-2 gap-1 text-xs">
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">総距離</div>
            <div class="text-white font-semibold text-xs">${savedRoute.totalDistance.toFixed(0)}m</div>
          </div>
          <div class="bg-white/10 rounded p-1 text-center">
            <div class="text-white/70 text-xs">所要時間</div>
            <div class="text-white font-semibold text-xs">${formatTime(savedRoute.totalTime)}</div>
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
