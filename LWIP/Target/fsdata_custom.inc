/* ═══════════════════════════════════════════════════════════════════════════════
 * fsdata_custom.inc — Web filesystem data for LWIP HTTPD
 *
 * Pages:
 *   /index.shtml  — Main dashboard (SSI)
 *   /index.html   — alias to index.shtml data
 *   /log_view.html — Embedded log viewer (SSI)
 *   /scpi_ok.html — Post-SCPI redirect
 *   /scpi_done.html — Post-SCPI redirect (iframe target)
 *   /netcfg_ok.html — Config saved confirmation
 *   /netcfg_err.html — Config error
 *   /404.html     — Not found
 * ═════════════════════════════════════════════════════════════════════════════ */

/* ── Shared CSS ──────────────────────────────────────────────────────────── */
#define WEB_CSS \
    "*{margin:0;padding:0;box-sizing:border-box}\r\n" \
    "body{font-family:system-ui,-apple-system,sans-serif;background:#f5f6f8;color:#021521;min-height:100vh}\r\n" \
    ".page{max-width:1200px;margin:0 auto;padding:0 20px}\r\n" \
    ".header{background:#fff;padding:15px 0;border-bottom:1px solid #e8e8e8;margin-bottom:20px}\r\n" \
    ".header h1{font-size:1.3rem;color:#2f4f75;font-weight:600}\r\n" \
    ".header .sub{font-size:.82rem;color:#888;margin-top:2px}\r\n" \
    ".row{display:flex;gap:20px;margin-bottom:20px}\r\n" \
    ".card{background:#fff;border-radius:8px;box-shadow:0 1px 4px rgba(0,0,0,.06);padding:20px}\r\n" \
    ".card h2{font-size:1rem;color:#2f4f75;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #f0f0f0}\r\n" \
    ".card--wide{flex:1}\r\n" \
    ".card--side{width:440px;flex-shrink:0}\r\n" \
    "table{width:100%;border-collapse:collapse}\r\n" \
    "td{padding:6px 0;font-size:.88rem}\r\n" \
    "td.k{color:#888;width:100px}\r\n" \
    "td.v{color:#021521}\r\n" \
    ".tag{display:inline-block;background:#e8f0fa;color:#2f4f75;font-size:.72rem;padding:2px 8px;border-radius:3px;margin-right:4px}\r\n" \
    ".led{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px}\r\n" \
    ".led.green{background:#52c41a}\r\n" \
    ".led.yellow{background:#faad14}\r\n" \
    ".led.red{background:#e05040}\r\n" \
    ".toolbar{display:flex;gap:8px;margin-bottom:8px}\r\n" \
    ".toolbar input{flex:1;padding:6px 10px;border:1px solid #d9d9d9;border-radius:4px;font-size:.85rem;font-family:Consolas,monospace;outline:none}\r\n" \
    ".toolbar input:focus{border-color:#2f4f75}\r\n" \
    ".toolbar button{padding:6px 16px;border:none;border-radius:4px;cursor:pointer;font-size:.85rem;color:#fff}\r\n" \
    ".toolbar .btn-send{background:#2f4f75}\r\n" \
    ".toolbar .btn-send:hover{background:#3a6090}\r\n" \
    ".toolbar .btn-clear{background:#bbb}\r\n" \
    ".toolbar .btn-clear:hover{background:#999}\r\n" \
    ".log-frame{width:100%;height:380px;border:1px solid #e8e8e8;border-radius:4px;background:#fafbfc}\r\n" \
    "a{color:#2f4f75;text-decoration:none}\r\n" \
    "a:hover{text-decoration:underline}\r\n" \
    ".card .nav{display:flex;gap:15px;margin-bottom:8px}\r\n" \
    ".card .nav a{font-size:.85rem;padding:4px 8px;border-radius:4px}\r\n" \
    ".card .nav a.on{background:#2f4f75;color:#fff}\r\n" \
    ".sec{border:1px solid #e0e0e0;border-radius:6px;padding:12px;margin-bottom:14px}\r\n" \
    ".sec h2{margin-top:0!important;font-size:.9rem;margin-bottom:8px;padding-bottom:6px}\r\n" \
    ".footer{text-align:center;color:#bbb;font-size:.75rem;padding:20px 0}"

/* ── /index.shtml + /index.html (Dashboard) ─────────────────────────────── */

static const unsigned char data_index_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
    "<title>NetBus</title>\r\n"
    "<style>\r\n"
    WEB_CSS
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<div class=\"page\">\r\n"
    "<div class=\"header\">\r\n"
    "  <h1>NetBus Board Controller</h1>\r\n"
    "  <div class=\"sub\">"
    "<span class=\"tag\">STM32H743</span>"
    "<span class=\"tag\">v<!--#version--></span>"
    "<span class=\"tag\"><!--#uptime--></span>"
    "<span class=\"tag\"><!--#sysst--></span>"
    "</div>\r\n"
    "</div>\r\n"
    "<div class=\"row\">\r\n"
    "<div class=\"card card--wide\">\r\n"
    "  <h2>SCPI Console</h2>\r\n"
    "  <div class=\"toolbar\">\r\n"
    "    <form action=\"/scpi.cgi\" method=\"GET\" target=\"logf\" style=\"display:flex;flex:1;gap:8px\">\r\n"
    "      <input name=\"cmd\" id=\"cmd-input\" value=\"<!--#lastcmd-->\" placeholder=\"*IDN?\" autocomplete=\"off\">\r\n"
    "      <button type=\"submit\" class=\"btn-send\">Send</button>\r\n"
    "    </form>\r\n"
    "    <button class=\"btn-clear\" onclick=\"document.getElementById('cmd-input').value='';return false\">Clear</button>\r\n"
    "  </div>\r\n"
    "  <iframe src=\"/log_view.html\" name=\"logf\" class=\"log-frame\"></iframe>\r\n"
    "</div>\r\n"
    "<div class=\"card card--side\">\r\n"
    "  <div class=\"sec\">\r\n"
    "  <h2>System</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Status</td><td class=\"v\"><!--#sysst--></td></tr>\r\n"
    "    <tr><td class=\"k\">Firmware</td><td class=\"v\">v<!--#version--></td></tr>\r\n"
    "    <tr><td class=\"k\">Uptime</td><td class=\"v\"><!--#uptime--></td></tr>\r\n"
    "    <tr><td class=\"k\">Free Heap</td><td class=\"v\"><!--#heap--> B</td></tr>\r\n"
    "  </table>\r\n"
    "  </div>\r\n"
    "  <div class=\"sec\">\r\n"
    "  <h2>Network Config</h2>\r\n"
    "  <form action=\"/netcfg.cgi\" method=\"GET\">\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Link</td><td class=\"v\"><!--#link--></td></tr>\r\n"
    "    <tr><td class=\"k\">IP</td><td class=\"v\"><input name=\"ip\" value=\"<!--#ip-->\" style=\"width:140px;padding:3px 6px;border:1px solid #d9d9d9;border-radius:3px;font-size:.82rem;font-family:Consolas,monospace\"></td></tr>\r\n"
    "    <tr><td class=\"k\">Mask</td><td class=\"v\"><input name=\"mask\" value=\"<!--#mask-->\" style=\"width:140px;padding:3px 6px;border:1px solid #d9d9d9;border-radius:3px;font-size:.82rem;font-family:Consolas,monospace\"></td></tr>\r\n"
    "    <tr><td class=\"k\">Gateway</td><td class=\"v\"><input name=\"gw\" value=\"<!--#gw-->\" style=\"width:140px;padding:3px 6px;border:1px solid #d9d9d9;border-radius:3px;font-size:.82rem;font-family:Consolas,monospace\"></td></tr>\r\n"
    "    <tr><td class=\"k\">MAC</td><td class=\"v\"><input name=\"mac\" value=\"<!--#mac-->\" style=\"width:140px;padding:3px 6px;border:1px solid #d9d9d9;border-radius:3px;font-size:.82rem;font-family:Consolas,monospace\" placeholder=\"XX:XX:XX:XX:XX:XX\"></td></tr>\r\n"
    "    <tr><td class=\"k\">Port</td><td class=\"v\"><input name=\"port\" value=\"<!--#scpiport-->\" type=\"number\" style=\"width:80px;padding:3px 6px;border:1px solid #d9d9d9;border-radius:3px;font-size:.82rem\"></td></tr>\r\n"
    "  </table>\r\n"
    "  <input type=\"hidden\" name=\"reboot\" value=\"1\">\r\n"
    "  <p style=\"margin-top:6px;color:#888;font-size:.72rem\">Changes take effect after reboot.</p>\r\n"
    "  <button type=\"submit\" style=\"margin-top:4px;padding:4px 16px;background:#2f4f75;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:.8rem\">Save &amp; Reboot</button>\r\n"
    "  </form>\r\n"
    "  </div>\r\n"
    "  <div class=\"sec\">\r\n"
    "  <h2>RF</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Att A</td><td class=\"v\"><!--#att1--></td><td class=\"k\">Sw 1</td><td class=\"v\"><!--#rfsw1--></td></tr>\r\n"
    "    <tr><td class=\"k\">Att B</td><td class=\"v\"><!--#att2--></td><td class=\"k\">Sw 2</td><td class=\"v\"><!--#rfsw2--></td></tr>\r\n"
    "    <tr><td class=\"k\">Power H</td><td class=\"v\"><!--#det_h--></td><td class=\"k\">Power V</td><td class=\"v\"><!--#det_v--></td></tr>\r\n"
    "  </table>\r\n"
    "  </div>\r\n"
    "  <div class=\"sec\">\r\n"
    "  <h2>Storage</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Usage</td><td class=\"v\"><!--#storpct--></td></tr>\r\n"
    "    <tr><td class=\"k\">Free</td><td class=\"v\"><!--#storfr--> B</td></tr>\r\n"
    "  </table>\r\n"
    "  </div>\r\n"
    "</div>\r\n"
    "</div>\r\n"
    "<div class=\"footer\">NetBus</div>\r\n"
    "</div>\r\n"
    "</body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_index_html = {
    NULL,
    (const unsigned char *)"/index.html",
    data_index_html,
    sizeof(data_index_html),
    FS_FILE_FLAGS_SSI
};

static const struct fsdata_file file_index_shtml = {
    &file_index_html,
    (const unsigned char *)"/index.shtml",
    data_index_html,
    sizeof(data_index_html),
    FS_FILE_FLAGS_SSI
};

/* ── /log_view.html (embedded in iframe, auto-refreshes) ────────────────── */

static const unsigned char data_log_view_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<title>Log</title>\r\n"
    "<style>\r\n"
    "  *{margin:0;padding:0;box-sizing:border-box}\r\n"
    "  body{background:#fafbfc;color:#333;font-family:Consolas,monospace;font-size:12px;padding:6px;line-height:1.5}\r\n"
    "  .err{color:#e05040}\r\n"
    "  .warn{color:#d08000}\r\n"
    "  .info{color:#555}\r\n"
    "  pre{margin:0;white-space:pre-wrap;word-break:break-all}\r\n"
    "</style>\r\n"
    "</head>\r\n"
    "<body><pre><!--#logtext--></pre></body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_log_view_html = {
    &file_index_shtml,
    (const unsigned char *)"/log_view.html",
    data_log_view_html,
    sizeof(data_log_view_html),
    FS_FILE_FLAGS_SSI
};

/* ── /scpi_ok.html (redirect to dashboard) ──────────────────────────────── */

static const unsigned char data_scpi_ok_html[] =
    "<!DOCTYPE html>\r\n"
    "<html><head><meta charset=\"UTF-8\">"
    "<meta http-equiv=\"refresh\" content=\"0;url=/index.shtml\">"
    "</head><body></body></html>\r\n";

static const struct fsdata_file file_scpi_ok_html = {
    &file_log_view_html,
    (const unsigned char *)"/scpi_ok.html",
    data_scpi_ok_html,
    sizeof(data_scpi_ok_html),
    0
};

/* ── /scpi_done.html (redirect to log view, for iframe target) ──────────── */

static const unsigned char data_scpi_done_html[] =
    "<!DOCTYPE html>\r\n"
    "<html><head><meta charset=\"UTF-8\">"
    "<meta http-equiv=\"refresh\" content=\"1;url=/log_view.html\">"
    "</head><body style=\"background:#fafbfc;font-family:system-ui;padding:10px;color:#888;font-size:13px\">processing...</body></html>\r\n";

static const struct fsdata_file file_scpi_done_html = {
    &file_scpi_ok_html,
    (const unsigned char *)"/scpi_done.html",
    data_scpi_done_html,
    sizeof(data_scpi_done_html),
    0
};

/* ── /netcfg_ok.html ────────────────────────────────────────────────────── */

static const unsigned char data_netcfg_ok_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\"><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<meta http-equiv=\"refresh\" content=\"2;url=/index.shtml\">"
    "<title>Saved</title>\r\n"
    "<style>" WEB_CSS "</style></head>\r\n"
    "<body><div class=\"page\"><div class=\"card\" style=\"max-width:400px;margin:40px auto\">"
    "<h2>Settings Saved</h2><p>Configuration written to flash. Reboot to apply.</p>"
    "<p style=\"margin-top:8px\"><a href=\"/index.shtml\">Back</a> | <a href=\"/index.shtml\">Dashboard</a></p>"
    "<p style=\"color:#aaa;font-size:.75rem;margin-top:4px\">Redirecting...</p>"
    "</div></div></body></html>\r\n";

static const struct fsdata_file file_netcfg_ok_html = {
    &file_scpi_done_html,
    (const unsigned char *)"/netcfg_ok.html",
    data_netcfg_ok_html,
    sizeof(data_netcfg_ok_html),
    0
};

/* ── /netcfg_err.html ───────────────────────────────────────────────────── */

static const unsigned char data_netcfg_err_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\"><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<meta http-equiv=\"refresh\" content=\"2;url=/index.shtml\">"
    "<title>Error</title>\r\n"
    "<style>" WEB_CSS "</style></head>\r\n"
    "<body><div class=\"page\"><div class=\"card\" style=\"max-width:400px;margin:40px auto\">"
    "<h2>Invalid Input</h2><p>Check IP (x.x.x.x), MAC (XX:XX:XX:XX:XX:XX), and port (1-65535).</p>"
    "<p style=\"margin-top:8px\"><a href=\"/index.shtml\">Back</a></p>"
    "</div></div></body></html>\r\n";

static const struct fsdata_file file_netcfg_err_html = {
    &file_netcfg_ok_html,
    (const unsigned char *)"/netcfg_err.html",
    data_netcfg_err_html,
    sizeof(data_netcfg_err_html),
    0
};

/* ── /404.html ────────────────────────────────────────────────────────────── */

static const unsigned char data_404_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\"><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>404</title>\r\n"
    "<style>" WEB_CSS "</style></head>\r\n"
    "<body><div class=\"page\"><div class=\"card\" style=\"max-width:400px;margin:40px auto;text-align:center\">"
    "<h1 style=\"font-size:3rem;color:#e05040\">404</h1>"
    "<p>Page not found</p><p style=\"margin-top:8px\"><a href=\"/index.shtml\">Dashboard</a></p>"
    "</div></div></body></html>\r\n";

static const struct fsdata_file file_404_html = {
    &file_netcfg_err_html,
    (const unsigned char *)"/404.html",
    data_404_html,
    sizeof(data_404_html),
    0
};

#undef WEB_CSS
#define FS_ROOT &file_404_html

/* ═══════════════════════════════════════════════════════════════════════════════
 * LWIP default-page: "/" → "/index.shtml" (shtml=1 → SSI enabled)
 * ═════════════════════════════════════════════════════════════════════════════ */
