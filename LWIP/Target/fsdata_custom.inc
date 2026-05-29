/* ═══════════════════════════════════════════════════════════════════════════════
 * fsdata_custom.c — Web filesystem data for LWIP HTTPD
 *
 * This file is #include'd by fs.c (via HTTPD_FSDATA_FILE macro when
 * HTTPD_USE_CUSTOM_FSDATA = 1). It defines FS_ROOT as the head of a
 * linked list of static file entries.
 *
 * Adding a new file:
 *   1. Declare the byte array (data_<path>)
 *   2. Declare the fsdata_file struct (file_<path>), chaining it to
 *      the previously-defined file via the `next` pointer
 *   3. Update FS_ROOT to point to the newest file
 * ═════════════════════════════════════════════════════════════════════════════ */

/* ── /index.html ──────────────────────────────────────────────────────────── */

static const unsigned char data_index_html[] =
    "<!DOCTYPE html>\r\n"
    "<html lang=\"en\">\r\n"
    "<head>\r\n"
    "<meta charset=\"UTF-8\">\r\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
    "<title>NetBus — STM32H743</title>\r\n"
    "<style>\r\n"
    "  *{margin:0;padding:0;box-sizing:border-box}\r\n"
    "  body{font-family:system-ui,-apple-system,sans-serif;background:#0f1923;color:#c0d0e0;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:2rem}\r\n"
    "  .card{background:#182530;border:1px solid #253545;border-radius:12px;padding:2rem;margin:1rem 0;width:100%;max-width:600px;box-shadow:0 4px 24px rgba(0,0,0,.3)}\r\n"
    "  h1{font-size:1.6rem;font-weight:600;color:#e0f0ff;margin-bottom:.25rem}\r\n"
    "  h2{font-size:1.1rem;font-weight:500;color:#90b8e0;margin-bottom:1rem;border-bottom:1px solid #253545;padding-bottom:.5rem}\r\n"
    "  .tag{display:inline-block;background:#1a3a50;color:#60c0ff;font-size:.75rem;padding:.2rem .6rem;border-radius:4px;margin-right:.4rem}\r\n"
    "  table{width:100%;border-collapse:collapse}\r\n"
    "  td{padding:.5rem .75rem;border-bottom:1px solid #1e2e3c}\r\n"
    "  td:first-child{color:#7099b8;width:40%}\r\n"
    "  td:last-child{color:#d0e0f0}\r\n"
    "  .led{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:.4rem}\r\n"
    "  .led.green{background:#40d060;box-shadow:0 0 6px #40d060}\r\n"
    "  .led.yellow{background:#e0b040;box-shadow:0 0 6px #e0b040}\r\n"
    "  .led.red{background:#e05040;box-shadow:0 0 6px #e05040}\r\n"
    "  .footer{text-align:center;color:#506070;font-size:.8rem;margin-top:1.5rem}\r\n"
    "  a{color:#60b0e0}\r\n"
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<div class=\"card\">\r\n"
    "  <h1>NetBus Board Controller</h1>\r\n"
    "  <span class=\"tag\">STM32H743ZI</span>\r\n"
    "  <span class=\"tag\">Cortex-M7 @ 480 MHz</span>\r\n"
    "  <span class=\"tag\">FreeRTOS + LwIP</span>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>System Status</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td>Status</td><td><span class=\"led green\"></span>Running</td></tr>\r\n"
    "    <tr><td>IP Address</td><td>192.168.1.10 (static)</td></tr>\r\n"
    "    <tr><td>Netmask</td><td>255.255.255.0</td></tr>\r\n"
    "    <tr><td>Gateway</td><td>192.168.1.1</td></tr>\r\n"
    "    <tr><td>HTTP Port</td><td>80</td></tr>\r\n"
    "  </table>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>Services</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td>SCPI (TCP)</td><td>Port 5025</td></tr>\r\n"
    "    <tr><td>Modbus RTU</td><td>RS485</td></tr>\r\n"
    "    <tr><td>CAN Bus</td><td>FDCAN1</td></tr>\r\n"
    "  </table>\r\n"
    "</div>\r\n"
    "<div class=\"card\">\r\n"
    "  <h2>Analog &amp; RF</h2>\r\n"
    "  <table>\r\n"
    "    <tr><td>Attenuator</td><td>Programmable RF</td></tr>\r\n"
    "    <tr><td>RF Switch</td><td>SPDT</td></tr>\r\n"
    "    <tr><td>Power Detector</td><td>AD8318</td></tr>\r\n"
    "  </table>\r\n"
    "</div>\r\n"
    "<div class=\"footer\">\r\n"
    "  NetBus &mdash; STM32H743ZI &mdash; LwIP 2.2.1\r\n"
    "</div>\r\n"
    "</body>\r\n"
    "</html>\r\n";

static const struct fsdata_file file_index_html = {
    NULL,               /* next */
    (const unsigned char *)"/index.html",      /* name */
    data_index_html,    /* data */
    sizeof(data_index_html), /* len */
    0                   /* flags */
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
    &file_index_html,   /* next */
    (const unsigned char *)"/404.html",
    data_404_html,
    sizeof(data_404_html),
    0
};

/* FS_ROOT — entry point for fs.c; points to the last-defined file
   (which chains to the previous one via ->next)                              */
#define FS_ROOT &file_404_html

/* ═══════════════════════════════════════════════════════════════════════════════
 * For LWIP's default-page logic: if the client requests "/", httpd tries
 * the files listed in g_psDefaultFilenames[] (see httpd_structs.h):
 *   {"/index.html", "/index.htm", "/index.shtml", "/index.ssi", "/index.json"}
 * so "/" will resolve to "/index.html" automatically.
 * ═════════════════════════════════════════════════════════════════════════════ */
