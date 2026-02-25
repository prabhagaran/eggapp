'use client';
import { useEffect } from 'react';
import { useRouter } from 'next/navigation';
import { useAuth } from '@/lib/auth-context';

export default function Home() {
    const router = useRouter();
    const { isAuthenticated, loading } = useAuth();

    useEffect(() => {
        if (!loading) {
            router.push(isAuthenticated ? '/dashboard' : '/login');
        }
    }, [isAuthenticated, loading, router]);

    return (
        <div className="auth-page">
            <div style={{ textAlign: 'center' }}>
                <div className="sidebar-logo-icon" style={{ width: 64, height: 64, fontSize: 32, margin: '0 auto 16px', borderRadius: 16 }}>🌩</div>
                <div className="sidebar-logo-text" style={{ fontSize: 32 }}>ClimateCloud</div>
                <p style={{ color: 'var(--text-secondary)', marginTop: 8 }}>Loading...</p>
            </div>
        </div>
    );
}
