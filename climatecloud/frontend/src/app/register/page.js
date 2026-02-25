'use client';
import { useState } from 'react';
import { useRouter } from 'next/navigation';
import { useAuth } from '@/lib/auth-context';
import Link from 'next/link';

export default function RegisterPage() {
    const [form, setForm] = useState({ email: '', password: '', firstName: '', lastName: '', orgName: '' });
    const [error, setError] = useState('');
    const [loading, setLoading] = useState(false);
    const { register } = useAuth();
    const router = useRouter();

    const handleChange = (e) => setForm({ ...form, [e.target.name]: e.target.value });

    const handleSubmit = async (e) => {
        e.preventDefault();
        setError('');
        setLoading(true);
        try {
            await register(form);
            router.push('/dashboard');
        } catch (err) {
            setError(err.message || 'Registration failed');
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="auth-page">
            <div className="auth-card animate-fade-in">
                <div style={{ textAlign: 'center', marginBottom: 32 }}>
                    <div className="sidebar-logo-icon" style={{ width: 56, height: 56, fontSize: 28, margin: '0 auto 16px', borderRadius: 14 }}>🌩</div>
                    <h1 className="auth-title">Create Account</h1>
                    <p className="auth-subtitle">Start monitoring your IoT devices</p>
                </div>

                {error && <div className="auth-error">{error}</div>}

                <form onSubmit={handleSubmit}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12 }}>
                        <div className="form-group">
                            <label className="form-label" htmlFor="firstName">First Name</label>
                            <input id="firstName" name="firstName" className="form-input" placeholder="John"
                                value={form.firstName} onChange={handleChange} required />
                        </div>
                        <div className="form-group">
                            <label className="form-label" htmlFor="lastName">Last Name</label>
                            <input id="lastName" name="lastName" className="form-input" placeholder="Doe"
                                value={form.lastName} onChange={handleChange} required />
                        </div>
                    </div>
                    <div className="form-group">
                        <label className="form-label" htmlFor="orgName">Organization Name</label>
                        <input id="orgName" name="orgName" className="form-input" placeholder="My Farm Co."
                            value={form.orgName} onChange={handleChange} required />
                    </div>
                    <div className="form-group">
                        <label className="form-label" htmlFor="regEmail">Email Address</label>
                        <input id="regEmail" name="email" type="email" className="form-input" placeholder="you@company.com"
                            value={form.email} onChange={handleChange} required />
                    </div>
                    <div className="form-group">
                        <label className="form-label" htmlFor="regPassword">Password</label>
                        <input id="regPassword" name="password" type="password" className="form-input" placeholder="Min. 8 characters"
                            value={form.password} onChange={handleChange} required minLength={8} />
                    </div>
                    <button type="submit" className="btn btn-primary btn-lg" style={{ width: '100%' }} disabled={loading}>
                        {loading ? 'Creating account...' : 'Create Account'}
                    </button>
                </form>

                <div className="auth-link">
                    Already have an account? <Link href="/login">Sign in</Link>
                </div>
            </div>
        </div>
    );
}
