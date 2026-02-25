'use client';
import { useEffect, useRef, useState, useCallback } from 'react';

const WS_URL = process.env.NEXT_PUBLIC_WS_URL || 'ws://localhost:3001/ws';

export function useWebSocket() {
    const wsRef = useRef(null);
    const [connected, setConnected] = useState(false);
    const [lastMessage, setLastMessage] = useState(null);
    const listenersRef = useRef(new Map());

    const connect = useCallback(() => {
        const token = typeof window !== 'undefined' ? localStorage.getItem('cc_access_token') : null;
        const url = token ? `${WS_URL}?token=${token}` : WS_URL;
        const ws = new WebSocket(url);

        ws.onopen = () => { setConnected(true); console.log('[WS] Connected'); };
        ws.onclose = () => {
            setConnected(false);
            console.log('[WS] Disconnected. Reconnecting in 5s...');
            setTimeout(connect, 5000);
        };
        ws.onerror = (e) => console.error('[WS] Error', e);
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                setLastMessage(data);
                // Notify type-specific listeners
                const typeListeners = listenersRef.current.get(data.type);
                if (typeListeners) typeListeners.forEach(cb => cb(data));
            } catch { }
        };

        wsRef.current = ws;
    }, []);

    const subscribe = useCallback((type, callback) => {
        if (!listenersRef.current.has(type)) listenersRef.current.set(type, new Set());
        listenersRef.current.get(type).add(callback);
        return () => listenersRef.current.get(type)?.delete(callback);
    }, []);

    useEffect(() => {
        connect();
        return () => { if (wsRef.current) wsRef.current.close(); };
    }, [connect]);

    return { connected, lastMessage, subscribe };
}
