'use client';

import { useEffect, useState } from 'react';

interface ToastProps {
    message: string;
    type?: 'info' | 'success' | 'error';
    duration?: number;
    onClose?: () => void;
}

export default function Toast({ message, type = 'info', duration = 3000, onClose }: ToastProps) {
    const [show, setShow] = useState(false);

    useEffect(() => {
        setShow(true);
        const timer = setTimeout(() => {
            setShow(false);
            setTimeout(() => {
                onClose?.();
            }, 300);
        }, duration);
        return () => clearTimeout(timer);
    }, [duration, onClose]);

    const iconClass =
        type === 'success'
            ? 'fas fa-check-circle'
            : type === 'error'
            ? 'fas fa-exclamation-circle'
            : 'fas fa-info-circle';

    return (
        <div
            className={`toast ${type} ${show ? 'show' : ''}`}
            style={{
                position: 'fixed',
                top: '20px',
                left: '50%',
                transform: 'translateX(-50%)',
                zIndex: 9999,
            }}
        >
            <i className={iconClass}></i>
            <span>{message}</span>
        </div>
    );
}
