// @turf/turf の型定義
// package.jsonのexportsフィールドの問題を回避するため、明示的に型定義を提供
declare module '@turf/turf' {
  const turf: any;
  export = turf;
  export as namespace turf;
}
