// Browser API client: token storage, automatic refresh-once-on-401, and
// farm selection. Used only from client components.
const BASE = process.env.NEXT_PUBLIC_API_URL ?? "http://localhost:3001";

export class ApiError extends Error {
  constructor(
    public readonly status: number,
    public readonly code: string,
    message: string,
  ) {
    super(message);
  }
}

export function setTokens(access: string, refresh: string) {
  localStorage.setItem("accessToken", access);
  localStorage.setItem("refreshToken", refresh);
}

export function clearAuth() {
  localStorage.removeItem("accessToken");
  localStorage.removeItem("refreshToken");
  localStorage.removeItem("farmId");
}

export function hasToken() {
  return typeof window !== "undefined" && Boolean(localStorage.getItem("accessToken"));
}

export function setFarmId(id: string) {
  localStorage.setItem("farmId", id);
}

export function getFarmId(): string | null {
  return localStorage.getItem("farmId");
}

async function tryRefresh(): Promise<boolean> {
  const refreshToken = localStorage.getItem("refreshToken");
  if (!refreshToken) return false;
  const res = await fetch(`${BASE}/v1/auth/refresh`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ refreshToken }),
  });
  if (!res.ok) return false;
  const data = await res.json();
  setTokens(data.accessToken, data.refreshToken);
  return true;
}

// CSV export — a plain <a href> can't carry the Bearer token, so this
// fetches as a blob with the auth header and triggers a client-side
// download instead. Simplified vs api(): no refresh-on-401 retry, since
// a mid-export token expiry is rare and re-clicking after a refresh
// elsewhere is an acceptable fallback for an export action.
export async function downloadCsv(path: string, filename: string) {
  const res = await fetch(`${BASE}${path}`, {
    headers: localStorage.getItem("accessToken") ? { authorization: `Bearer ${localStorage.getItem("accessToken")}` } : {},
  });
  if (!res.ok) throw new ApiError(res.status, "export_failed", "Export failed");
  const blob = await res.blob();
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

export async function api<T>(
  path: string,
  options: { method?: string; body?: unknown; auth?: boolean } = {},
): Promise<T> {
  const { method = "GET", body, auth = true } = options;
  const doFetch = () =>
    fetch(`${BASE}${path}`, {
      method,
      headers: {
        ...(body !== undefined ? { "content-type": "application/json" } : {}),
        ...(auth && localStorage.getItem("accessToken")
          ? { authorization: `Bearer ${localStorage.getItem("accessToken")}` }
          : {}),
      },
      body: body !== undefined ? JSON.stringify(body) : undefined,
    });

  let res = await doFetch();
  if (res.status === 401 && auth) {
    if (await tryRefresh()) {
      res = await doFetch();
    } else {
      clearAuth();
      window.location.href = "/login";
      throw new ApiError(401, "unauthorized", "Session expired");
    }
  }
  if (res.status === 204) return undefined as T;
  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    throw new ApiError(res.status, data?.error?.code ?? "error", data?.error?.message ?? res.statusText);
  }
  return data as T;
}
