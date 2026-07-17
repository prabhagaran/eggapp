import type { Metadata } from "next";
import type { ReactNode } from "react";
import { NavBar } from "../components/NavBar";
import "./globals.css";

export const metadata: Metadata = {
  title: "eggAPP — Farm Dashboard",
  description: "Smart Poultry Farm Management — admin/manager dashboard",
};

export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en">
      <body>
        <NavBar />
        <main>{children}</main>
      </body>
    </html>
  );
}
