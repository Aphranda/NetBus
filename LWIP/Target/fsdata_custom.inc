/* ═══════════════════════════════════════════════════════════════════════════════
 * fsdata_custom.inc — Web filesystem data for LWIP HTTPD
 *
 * This file is #include'd by fs.c (via HTTPD_FSDATA_FILE macro when
 * HTTPD_USE_CUSTOM_FSDATA = 1). It defines FS_ROOT as the head of a
 * linked list of static file entries.
 *
 * SSI tags: <!--#uptime--> <!--#version--> <!--#sysst--> <!--#heap-->
 *   <!--#ip--> <!--#mask--> <!--#gw--> <!--#mac--> <!--#link-->
 *   <!--#storfr--> <!--#storpct--> <!--#storwr--> <!--#storrec-->
 *   <!--#modst-->
 *
 * Adding a new file:
 *   1. Declare the byte array (data_<path>)
 *   2. Declare the fsdata_file struct (file_<path>), chaining it to
 *      the previously-defined file via the `next` pointer
 *   3. Update FS_ROOT to point to the newest file
 * ═════════════════════════════════════════════════════════════════════════════ */

/* ── Shared CSS (kept inline for HTTP/1.1 without link support) ──────────── */
#define WEB_CSS \
    "*{margin:0;padding:0;box-sizing:border-box}\r\n" \
    "body{font-family:system-ui,-apple-system,sans-serif;background:#0f1923;" \
      "color:#c0d0e0;min-height:100vh;display:flex;flex-direction:column;" \
      "align-items:center;padding:1.5rem}\r\n" \
    ".card{background:#182530;border:1px solid #253545;border-radius:10px;" \
      "padding:1.5rem;margin:.75rem 0;width:100%;max-width:640px;" \
      "box-shadow:0 4px 24px rgba(0,0,0,.3)}\r\n" \
    "h1{font-size:1.4rem;font-weight:600;color:#e0f0ff;margin-bottom:.2rem}\r\n" \
    "h2{font-size:1rem;font-weight:500;color:#90b8e0;margin-bottom:.75rem;" \
      "border-bottom:1px solid #253545;padding-bottom:.4rem}\r\n" \
    ".tag{display:inline-block;background:#1a3a50;color:#60c0ff;font-size:.7rem;" \
      "padding:.15rem .5rem;border-radius:4px;margin-right:.3rem;margin-top:.3rem}\r\n" \
    "table{width:100%;border-collapse:collapse}\r\n" \
    "td{padding:.4rem .6rem;border-bottom:1px solid #1e2e3c;font-size:.88rem}\r\n" \
    "td.k{color:#7099b8;width:38%}\r\n" \
    "td.v{color:#d0e0f0}\r\n" \
    "th{text-align:left;padding:.4rem .6rem;color:#6088a8;font-size:.78rem;" \
      "font-weight:500;text-transform:uppercase;letter-spacing:.05em;" \
      "border-bottom:1px solid #304050}\r\n" \
    ".led{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:.35rem}\r\n" \
    ".led.green{background:#40d060;box-shadow:0 0 6px #40d060}\r\n" \
    ".led.yellow{background:#e0b040;box-shadow:0 0 6px #e0b040}\r\n" \
    ".led.red{background:#e05040;box-shadow:0 0 6px #e05040}\r\n" \
    ".bar{width:100%;height:8px;background:#1e2e3c;border-radius:4px;overflow:hidden}\r\n" \
    ".bar>div{height:100%;background:#3080c0;border-radius:4px;transition:width .5s}\r\n" \
    ".nav{display:flex;gap:1rem;margin-bottom:.5rem}\r\n" \
    ".nav a{color:#7099b8;text-decoration:none;font-size:.85rem;padding:.3rem .6rem;" \
      "border-radius:4px;transition:background .2s}\r\n" \
    ".nav a:hover,.nav a.on{background:#1a3a50;color:#90c0e0}\r\n" \
    ".footer{text-align:center;color:#405060;font-size:.75rem;margin-top:1rem}\r\n" \
    ".muted{color:#506070;font-size:.8rem}\r\n" \
    "a{color:#60b0e0}\r\n" \
    ".main{display:flex;gap:1rem;width:100%;max-width:1100px;align-items:flex-start}\r\n" \
    ".left{flex:1;min-width:0}\r\n" \
    ".right{width:380px;flex-shrink:0;position:sticky;top:1rem}\r\n" \
    ".right .card{padding:1rem}\r\n" \
    "textarea{width:100%;background:#0a1018;color:#90c0d0;border:1px solid #253545;" \
      "border-radius:6px;padding:.5rem;font-family:Consolas,monospace;font-size:.78rem;" \
      "resize:vertical;height:360px}\r\n" \
    ".scpi-form{display:flex;gap:.4rem}\r\n" \
    ".scpi-form input{flex:1;padding:.4rem .5rem;background:#0a1018;border:1px solid #253545;" \
      "border-radius:6px;color:#d0e0f0;font-family:Consolas,monospace;font-size:.82rem}\r\n" \
    ".scpi-form button{padding:.4rem .8rem;background:#205080;color:#d0e0f0;border:none;" \
      "border-radius:6px;cursor:pointer;font-size:.82rem;white-space:nowrap}\r\n" \
    ".scpi-form button:hover{background:#2868a0}\r\n" \
    ".btn-sm{padding:.2rem .6rem;background:#205080;color:#d0e0f0;border:none;" \
      "border-radius:4px;cursor:pointer;font-size:.72rem;text-decoration:none;" \
      "margin-left:.5rem;vertical-align:middle}\r\n" \
    ".btn-sm:hover{background:#2868a0}\r\n" \
    ".log-frame{width:100%;height:360px;border:1px solid #253545;border-radius:6px;" \
      "background:#0a1018}\r\n" \
    "pre.result{background:#0a1018;color:#90c0d0;padding:.6rem;border-radius:6px;" \
      "border:1px solid #253545;font-family:Consolas,monospace;font-size:.78rem;" \
      "white-space:pre-wrap;word-break:break-all;max-height:150px;overflow:auto}"

/* ── /index.html (Dashboard) ──────────────────────────────────────────────── */

static const unsigned char data_index_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
    "<title>NetBus Dashboard</title>\r\n"
    "<style>\r\n"
    WEB_CSS
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<div class=\"main\">\r\n"
    "<div class=\"left\">\r\n"
    "<div class=\"card\">\r\n"
    "  <h1>NetBus</h1>\r\n"
    "  <span class=\"tag\">STM32H743ZI</span>\r\n"
    "  <span class=\"tag\">480 MHz Cortex-M7</span>\r\n"
    "  <span class=\"tag\">FreeRTOS + LwIP</span>\r\n"
    "  <span class=\"tag\">v<!--#version--></span>\r\n"
    "  <div class=\"nav\">\r\n"
    "    <a href=\"/index.shtml\" class=\"on\">Dashboard</a>\r\n"
    "    <a href=\"/netcfg.html\">Network</a>\r\n"
    "  </div>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>System</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Status</td><td class=\"v\"><!--#sysst--></td></tr>\r\n"
    "    <tr><td class=\"k\">Firmware</td><td class=\"v\">v<!--#version--></td></tr>\r\n"
    "    <tr><td class=\"k\">Uptime</td><td class=\"v\"><!--#uptime--></td></tr>\r\n"
    "    <tr><td class=\"k\">Free Heap</td><td class=\"v\"><!--#heap--> B</td></tr>\r\n"
    "  </table>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>Network</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Link</td><td class=\"v\"><!--#link--></td></tr>\r\n"
    "    <tr><td class=\"k\">IP Address</td><td class=\"v\"><!--#ip--></td></tr>\r\n"
    "    <tr><td class=\"k\">Netmask</td><td class=\"v\"><!--#mask--></td></tr>\r\n"
    "    <tr><td class=\"k\">Gateway</td><td class=\"v\"><!--#gw--></td></tr>\r\n"
    "    <tr><td class=\"k\">MAC</td><td class=\"v\"><!--#mac--></td></tr>\r\n"
    "  </table>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>Storage</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Usage</td><td class=\"v\"><!--#storpct--></td></tr>\r\n"
    "    <tr><td class=\"k\">Free</td><td class=\"v\"><!--#storfr--> B</td></tr>\r\n"
    "    <tr><td class=\"k\">Writes</td><td class=\"v\"><!--#storwr--></td></tr>\r\n"
    "    <tr><td class=\"k\">Records</td><td class=\"v\"><!--#storrec--></td></tr>\r\n"
    "  </table>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>RF Parameters</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Attenuator A</td><td class=\"v\"><!--#att1--></td></tr>\r\n"
    "    <tr><td class=\"k\">Attenuator B</td><td class=\"v\"><!--#att2--></td></tr>\r\n"
    "    <tr><td class=\"k\">RF Switch 1</td><td class=\"v\"><!--#rfsw1--></td></tr>\r\n"
    "    <tr><td class=\"k\">RF Switch 2</td><td class=\"v\"><!--#rfsw2--></td></tr>\r\n"
    "    <tr><td class=\"k\">Power H</td><td class=\"v\"><!--#det_h--></td></tr>\r\n"
    "    <tr><td class=\"k\">Power V</td><td class=\"v\"><!--#det_v--></td></tr>\r\n"
    "  </table>\r\n"
    "</div>\r\n"
    "</div>\r\n"
    "<div class=\"right\">\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>SCPI Console</h2>\r\n"
    "  <form class=\"scpi-form\" action=\"/scpi.cgi\" method=\"GET\" target=\"logf\">\r\n"
    "    <input name=\"cmd\" value=\"<!--#lastcmd-->\" placeholder=\"*IDN?\" autocomplete=\"off\">\r\n"
    "    <button type=\"submit\">Send</button>\r\n"
    "  </form>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>System Log <a href=\"/log_clear.cgi\" class=\"btn-sm\" target=\"logf\">Clear</a></h2>\r\n"
    "  <iframe src=\"/log_view.html\" name=\"logf\" class=\"log-frame\"></iframe>\r\n"
    "</div>\r\n"
    "</div>\r\n"
    "</div>\r\n"
    "<div class=\"footer\">NetBus</div>\r\n"
    "<div class=\"footer\">NetBus</div>\r\n"
    "</body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_index_html = {
    NULL,
    (const unsigned char *)"/index.html",
    data_index_html,
    sizeof(data_index_html),
    FS_FILE_FLAGS_SSI
};

/* /index.shtml — same data as index.html.  LWIP's default-filename list
   tries .shtml first with shtml=1 flag, so SSI works for root "/". */
static const struct fsdata_file file_index_shtml = {
    &file_index_html,
    (const unsigned char *)"/index.shtml",
    data_index_html,
    sizeof(data_index_html),
    FS_FILE_FLAGS_SSI
};

/* ── /netcfg.html (Network Configuration) ────────────────────────────────── */

static const unsigned char data_netcfg_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
    "<title>NetBus — Network Config</title>\r\n"
    "<style>\r\n"
    WEB_CSS
    "  input{width:100%;padding:.5rem;background:#0f1923;border:1px solid #253545;"
      "border-radius:6px;color:#d0e0f0;font-size:.88rem;margin:.2rem 0}\r\n"
    "  input:focus{outline:none;border-color:#3080c0}\r\n"
    "  button{background:#205080;color:#d0e0f0;border:none;padding:.6rem 1.5rem;"
      "border-radius:6px;font-size:.9rem;cursor:pointer;margin:.5rem .5rem 0 0}\r\n"
    "  button:hover{background:#2868a0}\r\n"
    "  button.danger{background:#802020}\r\n"
    "  button.danger:hover{background:#a03030}\r\n"
    "  .row{display:flex;gap:.5rem;align-items:center}\r\n"
    "  .row label{color:#7099b8;font-size:.85rem;min-width:70px}\r\n"
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<div class=\"card\">\r\n"
    "  <h1>Network Configuration</h1>\r\n"
    "  <div class=\"nav\">\r\n"
    "    <a href=\"/index.html\">Dashboard</a>\r\n"
    "    <a href=\"/netcfg.html\" class=\"on\">Network</a>\r\n"
    "  </div>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>IPv4 Settings</h2>\r\n"
    "  <form action=\"/netcfg.cgi\" method=\"GET\">\r\n"
    "    <div class=\"row\"><label>IP</label>"
      "<input name=\"ip\" value=\"<!--#ip-->\" placeholder=\"192.168.1.10\"></div>\r\n"
    "    <div class=\"row\"><label>Mask</label>"
      "<input name=\"mask\" value=\"<!--#mask-->\" placeholder=\"255.255.255.0\"></div>\r\n"
    "    <div class=\"row\"><label>Gateway</label>"
      "<input name=\"gw\" value=\"<!--#gw-->\" placeholder=\"192.168.1.1\"></div>\r\n"
    "    <div class=\"row\"><label>SCPI Port</label>"
      "<input name=\"port\" value=\"5025\" type=\"number\" min=\"1\" max=\"65535\"></div>\r\n"
    "    <p class=\"muted\">Settings take effect after reboot.</p>\r\n"
    "    <button type=\"submit\">Save Settings</button>\r\n"
    "    <a href=\"/reboot.cgi\"><button type=\"button\" class=\"danger\">Reboot Now</button></a>\r\n"
    "  </form>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>Current Status</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td class=\"k\">Link</td><td class=\"v\"><!--#link--></td></tr>\r\n"
    "    <tr><td class=\"k\">MAC</td><td class=\"v\"><!--#mac--></td></tr>\r\n"
    "    <tr><td class=\"k\">Uptime</td><td class=\"v\"><!--#uptime--></td></tr>\r\n"
    "  </table>\r\n"
    "</div>\r\n"
    "</body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_netcfg_html = {
    &file_index_shtml,
    (const unsigned char *)"/netcfg.html",
    data_netcfg_html,
    sizeof(data_netcfg_html),
    0
};

/* ── /netcfg_ok.html (Config saved confirmation) ─────────────────────────── */

static const unsigned char data_netcfg_ok_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
    "<meta http-equiv=\"refresh\" content=\"3;url=/netcfg.html\">\r\n"
    "<title>NetBus — Saved</title>\r\n"
    "<style>\r\n"
    WEB_CSS
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<div class=\"card\">\r\n"
    "  <h1><span class=\"led green\"></span>Settings Saved</h1>\r\n"
    "  <p>Configuration written to flash. You must <b>reboot</b> for changes to take effect.</p>\r\n"
    "  <p style=\"margin-top:1rem\">"
    "    <a href=\"/netcfg.html\">Back to Network</a> &nbsp;|&nbsp; "
    "    <a href=\"/index.html\">Dashboard</a> &nbsp;|&nbsp; "
    "    <a href=\"/reboot.cgi\" style=\"color:#e05040\">Reboot Now</a>"
    "  </p>\r\n"
    "  <p class=\"muted\">Redirecting in 3s...</p>\r\n"
    "</div>\r\n"
    "</body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_netcfg_ok_html = {
    &file_netcfg_html,
    (const unsigned char *)"/netcfg_ok.html",
    data_netcfg_ok_html,
    sizeof(data_netcfg_ok_html),
    0
};

/* ── /netcfg_err.html (Config validation error) ──────────────────────────── */

static const unsigned char data_netcfg_err_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
    "<meta http-equiv=\"refresh\" content=\"3;url=/netcfg.html\">\r\n"
    "<title>NetBus — Error</title>\r\n"
    "<style>\r\n"
    WEB_CSS
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<div class=\"card\">\r\n"
    "  <h1><span class=\"led red\"></span>Invalid Input</h1>\r\n"
    "  <p>One or more fields contain invalid values. Please check the IP address format (x.x.x.x),"
    " netmask, and port number (1-65535).</p>\r\n"
    "  <p style=\"margin-top:1rem\"><a href=\"/netcfg.html\">Back to Network</a></p>\r\n"
    "  <p class=\"muted\">Redirecting in 3s...</p>\r\n"
    "</div>\r\n"
    "</body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_netcfg_err_html = {
    &file_netcfg_ok_html,
    (const unsigned char *)"/netcfg_err.html",
    data_netcfg_err_html,
    sizeof(data_netcfg_err_html),
    0
};

/* ── /scpi_ok.html (minimal redirect after SCPI enqueue) ────────────────── */

static const unsigned char data_scpi_ok_html[] =
    "<!DOCTYPE html>\r\n"
    "<html><head>"
    "<meta charset=\"UTF-8\">"
    "<meta http-equiv=\"refresh\" content=\"0;url=/index.shtml\">"
    "</head><body><a href=\"/index.shtml\">OK</a></body></html>\r\n";

static const struct fsdata_file file_scpi_ok_html = {
    &file_netcfg_err_html,
    (const unsigned char *)"/scpi_ok.html",
    data_scpi_ok_html,
    sizeof(data_scpi_ok_html),
    0
};

/* ── /scpi_done.html (redirects to log view, for iframe form target) ────── */

static const unsigned char data_scpi_done_html[] =
    "<!DOCTYPE html>\r\n"
    "<html><head>"
    "<meta charset=\"UTF-8\">"
    "<meta http-equiv=\"refresh\" content=\"0;url=/log_view.html\">"
    "</head><body></body></html>\r\n";

static const struct fsdata_file file_scpi_done_html = {
    &file_scpi_ok_html,
    (const unsigned char *)"/scpi_done.html",
    data_scpi_done_html,
    sizeof(data_scpi_done_html),
    0
};

/* ── /log_view.html (standalone log viewer) ─────────────────────────────── */

static const unsigned char data_log_view_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<meta http-equiv=\"refresh\" content=\"5\">\r\n"
    "<title>Log</title>\r\n"
    "<style>\r\n"
    "  *{margin:0;padding:0;box-sizing:border-box}\r\n"
    "  body{background:#0a1018;color:#90c0d0;font-family:Consolas,monospace;"
      "font-size:12px;padding:6px;line-height:1.4}\r\n"
    "</style>\r\n"
    "</head>\r\n"
    "<body><pre style=\"white-space:pre-wrap;word-break:break-all;margin:0\"><!--#logtext--></pre></body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_log_view_html = {
    &file_scpi_done_html,
    (const unsigned char *)"/log_view.html",
    data_log_view_html,
    sizeof(data_log_view_html),
    FS_FILE_FLAGS_SSI
};

/* ── /404.html ────────────────────────────────────────────────────────────── */

static const unsigned char data_404_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
    "<title>404 — NetBus</title>\r\n"
    "<style>\r\n"
    "  *{margin:0;padding:0;box-sizing:border-box}\r\n"
    "  body{font-family:system-ui,sans-serif;background:#0f1923;color:#c0d0e0;display:flex;align-items:center;justify-content:center;min-height:100vh}\r\n"
    "  .box{text-align:center}\r\n"
    "  h1{font-size:4rem;color:#e05040}\r\n"
    "  p{color:#7099b8;margin:1rem 0}\r\n"
    "  a{color:#60b0e0}\r\n"
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<div class=\"box\">\r\n"
    "  <h1>404</h1>\r\n"
    "  <p>Page not found</p>\r\n"
    "  <a href=\"/index.html\">Back to home</a>\r\n"
    "</div>\r\n"
    "</body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_404_html = {
    &file_log_view_html,
    (const unsigned char *)"/404.html",
    data_404_html,
    sizeof(data_404_html),
    0
};

/* FS_ROOT — entry point for fs.c; points to the last-defined file
   (which chains to the previous one via ->next)                              */
#define FS_ROOT &file_404_html

#undef WEB_CSS

/* ═══════════════════════════════════════════════════════════════════════════════
 * For LWIP's default-page logic: if the client requests "/", httpd tries
 * the files listed in g_psDefaultFilenames[] (see httpd_structs.h):
 *   {"/index.html", "/index.htm", "/index.shtml", "/index.ssi", "/index.json"}
 * so "/" will resolve to "/index.html" automatically.
 * ═════════════════════════════════════════════════════════════════════════════ */
