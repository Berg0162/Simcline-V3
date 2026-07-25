#include "WebConfigPortal.h"

#if defined(ENABLE_CONFIG_PORTAL)

#include "config/configSimcline.h"

#define DEBUG
#include "config/configDebug.h"

// These values are derived from the mechanical raw-grade limits.
const uint8_t RGVMIN_GRADE = (20000 - RGVMIN) / 100;
const uint8_t RGVMAX_GRADE = (RGVMAX - 20000) / 100;

namespace
{
constexpr uint32_t CONFIG_PORTAL_TIMEOUT_MS = 60UL * 1000UL;
constexpr uint32_t CONFIG_PORTAL_CLOSE_DELAY_MS = 250UL;
constexpr uint32_t CONFIG_PORTAL_CLIENT_TIMEOUT_MS = 120UL;

// Derived from config/configPortal.h
const char* CONFIG_PORTAL_APPNAME = PORTAL_APPNAME;
const char* CONFIG_PORTAL_PASSWORD = PORTAL_PASSWORD;

NetworkServer server(80);

bool serverStarted = false;
bool stopRequested = false;
uint32_t closeDueMs = 0;
uint32_t lastActivityMs = 0;

uint8_t boundedUChar(const int value, const int minValue, const int maxValue)
{
    return static_cast<uint8_t>(constrain(value, minValue, maxValue));
}

void touchActivity()
{
    lastActivityMs = millis();
}

String urlValue(const String& body, const char* key)
{
    String marker = String(key) + "=";
    int start = body.indexOf(marker);
    if(start < 0) {
        return "";
    }

    start += marker.length();
    int end = body.indexOf('&', start);
    if(end < 0) {
        end = body.length();
    }

    return body.substring(start, end);
}

String rangeField(const char* key,
                  const char* label,
                  const uint8_t value,
                  const uint8_t minValue,
                  const uint8_t maxValue)
{
    String html;
    html += "<div class='line'><label>";
    html += label;
    html += "</label><br><input type='range' id='";
    html += key;
    html += "' name='";
    html += key;
    html += "' min='";
    html += String(minValue);
    html += "' max='";
    html += String(maxValue);
    html += "' value='";
    html += String(value);
    html += "'><div><span id='";
    html += key;
    html += "_val'>";
    html += String(value);
    html += "</span></div></div><script>document.getElementById('";
    html += key;
    html += "').oninput=e=>document.getElementById('";
    html += key;
    html += "_val').textContent=e.target.value;</script>";
    return html;
}

void sendHttp(NetworkClient& client, const String& body)
{
    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ");
    client.print(body.length());
    client.print("\r\n\r\n");
    client.print(body);
}

void sendNotFound(NetworkClient& client)
{
    const char body[] = "Not found";
    client.print("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\nContent-Length: ");
    client.print(strlen(body));
    client.print("\r\n\r\n");
    client.print(body);
}

String readLine(NetworkClient& client)
{
    String line = client.readStringUntil('\n');
    line.trim();
    return line;
}

String readBody(NetworkClient& client, const size_t contentLength)
{
    String body;
    body.reserve(contentLength);

    const uint32_t deadline = millis() + CONFIG_PORTAL_CLIENT_TIMEOUT_MS;
    while(body.length() < contentLength && millis() < deadline) {
        while(client.available() && body.length() < contentLength) {
            body += static_cast<char>(client.read());
        }
        delay(1);
    }

    return body;
}
} // namespace

WebConfigPortal::WebConfigPortal()
{
    operations = OPS::getInstance();
}

void WebConfigPortal::refreshFromOperations()
{
    std::tie(ascent, descent, factor, display) = operations->getPhonePrefs();
}

void WebConfigPortal::storeToOperations()
{
    operations->storePhonePrefs(PhonePrefs(ascent, descent, factor, display));
}

String WebConfigPortal::pageHtml() const
{
    String html;
    html.reserve(2400);
    html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<style>body{background:#d2f3eb;font-family:Arial;color:#000;font-size:12pt;";
    html += "max-width:320px;margin:auto}.title{font-weight:bold;text-align:center;padding:10px 0}";
    html += ".line{text-align:center;padding:8px 0}button{font-size:14pt;width:150px;";
    html += "border-radius:10px;margin:10px;padding:6px}.radio{text-align:left;display:inline-block}</style>";
    html += "</head><body><div class='title'>SimclineCfg - Ride Setup</div>";
    html += "<form method='POST' action='/save'>";
    html += rangeField("asc", "Ascent Limit (%)", ascent, 0, RGVMAX_GRADE);
    html += rangeField("desc", "Descent Limit (%)", descent, 0, RGVMIN_GRADE);
    html += rangeField("factor", "Grade Factor (%)", factor, 0, 200);
    html += "<div class='line'>Display Mode<br><div class='radio'>";
    html += "<label><input type='radio' name='display' value='1'";
    if(display == 1) html += " checked";
    html += "> Cycling</label><br><label><input type='radio' name='display' value='2'";
    if(display != 1) html += " checked";
    html += "> Grade</label></div></div>";
    html += "<div class='line'><button type='submit'>Save</button></div></form>";
    html += "<div class='line'><a href='/close'><button>Close</button></a></div></body></html>";
    return html;
}

void WebConfigPortal::handleSave(NetworkClient& client, const String& body)
{
    ascent = boundedUChar(urlValue(body, "asc").toInt(), 0, RGVMAX_GRADE);
    descent = boundedUChar(urlValue(body, "desc").toInt(), 0, RGVMIN_GRADE);
    factor = boundedUChar(urlValue(body, "factor").toInt(), 0, 200);
    display = boundedUChar(urlValue(body, "display").toInt(), 1, 2);

    storeToOperations();
    LOG("WebConfigPortal: Settings saved to Operations");

    String redirectURL = "/";
    String closeURL = "/close";
    String html;
    html  = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Configuration Saved</title>";
    html += "<style>"
            "body { background:#d2f3eb; font-family:Arial; text-align:center; padding:20px; }"
            "a.button { display:inline-block; margin-top:20px; padding:10px 20px;"
            "background:#4CAF50; color:#fff; text-decoration:none; border-radius:8px; }"
            "</style></head><body>";
    html += "<h2>Configuration Saved!</h2>";
    html += "<p>You can return to the configuration page or close the session.</p>";
    html += "<a class='button' href='" + redirectURL + "'>Return</a>";
    html += "<p><a class='button' href='" + closeURL + "'>Close</a></p>";
    html += "</body></html>";

    sendHttp(client, html);
}

void WebConfigPortal::handleClient()
{
    NetworkClient client = server.accept();  // listen for incoming clients
    if(!client) {
        return;
    }

    client.setTimeout(CONFIG_PORTAL_CLIENT_TIMEOUT_MS);
    touchActivity();

    const String requestLine = readLine(client);
    size_t contentLength = 0;

    while(client.connected()) {
        String header = readLine(client);
        if(header.length() == 0) {
            break;
        }

        header.toLowerCase();
        if(header.startsWith("content-length:")) {
            contentLength = header.substring(15).toInt();
        }
    }

    if(requestLine.startsWith("GET /close")) {
        sendHttp(client,
            "<html><body style='text-align:center;font-family:Arial;font-size:22pt;'>"
            "<h2>Configuration complete!</h2><p>You may safely exit this page.</p>"
            "</body></html>");
        stopRequested = true;
        closeDueMs = millis() + CONFIG_PORTAL_CLOSE_DELAY_MS;
    } else if(requestLine.startsWith("POST /save")) {
        handleSave(client, readBody(client, contentLength));
    } else if(requestLine.startsWith("GET / ")) {
        refreshFromOperations();
        sendHttp(client, pageHtml());
    } else {
        sendNotFound(client);
    }

    delay(1);
    client.stop();
}

bool WebConfigPortal::start()
{
    if(initialized) {
        return true;
    }

    refreshFromOperations();
    initialized = true;
    LOG("WebConfigPortal: Initialized");

    if(!WiFi.softAP(CONFIG_PORTAL_APPNAME, CONFIG_PORTAL_PASSWORD)) {
        LOG("WebConfigPortal: Failed to start AP");
        return false;
    }

    server.begin();
    stopRequested = false;
    closeDueMs = 0;
    serverStarted = true;
    touchActivity();

    LOG("WebConfigPortal: AP open for configuration, IP: %s", WiFi.softAPIP().toString().c_str());
    return true;
}

void WebConfigPortal::update()
{
    if(!serverStarted) {
        return;
    }

    handleClient();

    if(stopRequested && millis() >= closeDueMs) {
        stop();
        return;
    }

    if(millis() - lastActivityMs > CONFIG_PORTAL_TIMEOUT_MS) {
        LOG("WebConfigPortal: Inactivity timeout, stopping WiFi");
        stop();
    }
}

bool WebConfigPortal::isActive() const
{
    return serverStarted;
}

void WebConfigPortal::stop()
{
    if(!serverStarted) {
        return;
    }

    server.stop();
    // 1. Kick off any connected clients and stop the AP broadcast safely
    WiFi.softAPdisconnect(false); 
    // 2. Allow a brief window for the netstack to unregister callbacks
    delay(100); 
    // 3. Explicitly shut down the WiFi
    WiFi.mode(WIFI_OFF);

    serverStarted = false;
    stopRequested = false;
    closeDueMs = 0;

    LOG("WebConfigPortal: WiFi stopped");
}

#else

#define DEBUG
#include "config/configDebug.h"

WebConfigPortal::WebConfigPortal()
{
    operations = OPS::getInstance();
}

bool WebConfigPortal::start()
{
    LOG("WebConfigPortal: WiFi configuration disabled for this build");
    return false;
}

void WebConfigPortal::update()
{
}

bool WebConfigPortal::isActive() const
{
    return false;
}

void WebConfigPortal::stop()
{
}

#endif
