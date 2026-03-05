How to extract the Root CA PEM for script.google.com

Option A — Using OpenSSL (Linux, macOS, or Windows with OpenSSL installed)
1. Run:

   openssl s_client -showcerts -connect script.google.com:443 </dev/null

2. The output contains one or more certificate blocks. Copy the full PEM for the root CA you need (the last cert in the chain is typically the root). Each block starts with "-----BEGIN CERTIFICATE-----" and ends with "-----END CERTIFICATE-----".
3. Paste the PEM into `secrets.h` as a C string literal (preserve newlines) so it can be passed to `WiFiClientSecure::setCACert()`.

Option B — Export from Chrome (Windows)
1. Open https://script.google.com in Chrome.
2. Click the lock icon → "Connection is secure" → "Certificate is valid" → "Details".
3. In the Certificate dialog, go to the Certification Path, select the root certificate (top of chain), click "View Certificate" → "Details" → "Copy to File..." and export in Base-64 encoded X.509 (.CER).
4. Open the exported `.cer` file in a text editor — it will already be PEM (BEGIN/END lines). Copy and paste into `secrets.h`.

Notes and tips
- Prefer the exact root that the server presents. For Google-hosted services the chain usually roots to a Google Trust Services / ISRG root.
- In `secrets.h` wrap the PEM as a C string like:

  static const char CLOUD_ROOT_CA[] = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n";

- After adding the PEM, remove any calls to `client.setInsecure()` and use `client.setCACert(CLOUD_ROOT_CA)`.
- If you want, paste the PEM here and I will insert it into `secrets.h` and recompile the project.
