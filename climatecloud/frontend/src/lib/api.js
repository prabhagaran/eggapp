const API_URL = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:3001';

class ApiClient {
    constructor() {
        this.baseUrl = API_URL;
        this.accessToken = null;
        this.refreshToken = null;
        if (typeof window !== 'undefined') {
            this.accessToken = localStorage.getItem('cc_access_token');
            this.refreshToken = localStorage.getItem('cc_refresh_token');
        }
    }

    setTokens(access, refresh) {
        this.accessToken = access;
        this.refreshToken = refresh;
        if (typeof window !== 'undefined') {
            localStorage.setItem('cc_access_token', access);
            localStorage.setItem('cc_refresh_token', refresh);
        }
    }

    clearTokens() {
        this.accessToken = null;
        this.refreshToken = null;
        if (typeof window !== 'undefined') {
            localStorage.removeItem('cc_access_token');
            localStorage.removeItem('cc_refresh_token');
            localStorage.removeItem('cc_user');
        }
    }

    async request(path, options = {}) {
        const url = `${this.baseUrl}${path}`;
        const headers = { 'Content-Type': 'application/json', ...options.headers };
        if (this.accessToken) headers['Authorization'] = `Bearer ${this.accessToken}`;

        let res = await fetch(url, { ...options, headers });

        // Auto-refresh on 401
        if (res.status === 401 && this.refreshToken) {
            const refreshed = await this.doRefresh();
            if (refreshed) {
                headers['Authorization'] = `Bearer ${this.accessToken}`;
                res = await fetch(url, { ...options, headers });
            } else {
                this.clearTokens();
                if (typeof window !== 'undefined') window.location.href = '/login';
                throw new Error('Session expired');
            }
        }

        const data = await res.json();
        if (!res.ok) throw new Error(data.error || 'Request failed');
        return data;
    }

    async doRefresh() {
        try {
            const res = await fetch(`${this.baseUrl}/auth/refresh`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ refreshToken: this.refreshToken }),
            });
            if (!res.ok) return false;
            const data = await res.json();
            this.setTokens(data.accessToken, data.refreshToken);
            return true;
        } catch { return false; }
    }

    // Auth
    async register(data) {
        const res = await this.request('/auth/register', { method: 'POST', body: JSON.stringify(data) });
        this.setTokens(res.accessToken, res.refreshToken);
        if (typeof window !== 'undefined') localStorage.setItem('cc_user', JSON.stringify(res.user));
        return res;
    }

    async login(email, password) {
        const res = await this.request('/auth/login', { method: 'POST', body: JSON.stringify({ email, password }) });
        this.setTokens(res.accessToken, res.refreshToken);
        if (typeof window !== 'undefined') localStorage.setItem('cc_user', JSON.stringify(res.user));
        return res;
    }

    async logout() {
        try { await this.request('/auth/logout', { method: 'POST' }); } catch { }
        this.clearTokens();
    }

    async getMe() { return this.request('/auth/me'); }

    // Devices
    async getDevices() { return this.request('/api/devices'); }
    async getDevice(id) { return this.request(`/api/devices/${id}`); }
    async registerDevice(data) { return this.request('/api/devices', { method: 'POST', body: JSON.stringify(data) }); }
    async updateDevice(id, data) { return this.request(`/api/devices/${id}`, { method: 'PATCH', body: JSON.stringify(data) }); }
    async deleteDevice(id) { return this.request(`/api/devices/${id}`, { method: 'DELETE' }); }

    // Telemetry
    async getLatestTelemetry(deviceId) { return this.request(`/api/devices/${deviceId}/telemetry/latest`); }
    async getTelemetry(deviceId, params = {}) {
        const qs = new URLSearchParams(params).toString();
        return this.request(`/api/devices/${deviceId}/telemetry${qs ? '?' + qs : ''}`);
    }
    async getAggregatedTelemetry(deviceId, interval = '24h') {
        return this.request(`/api/devices/${deviceId}/telemetry/aggregate?interval=${interval}`);
    }

    // Dashboard
    async getDashboardSummary() { return this.request('/api/dashboard/summary'); }

    // Commands
    async sendCommand(deviceId, data) {
        return this.request(`/api/devices/${deviceId}/commands`, { method: 'POST', body: JSON.stringify(data) });
    }
    async getCommands(deviceId) { return this.request(`/api/devices/${deviceId}/commands`); }

    // Shadow
    async getShadow(deviceId) { return this.request(`/api/devices/${deviceId}/shadow`); }
    async updateShadowDesired(deviceId, desired) {
        return this.request(`/api/devices/${deviceId}/shadow/desired`, { method: 'PUT', body: JSON.stringify({ desired }) });
    }

    // Admin
    async getUsers() { return this.request('/api/admin/users'); }
    async addUser(data) { return this.request('/api/admin/users', { method: 'POST', body: JSON.stringify(data) }); }
    async updateUser(id, data) { return this.request(`/api/admin/users/${id}`, { method: 'PATCH', body: JSON.stringify(data) }); }
    async deleteUser(id) { return this.request(`/api/admin/users/${id}`, { method: 'DELETE' }); }
    async getOrganization() { return this.request('/api/admin/organization'); }
}

export const api = new ApiClient();
export default api;
