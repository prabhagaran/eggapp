'use client';
import { useState, useEffect, useCallback } from 'react';
import Link from 'next/link';
import api from '@/lib/api';

export default function DevicesPage() {
    const [devices, setDevices] = useState([]);
    const [loading, setLoading] = useState(true);
    const [showAddDialog, setShowAddDialog] = useState(false);
    const [newDevice, setNewDevice] = useState({ name: '', type: 'egg_incubator', deviceUid: '' });
    const [credentials, setCredentials] = useState(null);
    const [error, setError] = useState('');

    const fetchDevices = useCallback(async () => {
        try {
            const res = await api.getDevices();
            setDevices(res.devices);
        } catch (err) { console.error(err); }
        finally { setLoading(false); }
    }, []);

    useEffect(() => { fetchDevices(); }, [fetchDevices]);

    const handleAddDevice = async (e) => {
        e.preventDefault();
        setError('');
        try {
            const res = await api.registerDevice(newDevice);
            setCredentials(res.credentials);
            setNewDevice({ name: '', type: 'egg_incubator', deviceUid: '' });
            fetchDevices();
        } catch (err) { setError(err.message); }
    };

    const handleDelete = async (id) => {
        if (!confirm('Delete this device? This cannot be undone.')) return;
        try { await api.deleteDevice(id); fetchDevices(); }
        catch (err) { alert(err.message); }
    };

    return (
        <>
            <div className="page-header">
                <div>
                    <h1 className="page-title">Devices</h1>
                    <p className="page-subtitle">Manage your IoT devices</p>
                </div>
                <button className="btn btn-primary" onClick={() => { setShowAddDialog(true); setCredentials(null); }}>
                    + Add Device
                </button>
            </div>

            <div className="page-body animate-fade-in">
                {/* Add Device Dialog */}
                {showAddDialog && (
                    <div className="card" style={{ marginBottom: 24 }}>
                        <div className="card-header">
                            <span className="card-title">{credentials ? '✅ Device Registered' : '📡 Register New Device'}</span>
                            <button className="btn btn-secondary btn-sm" onClick={() => setShowAddDialog(false)}>Close</button>
                        </div>

                        {credentials ? (
                            <div>
                                <div style={{
                                    background: 'var(--accent-orange-glow)', border: '1px solid rgba(245,158,11,0.3)',
                                    borderRadius: 'var(--radius-md)', padding: 16, marginBottom: 16
                                }}>
                                    <strong style={{ color: 'var(--accent-orange)' }}>⚠️ Save these credentials — they won&apos;t be shown again!</strong>
                                </div>
                                <div style={{ background: 'var(--bg-input)', padding: 16, borderRadius: 'var(--radius-md)', fontFamily: 'monospace', fontSize: 13 }}>
                                    <div><strong>API Key:</strong> {credentials.apiKey}</div>
                                    <div style={{ marginTop: 8 }}><strong>MQTT Username:</strong> {credentials.mqttUsername}</div>
                                    <div style={{ marginTop: 8 }}><strong>MQTT Password:</strong> {credentials.mqttPassword}</div>
                                    <div style={{ marginTop: 8 }}><strong>MQTT Broker:</strong> {credentials.mqttBrokerUrl}</div>
                                </div>
                            </div>
                        ) : (
                            <form onSubmit={handleAddDevice}>
                                {error && <div className="auth-error">{error}</div>}
                                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 16 }}>
                                    <div className="form-group">
                                        <label className="form-label">Device Name</label>
                                        <input className="form-input" placeholder="Incubator #1" value={newDevice.name}
                                            onChange={e => setNewDevice({ ...newDevice, name: e.target.value })} required />
                                    </div>
                                    <div className="form-group">
                                        <label className="form-label">Type</label>
                                        <select className="form-input form-select" value={newDevice.type}
                                            onChange={e => setNewDevice({ ...newDevice, type: e.target.value })}>
                                            <option value="egg_incubator">Egg Incubator</option>
                                            <option value="climate_chamber">Climate Chamber</option>
                                            <option value="thermostat">Thermostat</option>
                                            <option value="generic">Generic</option>
                                        </select>
                                    </div>
                                    <div className="form-group">
                                        <label className="form-label">Device UID (optional)</label>
                                        <input className="form-input" placeholder="Auto-generated" value={newDevice.deviceUid}
                                            onChange={e => setNewDevice({ ...newDevice, deviceUid: e.target.value })} />
                                    </div>
                                </div>
                                <button type="submit" className="btn btn-primary">Register Device</button>
                            </form>
                        )}
                    </div>
                )}

                {/* Device Table */}
                {loading ? (
                    <div>{[1, 2, 3].map(i => <div key={i} className="skeleton" style={{ height: 56, marginBottom: 8, borderRadius: 8 }} />)}</div>
                ) : devices.length === 0 ? (
                    <div className="empty-state">
                        <div className="empty-state-icon">📡</div>
                        <div className="empty-state-title">No devices registered</div>
                        <div className="empty-state-text">Click &quot;Add Device&quot; to register your first device.</div>
                    </div>
                ) : (
                    <div className="table-container">
                        <table>
                            <thead>
                                <tr>
                                    <th>Device</th>
                                    <th>UID</th>
                                    <th>Type</th>
                                    <th>Status</th>
                                    <th>Firmware</th>
                                    <th>Last Seen</th>
                                    <th>Actions</th>
                                </tr>
                            </thead>
                            <tbody>
                                {devices.map(d => (
                                    <tr key={d.id}>
                                        <td><Link href={`/dashboard/devices/${d.id}`} style={{ color: 'var(--accent-blue)', textDecoration: 'none', fontWeight: 600 }}>{d.name}</Link></td>
                                        <td><code style={{ fontSize: 12, color: 'var(--text-muted)' }}>{d.deviceUid}</code></td>
                                        <td style={{ textTransform: 'capitalize' }}>{d.type?.replace('_', ' ')}</td>
                                        <td><span className={`status-badge ${d.isOnline ? 'online' : 'offline'}`}><span className="status-dot" />{d.isOnline ? 'Online' : 'Offline'}</span></td>
                                        <td>{d.fwVersion || '—'}</td>
                                        <td style={{ fontSize: 13, color: 'var(--text-muted)' }}>{d.lastSeen ? new Date(d.lastSeen).toLocaleString() : 'Never'}</td>
                                        <td>
                                            <button className="btn btn-danger btn-sm" onClick={() => handleDelete(d.id)}>Delete</button>
                                        </td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    </div>
                )}
            </div>
        </>
    );
}
