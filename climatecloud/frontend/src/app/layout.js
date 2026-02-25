import './globals.css';
import { AuthProvider } from '@/lib/auth-context';

export const metadata = {
    title: 'ClimateCloud — IoT Device Management Platform',
    description: 'Production-grade IoT SaaS platform for climate control devices. Real-time monitoring, remote control, and device management.',
    keywords: ['IoT', 'climate control', 'incubator', 'MQTT', 'dashboard', 'SaaS'],
};

export default function RootLayout({ children }) {
    return (
        <html lang="en">
            <body>
                <AuthProvider>{children}</AuthProvider>
            </body>
        </html>
    );
}
