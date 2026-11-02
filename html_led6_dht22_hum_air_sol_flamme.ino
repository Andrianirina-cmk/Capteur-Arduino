#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <DHT.h>

// =====================================================
// 1. INFORMATIONS DU WIFI
// =====================================================
const char* ssid = "ZTE";
const char* password = "12345678";

// =====================================================
// 2. SERVEUR WEB
// =====================================================
WebServer server(80);

// =====================================================
// 3. BROCHES
// =====================================================
// LEDs (4 pièces)
const int LED_PINS[4] = {23, 22, 21, 19};

// DHT22
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Capteur de flamme (analogique)
const int FLAME_PIN = 34;

// Buzzer
const int BUZZER_PIN = 5;

// Ultrason HC-SR04
const int TRIG_PIN = 26;
const int ECHO_PIN = 27;

// Humidité du sol (analogique)
const int SOIL_PIN = 35;

// =====================================================
// 4. VARIABLES
// =====================================================
bool ledStates[4] = {false, false, false, false};

// =====================================================
// 5. FONCTIONS CAPTEURS
// =====================================================
float readTemperature() {
    float t = dht.readTemperature();
    if (isnan(t)) return -999;
    return t;
}

float readHumidity() {
    float h = dht.readHumidity();
    if (isnan(h)) return -999;
    return h;
}

bool readFlame() {
    int val = analogRead(FLAME_PIN);
    return val < 2000; // true = flamme présente (ajuster le seuil)
}

int readDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) return -1;
    return duration * 0.034 / 2;
}

int readSoilMoisture() {
    return analogRead(SOIL_PIN);
}

// =====================================================
// 6. SETUP
// =====================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32 - Contrôle Maison ===");

    // LEDs
    for (int i = 0; i < 4; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }

    // Buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // Ultrason
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // DHT22
    dht.begin();

    // LittleFS
    if (!LittleFS.begin(false)) {
        Serial.println("Erreur LittleFS !");
        return;
    }
    Serial.println("LittleFS initialise.");

    // Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connexion Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connecté !");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // =================================================
    // PAGE PRINCIPALE
    // =================================================
    server.on("/", HTTP_GET, []() {
        File file = LittleFS.open("/index.html", "r");
        if (!file) {
            server.send(404, "text/plain", "index.html introuvable");
            return;
        }
        server.streamFile(file, "text/html");
        file.close();
    });

    // =================================================
    // COMMANDES LED (4 pièces)
    // =================================================
    for (int i = 0; i < 4; i++) {
        String path = "/led/" + String(i + 1) + "/on";
        server.on(path.c_str(), HTTP_GET, [i]() {
            ledStates[i] = true;
            digitalWrite(LED_PINS[i], HIGH);
            server.send(200, "text/plain", "LED ON");
        });
        path = "/led/" + String(i + 1) + "/off";
        server.on(path.c_str(), HTTP_GET, [i]() {
            ledStates[i] = false;
            digitalWrite(LED_PINS[i], LOW);
            server.send(200, "text/plain", "LED OFF");
        });
    }

    // =================================================
    // API TEMPÉRATURE / HUMIDITÉ
    // =================================================
    server.on("/api/dht", HTTP_GET, []() {
        float t = readTemperature();
        float h = readHumidity();
        String json = "{\"temperature\":";
        json += String(t, 1);
        json += ",\"humidity\":";
        json += String(h, 1);
        json += "}";
        server.send(200, "application/json", json);
    });

    // =================================================
    // API FLAMME
    // =================================================
    server.on("/api/flame", HTTP_GET, []() {
        bool flame = readFlame();
        String json = "{\"flame\":";
        json += flame ? "true" : "false";
        json += "}";
        server.send(200, "application/json", json);
    });

    // =================================================
    // API ULTRASON
    // =================================================
    server.on("/api/ultrason", HTTP_GET, []() {
        int d = readDistance();
        bool presence = (d > 0 && d < 50); // < 50 cm = personne proche
        String json = "{\"distance\":";
        json += String(d);
        json += ",\"presence\":";
        json += presence ? "true" : "false";
        json += "}";
        server.send(200, "application/json", json);
    });

    // =================================================
    // API HUMIDITÉ SOL
    // =================================================
    server.on("/api/soil", HTTP_GET, []() {
        int val = readSoilMoisture();
        String etat;
        if (val > 3000) etat = "sec";
        else if (val > 1500) etat = "humide";
        else etat = "tres_humide";
        String json = "{\"raw\":";
        json += String(val);
        json += ",\"etat\":\"";
        json += etat;
        json += "\"}";
        server.send(200, "application/json", json);
    });

    server.begin();
    Serial.println("Serveur HTTP démarré !");
}

// =====================================================
// 7. LOOP
// =====================================================
unsigned long lastCheck = 0;
bool lastFlame = false;

void loop() {
    server.handleClient();

    // Gestion buzzer flamme toutes les 500ms
    if (millis() - lastCheck > 500) {
        lastCheck = millis();
        bool flame = readFlame();

        if (flame) {
            digitalWrite(BUZZER_PIN, HIGH);
            if (!lastFlame) Serial.println("FLAMME DETECTEE ! Buzzer ON");
        } else {
            digitalWrite(BUZZER_PIN, LOW);
            if (lastFlame) Serial.println("Plus de flamme. Buzzer OFF");
        }
        lastFlame = flame;
    }
}