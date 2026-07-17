import type { Metadata } from "next";
import type { ReactNode } from "react";

export const metadata: Metadata = {
  title: "eggAPP — Farm Dashboard",
  description: "Smart Poultry Farm Management — admin/manager dashboard",
};

export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en">
      <body style={{ fontFamily: "system-ui, sans-serif", margin: "2rem" }}>{children}</body>
    </html>
  );
}
