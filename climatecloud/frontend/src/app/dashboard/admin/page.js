'use client';
import { useState, useEffect, useCallback } from 'react';
import api from '@/lib/api';

export default function AdminPage() {
    const [users, setUsers] = useState([]);
    const [org, setOrg] = useState(null);
    const [loading, setLoading] = useState(true);
    const [showAdd, setShowAdd] = useState(false);
    const [newUser, setNewUser] = useState({ email: '', firstName: '', lastName: '', role: 'viewer', password: '' });
    const [error, setError] = useState('');

    const fetchData = useCallback(async () => {
        try {
            const [usersRes, orgRes] = await Promise.all([api.getUsers(), api.getOrganization()]);
            setUsers(usersRes.users);
            setOrg(orgRes.organization);
        } catch (err) { console.error(err); }
        finally { setLoading(false); }
    }, []);

    useEffect(() => { fetchData(); }, [fetchData]);

    const handleAdd = async (e) => {
        e.preventDefault(); setError('');
        try { await api.addUser(newUser); setShowAdd(false); setNewUser({ email: '', firstName: '', lastName: '', role: 'viewer', password: '' }); fetchData(); }
        catch (err) { setError(err.message); }
    };

    const handleRoleChange = async (userId, role) => {
        try { await api.updateUser(userId, { role }); fetchData(); }
        catch (err) { alert(err.message); }
    };

    const handleDelete = async (userId) => {
        if (!confirm('Delete this user?')) return;
        try { await api.deleteUser(userId); fetchData(); }
        catch (err) { alert(err.message); }
    };

    if (loading) return (<><div className="page-header"><div className="skeleton" style={{ width: 200, height: 28 }} /></div></>);

    return (
        <>
            <div className="page-header">
                <div>
                    <h1 className="page-title">Admin Console</h1>
                    <p className="page-subtitle">{org?.name} • {org?.plan} plan</p>
                </div>
                <button className="btn btn-primary" onClick={() => setShowAdd(true)}>+ Add User</button>
            </div>
            <div className="page-body animate-fade-in">
                {/* Org Stats */}
                <div className="card-grid" style={{ marginBottom: 24 }}>
                    <div className="stat-card">
                        <div className="stat-icon blue">👥</div>
                        <div className="stat-content">
                            <div className="stat-label">Users</div>
                            <div className="stat-value">{users.length}</div>
                        </div>
                    </div>
                    <div className="stat-card">
                        <div className="stat-icon device">🏢</div>
                        <div className="stat-content">
                            <div className="stat-label">Plan</div>
                            <div className="stat-value" style={{ fontSize: 20, textTransform: 'capitalize' }}>{org?.plan}</div>
                        </div>
                    </div>
                </div>

                {showAdd && (
                    <div className="card" style={{ marginBottom: 24 }}>
                        <div className="card-header">
                            <span className="card-title">Add User</span>
                            <button className="btn btn-secondary btn-sm" onClick={() => setShowAdd(false)}>Cancel</button>
                        </div>
                        {error && <div className="auth-error">{error}</div>}
                        <form onSubmit={handleAdd}>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12 }}>
                                <div className="form-group"><label className="form-label">First Name</label>
                                    <input className="form-input" value={newUser.firstName} onChange={e => setNewUser({ ...newUser, firstName: e.target.value })} required /></div>
                                <div className="form-group"><label className="form-label">Last Name</label>
                                    <input className="form-input" value={newUser.lastName} onChange={e => setNewUser({ ...newUser, lastName: e.target.value })} required /></div>
                            </div>
                            <div className="form-group"><label className="form-label">Email</label>
                                <input type="email" className="form-input" value={newUser.email} onChange={e => setNewUser({ ...newUser, email: e.target.value })} required /></div>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12 }}>
                                <div className="form-group"><label className="form-label">Role</label>
                                    <select className="form-input form-select" value={newUser.role} onChange={e => setNewUser({ ...newUser, role: e.target.value })}>
                                        <option value="admin">Admin</option><option value="technician">Technician</option><option value="viewer">Viewer</option>
                                    </select></div>
                                <div className="form-group"><label className="form-label">Temporary Password</label>
                                    <input type="password" className="form-input" value={newUser.password} onChange={e => setNewUser({ ...newUser, password: e.target.value })} required minLength={8} /></div>
                            </div>
                            <button type="submit" className="btn btn-primary">Add User</button>
                        </form>
                    </div>
                )}

                <div className="table-container">
                    <table>
                        <thead><tr><th>User</th><th>Email</th><th>Role</th><th>Status</th><th>Last Login</th><th>Actions</th></tr></thead>
                        <tbody>
                            {users.map(u => (
                                <tr key={u.id}>
                                    <td style={{ fontWeight: 600 }}>{u.firstName} {u.lastName}</td>
                                    <td>{u.email}</td>
                                    <td>
                                        {u.role === 'owner' ? <span style={{ color: 'var(--accent-purple)', fontWeight: 600, textTransform: 'capitalize' }}>{u.role}</span> : (
                                            <select className="form-input form-select" value={u.role} onChange={e => handleRoleChange(u.id, e.target.value)}
                                                style={{ padding: '4px 24px 4px 8px', width: 'auto', fontSize: 13 }}>
                                                <option value="admin">Admin</option><option value="technician">Technician</option><option value="viewer">Viewer</option>
                                            </select>
                                        )}
                                    </td>
                                    <td><span className={`status-badge ${u.isActive ? 'online' : 'offline'}`}>{u.isActive ? 'Active' : 'Inactive'}</span></td>
                                    <td style={{ fontSize: 13, color: 'var(--text-muted)' }}>{u.lastLogin ? new Date(u.lastLogin).toLocaleString() : 'Never'}</td>
                                    <td>{u.role !== 'owner' && <button className="btn btn-danger btn-sm" onClick={() => handleDelete(u.id)}>Remove</button>}</td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </div>
            </div>
        </>
    );
}
