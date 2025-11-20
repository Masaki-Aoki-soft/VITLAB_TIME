export const formatTime = (minutes: number | null): string => {
  if (minutes === null || isNaN(minutes)) {
    return '0分00秒';
  }
  const totalSeconds = Math.round(minutes * 60);
  const displayMinutes = Math.floor(totalSeconds / 60);
  const displaySeconds = totalSeconds % 60;
  return `${displayMinutes}分${displaySeconds.toString().padStart(2, '0')}秒`;
};

export const syncValues = (source: HTMLInputElement, targetId: string) => {
  const target = document.getElementById(targetId) as HTMLInputElement;
  if (target) {
    target.value = source.value;
  }
};

