'use client';

interface LoadingOverlayProps {
    isLoading: boolean;
    message: string;
}

export default function LoadingOverlay({ isLoading, message }: LoadingOverlayProps) {
    if (!isLoading) return null;

    return (
        <div className="fixed inset-0 bg-black/80 backdrop-blur-sm z-[9999] flex items-center justify-center">
            <div className="glass-effect rounded-2xl p-8 flex items-center space-x-4 text-white">
                <div className="w-8 h-8 border-3 border-white/30 border-t-white rounded-full animate-spin"></div>
                <div className="text-lg font-medium">{message}</div>
            </div>
        </div>
    );
}
