'use client';
import { useState, useEffect, useCallback } from 'react';
import Link from 'next/link';
import api from '@/lib/api';
import { useWebSocket } from '@/lib/useWebSocket';

export default function DashboardPage() {
    const [summary, setSummary] = useState(null);
    const [stats, setStats] = useState({ total: 0, online: 0, offline: 0 });
    const [loading, setLoading] = useState(true);
    const { subscribe, connected } = useWebSocket();

    const fetchData = useCallback(async () => {
        try {
            const res = await api.getDashboardSummary();
            setSummary(res.devices);
            setStats(res.stats);
        } catch (err) {
            console.error('Dashboard fetch error:', err);
        } finally {
            setLoading(false);
        }
    }, []);

    useEffect(() => { fetchData(); }, [fetchData]);

    // Real-time telemetry updates
    useEffect(() => {
        const unsub = subscribe('telemetry', (msg) => {
            setSummary(prev => {
                if (!prev) return prev;
                return prev.map(d =>
                    d.device_uid === msg.deviceUid
                        ? {
                            ...d, temperature: msg.data.temperature, humidity: msg.data.humidity,
                            heater_on: msg.data.heater, cooler_on: msg.data.cooler,
                            recorded_at: msg.timestamp, is_online: true
                        }
                        : d
                );
            });
        });
        return unsub;
    }, [subscribe]);

    useEffect(() => {
        const unsub = subscribe('device_status', (msg) => {
            setSummary(prev => {
                if (!prev) return prev;
                return prev.map(d =>
                    d.device_uid === msg.deviceUid ? { ...d, is_online: msg.status === 'online' } : d
                );
            });
        });
        return unsub;
    }, [subscribe]);

    if (loading) {
        return (
            <>
                <div className="page-header">
                    <div><div className="skeleton" style={{ width: 200, height: 28 }} /></div>
                </div>
                <div className="page-body">
                    <div className="card-grid">
                        {[1, 2, 3, 4].map(i => <div key={i} className="skeleton" style={{ height: 100, borderRadius: 16 }} />)}
                    </div>
                </div>
            </>
        );
    }

    return (
        <>
            <div className="page-header">
                <div>
                    <h1 className="page-title">Dashboard</h1>
                    <p className="page-subtitle">Overview of all your devices</p>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
                    <span className={`status-badge ${connected ? 'online' : 'offline'}`}>
                        <span className="status-dot" />
                        {connected ? 'Live' : 'Connecting...'}
                    </span>
                    <Link href="/dashboard/devices" className="btn btn-primary btn-sm">
                        Manage Devices
                    </Link>
                </div>
            </div>

            <div className="page-body animate-fade-in">
                {/* Stats Row */}
                <div className="card-grid" style={{ marginBottom: 28 }}>
                    <div className="stat-card">
                        <div className="stat-icon device">📡</div>
                        <div className="stat-content">
                            <div className="stat-label">Total Devices</div>
                            <div className="stat-value">{stats.total}</div>
                        </div>
                    </div>
                    <div className="stat-card">
                        <div className="stat-icon online">✅</div>
                        <div className="stat-content">
                            <div className="stat-label">Online</div>
                            <div className="stat-value" style={{ color: 'var(--accent-green)' }}>{stats.online}</div>
                        </div>
                    </div>
                    <div className="stat-card">
                        <div className="stat-icon offline">⚠️</div>
                        <div className="stat-content">
                            <div className="stat-label">Offline</div>
                            <div className="stat-value" style={{ color: 'var(--accent-red)' }}>{stats.offline}</div>
                        </div>
                    </div>
                </div>

                {/* Device Cards */}
                {(!summary || summary.length === 0) ? (
                    <div className="empty-state">
                        <div className="empty-state-icon">📡</div>
                        <div className="empty-state-title">No devices yet</div>
                        <div className="empty-state-text">Register your first device to start monitoring.</div>
                        <Link href="/dashboard/devices" className="btn btn-primary">Add Device</Link>
                    </div>
                ) : (
                    <div className="card-grid">
                        {summary.map((device, i) => (
                            <Link key={device.id || i} href={`/dashboard/devices/${device.id}`} style={{ textDecoration: 'none' }}>
                                <div className="device-card animate-fade-in" style={{ animationDelay: `${i * 0.05}s` }}>
                                    <div className="device-card-header">
                                        <div>
                                            <div className="device-name">{device.name}</div>
                                            <div className="device-uid">{device.device_uid}</div>
                                        </div>
                                        <span className={`status-badge ${device.is_online ? 'online' : 'offline'}`}>
                                            <span className="status-dot" />
                                            {device.is_online ? 'Online' : 'Offline'}
                                        </span>
                                    </div>
                                    <div className="device-metrics">
                                        <div className="device-metric">
                                            <span className="device-metric-label">Temperature</span>
                                            <span className="device-metric-value temp">
                                                {device.temperature != null ? `${Number(device.temperature).toFixed(1)}°C` : '—'}
                                            </span>
                                        </div>
                                        <div className="device-metric">
                                            <span className="device-metric-label">Humidity</span>
                                            <span className="device-metric-value humid">
                                                {device.humidity != null ? `${Math.round(device.humidity)}%` : '—'}
                                            </span>
                                        </div>
                                        <div className="device-metric">
                                            <span className="device-metric-label">Heater</span>
                                            <span className={`relay-indicator ${device.heater_on ? 'on' : 'off'}`}>
                                                {device.heater_on ? '● ON' : '○ OFF'}
                                            </span>
                                        </div>
                                        <div className="device-metric">
                                            <span className="device-metric-label">Cooler</span>
                                            <span className={`relay-indicator ${device.cooler_on ? 'on' : 'off'}`}>
                                                {device.cooler_on ? '● ON' : '○ OFF'}
                                            </span>
                                        </div>
                                    </div>
                                    {device.last_seen && (
                                        <div style={{ marginTop: 12, fontSize: 11, color: 'var(--text-muted)' }}>
                                            Last seen: {new Date(device.last_seen).toLocaleString()}
                                        </div>
                                    )}
                                </div>
                            </Link>
                        ))}
                    </div>
                )}
            </div>
        </>
    );
}
