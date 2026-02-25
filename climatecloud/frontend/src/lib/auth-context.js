'use client';
import { createContext, useContext, useState, useEffect, useCallback } from 'react';
import api from '@/lib/api';

const AuthContext = createContext(null);

export function AuthProvider({ children }) {
    const [user, setUser] = useState(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        const stored = typeof window !== 'undefined' ? localStorage.getItem('cc_user') : null;
        if (stored) {
            try { setUser(JSON.parse(stored)); } catch { }
        }
        setLoading(false);
    }, []);

    const login = useCallback(async (email, password) => {
        const res = await api.login(email, password);
        setUser(res.user);
        return res;
    }, []);

    const register = useCallback(async (data) => {
        const res = await api.register(data);
        setUser(res.user);
        return res;
    }, []);

    const logout = useCallback(async () => {
        await api.logout();
        setUser(null);
    }, []);

    return (
        <AuthContext.Provider value={{ user, login, register, logout, loading, isAuthenticated: !!user }}>
            {children}
        </AuthContext.Provider>
    );
}

export function useAuth() {
    const ctx = useContext(AuthContext);
    if (!ctx) throw new Error('useAuth must be used within AuthProvider');
    return ctx;
}
