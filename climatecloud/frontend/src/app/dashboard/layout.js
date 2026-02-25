'use client';
import { useEffect } from 'react';
import { useRouter, usePathname } from 'next/navigation';
import Link from 'next/link';
import { useAuth } from '@/lib/auth-context';

const navItems = [
    { href: '/dashboard', label: 'Dashboard', icon: '📊' },
    { href: '/dashboard/devices', label: 'Devices', icon: '📡' },
    { href: '/dashboard/admin', label: 'Admin', icon: '⚙️', roles: ['owner', 'admin'] },
];

export default function DashboardLayout({ children }) {
    const { user, logout, isAuthenticated, loading } = useAuth();
    const router = useRouter();
    const pathname = usePathname();

    useEffect(() => {
        if (!loading && !isAuthenticated) router.push('/login');
    }, [isAuthenticated, loading, router]);

    if (loading || !isAuthenticated) {
        return (
            <div className="auth-page">
                <div style={{ textAlign: 'center' }}>
                    <div className="skeleton" style={{ width: 48, height: 48, borderRadius: 12, margin: '0 auto 16px' }} />
                    <div className="skeleton" style={{ width: 120, height: 20, margin: '0 auto' }} />
                </div>
            </div>
        );
    }

    return (
        <div className="app-layout">
            {/* Sidebar */}
            <aside className="sidebar">
                <div className="sidebar-header">
                    <Link href="/dashboard" className="sidebar-logo" style={{ textDecoration: 'none' }}>
                        <div className="sidebar-logo-icon">🌩</div>
                        <span className="sidebar-logo-text">ClimateCloud</span>
                    </Link>
                </div>

                <nav className="sidebar-nav">
                    {navItems.map(item => {
                        if (item.roles && !item.roles.includes(user?.role)) return null;
                        const isActive = pathname === item.href || (item.href !== '/dashboard' && pathname.startsWith(item.href));
                        return (
                            <Link key={item.href} href={item.href} className={`nav-item ${isActive ? 'active' : ''}`}>
                                <span style={{ fontSize: 18 }}>{item.icon}</span>
                                {item.label}
                            </Link>
                        );
                    })}
                </nav>

                <div className="sidebar-footer">
                    <div style={{ padding: '8px 14px', marginBottom: 8 }}>
                        <div style={{ fontSize: 14, fontWeight: 600 }}>{user?.firstName} {user?.lastName}</div>
                        <div style={{ fontSize: 12, color: 'var(--text-muted)' }}>{user?.email}</div>
                        <div style={{ fontSize: 11, color: 'var(--accent-blue)', marginTop: 4, textTransform: 'capitalize' }}>
                            {user?.role} • {user?.orgName}
                        </div>
                    </div>
                    <button onClick={logout} className="nav-item" style={{ color: 'var(--accent-red)' }}>
                        <span style={{ fontSize: 18 }}>🚪</span>
                        Sign Out
                    </button>
                </div>
            </aside>

            {/* Main Content */}
            <main className="main-content">
                {children}
            </main>
        </div>
    );
}
