'use client';

interface WeightSliderProps {
  id: string;
  label: string;
  value: number;
  min: number;
  max: number;
  step: number;
  onChange: (value: number) => void;
  disabled?: boolean;
}

export default function WeightSlider({
  id,
  label,
  value,
  min,
  max,
  step,
  onChange,
  disabled = false,
}: WeightSliderProps) {
  const handleSliderChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = parseFloat(e.target.value);
    onChange(newValue);
    const textInput = document.getElementById(`text${id}`) as HTMLInputElement;
    if (textInput) {
      textInput.value = newValue.toString();
    }
  };

  const handleTextChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = parseFloat(e.target.value) || 0;
    onChange(newValue);
    const slider = document.getElementById(id) as HTMLInputElement;
    if (slider) {
      slider.value = newValue.toString();
    }
  };

  return (
    <div className="bg-white/10 p-3 rounded-lg border border-white/20 flex flex-col space-y-2">
      <div className="flex items-center justify-between">
        <label className="text-white/90 text-sm font-medium">{label}</label>
        <input
          type="text"
          id={`text${id}`}
          value={value}
          disabled={disabled}
          className="w-14 px-1 py-0.5 text-xs text-center rounded bg-white/20 text-white border border-white/30 focus:border-yellow-300 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed"
          onChange={handleTextChange}
        />
      </div>
      <input
        type="range"
        id={id}
        value={value}
        min={min}
        max={max}
        step={step}
        disabled={disabled}
        className="range-slider w-full h-1 bg-white/20 rounded-full appearance-none cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
        onChange={handleSliderChange}
      />
    </div>
  );
}

