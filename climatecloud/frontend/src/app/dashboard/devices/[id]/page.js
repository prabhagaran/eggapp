'use client';
import { useState, useEffect, useCallback } from 'react';
import { useParams } from 'next/navigation';
import api from '@/lib/api';
import { useWebSocket } from '@/lib/useWebSocket';

export default function DeviceDetailPage() {
    const { id } = useParams();
    const [device, setDevice] = useState(null);
    const [shadow, setShadow] = useState(null);
    const [latest, setLatest] = useState(null);
    const [history, setHistory] = useState([]);
    const [cmdHistory, setCmdHistory] = useState([]);
    const [loading, setLoading] = useState(true);
    const [setpoint, setSetpoint] = useState(37.5);
    const [humSetpoint, setHumSetpoint] = useState(60);
    const [cmdLoading, setCmdLoading] = useState(false);
    const [activeTab, setActiveTab] = useState('overview');
    const { subscribe, connected } = useWebSocket();

    const fetchAll = useCallback(async () => {
        try {
            const [devRes, telRes, histRes, cmdRes] = await Promise.all([
                api.getDevice(id),
                api.getLatestTelemetry(id),
                api.getTelemetry(id, { limit: 50 }),
                api.getCommands(id),
            ]);
            setDevice(devRes.device);
            setShadow(devRes.shadow);
            setLatest(telRes.telemetry);
            setHistory(histRes.telemetry || []);
            setCmdHistory(cmdRes.commands || []);
            if (devRes.shadow?.desired?.setpoint) setSetpoint(devRes.shadow.desired.setpoint);
            if (devRes.shadow?.desired?.humid_setpoint) setHumSetpoint(devRes.shadow.desired.humid_setpoint);
        } catch (err) { console.error(err); }
        finally { setLoading(false); }
    }, [id]);

    useEffect(() => { fetchAll(); }, [fetchAll]);

    useEffect(() => {
        const unsub = subscribe('telemetry', (msg) => {
            if (msg.deviceUid === device?.deviceUid) {
                setLatest(prev => ({ ...prev, ...msg.data, recorded_at: msg.timestamp }));
                setHistory(prev => [{ ...msg.data, recordedAt: msg.timestamp }, ...prev].slice(0, 50));
            }
        });
        return unsub;
    }, [subscribe, device?.deviceUid]);

    const sendSetpoint = async () => {
        setCmdLoading(true);
        try {
            await api.sendCommand(id, { commandType: 'set_setpoint', payload: { setpoint } });
            fetchAll();
        } catch (err) { alert(err.message); }
        finally { setCmdLoading(false); }
    };

    const sendHumSetpoint = async () => {
        setCmdLoading(true);
        try {
            await api.sendCommand(id, { commandType: 'set_humidity_setpoint', payload: { humid_setpoint: humSetpoint } });
        } catch (err) { alert(err.message); }
        finally { setCmdLoading(false); }
    };

    const sendMode = async (mode) => {
        setCmdLoading(true);
        try {
            await api.sendCommand(id, { commandType: 'set_mode', payload: { mode } });
        } catch (err) { alert(err.message); }
        finally { setCmdLoading(false); }
    };

    if (loading) return (
        <>
            <div className="page-header"><div className="skeleton" style={{ width: 300, height: 28 }} /></div>
            <div className="page-body"><div className="skeleton" style={{ height: 400, borderRadius: 16 }} /></div>
        </>
    );

    if (!device) return (
        <div className="page-body"><div className="empty-state"><div className="empty-state-title">Device not found</div></div></div>
    );

    return (
        <>
            <div className="page-header">
                <div>
                    <h1 className="page-title">{device.name}</h1>
                    <p className="page-subtitle" style={{ fontFamily: 'monospace' }}>{device.deviceUid} • FW {device.fwVersion || 'Unknown'}</p>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
                    <span className={`status-badge ${device.isOnline ? 'online' : 'offline'}`}>
                        <span className="status-dot" />{device.isOnline ? 'Online' : 'Offline'}
                    </span>
                    <span className={`status-badge ${connected ? 'online' : 'offline'}`}>
                        <span className="status-dot" />{connected ? 'Live' : 'Offline'}
                    </span>
                </div>
            </div>

            <div className="page-body animate-fade-in">
                {/* Tabs */}
                <div style={{ display: 'flex', gap: 4, marginBottom: 24, borderBottom: '1px solid var(--border-color)', paddingBottom: 0 }}>
                    {['overview', 'commands', 'shadow'].map(tab => (
                        <button key={tab} onClick={() => setActiveTab(tab)}
                            style={{
                                padding: '10px 20px', border: 'none', background: 'none', cursor: 'pointer',
                                color: activeTab === tab ? 'var(--accent-blue)' : 'var(--text-muted)',
                                borderBottom: activeTab === tab ? '2px solid var(--accent-blue)' : '2px solid transparent',
                                fontWeight: 600, fontSize: 14, fontFamily: 'var(--font-family)',
                                textTransform: 'capitalize', transition: 'var(--transition)',
                            }}>{tab}</button>
                    ))}
                </div>

                {activeTab === 'overview' && (
                    <>
                        {/* Live Metrics */}
                        <div className="card-grid" style={{ marginBottom: 24 }}>
                            <div className="stat-card">
                                <div className="stat-icon temp">🌡</div>
                                <div className="stat-content">
                                    <div className="stat-label">Temperature</div>
                                    <div className="stat-value" style={{ color: 'var(--accent-orange)' }}>
                                        {latest?.temperature != null ? `${Number(latest.temperature).toFixed(1)}°C` : '—'}
                                    </div>
                                </div>
                            </div>
                            <div className="stat-card">
                                <div className="stat-icon humid">💧</div>
                                <div className="stat-content">
                                    <div className="stat-label">Humidity</div>
                                    <div className="stat-value" style={{ color: 'var(--accent-cyan)' }}>
                                        {latest?.humidity != null ? `${Math.round(latest.humidity)}%` : '—'}
                                    </div>
                                </div>
                            </div>
                            <div className="stat-card">
                                <div className="stat-icon blue">🎯</div>
                                <div className="stat-content">
                                    <div className="stat-label">Setpoint</div>
                                    <div className="stat-value">{latest?.setpoint != null ? `${Number(latest.setpoint).toFixed(1)}°C` : '—'}</div>
                                </div>
                            </div>
                        </div>

                        {/* Relay Status */}
                        <div className="card" style={{ marginBottom: 24 }}>
                            <div className="card-title" style={{ marginBottom: 16 }}>Relay Status</div>
                            <div style={{ display: 'flex', gap: 16, flexWrap: 'wrap' }}>
                                <div className="control-item" style={{ flex: 1, minWidth: 140 }}>
                                    <div className="control-item-label">Heater</div>
                                    <span className={`relay-indicator ${latest?.heater_on || latest?.heater ? 'on' : 'off'}`} style={{ fontSize: 16 }}>
                                        {(latest?.heater_on || latest?.heater) ? '🔥 ON' : '○ OFF'}
                                    </span>
                                </div>
                                <div className="control-item" style={{ flex: 1, minWidth: 140 }}>
                                    <div className="control-item-label">Cooler</div>
                                    <span className={`relay-indicator ${latest?.cooler_on || latest?.cooler ? 'on' : 'off'}`} style={{ fontSize: 16 }}>
                                        {(latest?.cooler_on || latest?.cooler) ? '❄️ ON' : '○ OFF'}
                                    </span>
                                </div>
                                <div className="control-item" style={{ flex: 1, minWidth: 140 }}>
                                    <div className="control-item-label">Mode</div>
                                    <span style={{ fontSize: 16, fontWeight: 600, color: 'var(--accent-blue)' }}>
                                        {latest?.mode || 'AUTO'}
                                    </span>
                                </div>
                            </div>
                        </div>

                        {/* Control Panel */}
                        <div className="card">
                            <div className="card-title" style={{ marginBottom: 16 }}>🎮 Remote Control</div>
                            <div className="control-panel">
                                <div className="control-item">
                                    <div className="control-item-label">Temperature Setpoint</div>
                                    <div style={{ display: 'flex', alignItems: 'center', gap: 8, justifyContent: 'center', marginTop: 8 }}>
                                        <button className="btn btn-secondary btn-sm" onClick={() => setSetpoint(s => Math.max(20, s - 0.5))}>−</button>
                                        <span style={{ fontSize: 24, fontWeight: 700, minWidth: 80, textAlign: 'center' }}>{setpoint.toFixed(1)}°C</span>
                                        <button className="btn btn-secondary btn-sm" onClick={() => setSetpoint(s => Math.min(50, s + 0.5))}>+</button>
                                    </div>
                                    <button className="btn btn-primary btn-sm" style={{ marginTop: 12, width: '100%' }}
                                        onClick={sendSetpoint} disabled={cmdLoading}>Apply</button>
                                </div>
                                <div className="control-item">
                                    <div className="control-item-label">Humidity Setpoint</div>
                                    <div style={{ display: 'flex', alignItems: 'center', gap: 8, justifyContent: 'center', marginTop: 8 }}>
                                        <button className="btn btn-secondary btn-sm" onClick={() => setHumSetpoint(s => Math.max(0, s - 5))}>−</button>
                                        <span style={{ fontSize: 24, fontWeight: 700, minWidth: 60, textAlign: 'center' }}>{humSetpoint}%</span>
                                        <button className="btn btn-secondary btn-sm" onClick={() => setHumSetpoint(s => Math.min(100, s + 5))}>+</button>
                                    </div>
                                    <button className="btn btn-primary btn-sm" style={{ marginTop: 12, width: '100%' }}
                                        onClick={sendHumSetpoint} disabled={cmdLoading}>Apply</button>
                                </div>
                                <div className="control-item">
                                    <div className="control-item-label">Control Mode</div>
                                    <div style={{ display: 'flex', flexDirection: 'column', gap: 8, marginTop: 8 }}>
                                        <button className="btn btn-primary btn-sm" onClick={() => sendMode('AUTO')} disabled={cmdLoading}>AUTO</button>
                                        <button className="btn btn-secondary btn-sm" onClick={() => sendMode('MANUAL')} disabled={cmdLoading}>MANUAL</button>
                                    </div>
                                </div>
                            </div>
                        </div>

                        {/* Recent Telemetry History */}
                        {history.length > 0 && (
                            <div className="card" style={{ marginTop: 24 }}>
                                <div className="card-title" style={{ marginBottom: 16 }}>📈 Recent Readings</div>
                                <div className="table-container">
                                    <table>
                                        <thead>
                                            <tr><th>Time</th><th>Temp</th><th>Humidity</th><th>Heater</th><th>Cooler</th><th>Setpoint</th></tr>
                                        </thead>
                                        <tbody>
                                            {history.slice(0, 20).map((t, i) => (
                                                <tr key={i}>
                                                    <td style={{ fontSize: 12 }}>{new Date(t.recordedAt || t.recorded_at).toLocaleString()}</td>
                                                    <td style={{ color: 'var(--accent-orange)', fontWeight: 600 }}>{t.temperature != null ? `${Number(t.temperature).toFixed(1)}°C` : '—'}</td>
                                                    <td style={{ color: 'var(--accent-cyan)', fontWeight: 600 }}>{t.humidity != null ? `${Math.round(t.humidity)}%` : '—'}</td>
                                                    <td><span className={`relay-indicator ${t.heaterOn || t.heater_on || t.heater ? 'on' : 'off'}`}>{(t.heaterOn || t.heater_on || t.heater) ? 'ON' : 'OFF'}</span></td>
                                                    <td><span className={`relay-indicator ${t.coolerOn || t.cooler_on || t.cooler ? 'on' : 'off'}`}>{(t.coolerOn || t.cooler_on || t.cooler) ? 'ON' : 'OFF'}</span></td>
                                                    <td>{t.setpoint != null ? `${Number(t.setpoint).toFixed(1)}°C` : '—'}</td>
                                                </tr>
                                            ))}
                                        </tbody>
                                    </table>
                                </div>
                            </div>
                        )}
                    </>
                )}

                {activeTab === 'commands' && (
                    <div className="card">
                        <div className="card-title" style={{ marginBottom: 16 }}>📋 Command History</div>
                        {cmdHistory.length === 0 ? (
                            <div className="empty-state"><div className="empty-state-title">No commands sent yet</div></div>
                        ) : (
                            <div className="table-container">
                                <table>
                                    <thead><tr><th>Time</th><th>Type</th><th>Payload</th><th>Status</th><th>Acknowledged</th></tr></thead>
                                    <tbody>
                                        {cmdHistory.map(cmd => (
                                            <tr key={cmd.id}>
                                                <td style={{ fontSize: 12 }}>{new Date(cmd.createdAt).toLocaleString()}</td>
                                                <td><code>{cmd.commandType}</code></td>
                                                <td><code style={{ fontSize: 11 }}>{JSON.stringify(cmd.payload)}</code></td>
                                                <td>
                                                    <span className={`status-badge ${cmd.status === 'applied' ? 'online' : cmd.status === 'failed' ? 'offline' : ''}`}
                                                        style={!['applied', 'failed'].includes(cmd.status) ? { background: 'var(--accent-orange-glow)', color: 'var(--accent-orange)' } : {}}>
                                                        {cmd.status}
                                                    </span>
                                                </td>
                                                <td style={{ fontSize: 12, color: 'var(--text-muted)' }}>{cmd.ackAt ? new Date(cmd.ackAt).toLocaleString() : '—'}</td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            </div>
                        )}
                    </div>
                )}

                {activeTab === 'shadow' && (
                    <div style={{ display: 'grid', gap: 20 }}>
                        <div className="card">
                            <div className="card-title" style={{ marginBottom: 16 }}>🎯 Desired State</div>
                            <pre style={{
                                background: 'var(--bg-input)', padding: 16, borderRadius: 'var(--radius-md)',
                                fontSize: 13, color: 'var(--accent-green)', overflow: 'auto'
                            }}>
                                {JSON.stringify(shadow?.desired || {}, null, 2)}
                            </pre>
                        </div>
                        <div className="card">
                            <div className="card-title" style={{ marginBottom: 16 }}>📡 Reported State</div>
                            <pre style={{
                                background: 'var(--bg-input)', padding: 16, borderRadius: 'var(--radius-md)',
                                fontSize: 13, color: 'var(--accent-cyan)', overflow: 'auto'
                            }}>
                                {JSON.stringify(shadow?.reported || {}, null, 2)}
                            </pre>
                        </div>
                        {shadow?.delta && Object.keys(shadow.delta).length > 0 && (
                            <div className="card" style={{ borderColor: 'rgba(245,158,11,0.3)' }}>
                                <div className="card-title" style={{ marginBottom: 16, color: 'var(--accent-orange)' }}>⚡ Delta (Out of Sync)</div>
                                <pre style={{
                                    background: 'var(--bg-input)', padding: 16, borderRadius: 'var(--radius-md)',
                                    fontSize: 13, color: 'var(--accent-orange)', overflow: 'auto'
                                }}>
                                    {JSON.stringify(shadow.delta, null, 2)}
                                </pre>
                            </div>
                        )}
                    </div>
                )}
            </div>
        </>
    );
}
