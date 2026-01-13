# CoolerDash Plugin Integration Guide

## 📁 Plugin Struktur

```
/etc/coolercontrol/plugins/coolerdash/
├── manifest.toml              # Plugin-Konfiguration für CoolerControl
├── index.html                 # Web-UI für Einstellungen
├── config.json                # Daemon-Konfiguration (wird vom Binary gelesen)
├── coolerdash                 # Binary (Daemon)
├── update-config.sh           # Helper-Script zum Schreiben von config.json
└── *.png                      # LCD-Bilder
```

## 🔄 Konfigurations-Flow

### 1. **UI → CoolerControl** (Immer)
```javascript
await savePluginConfig(config)  // Speichert in CoolerControl's Datenbank
```
- ✅ Funktioniert immer
- ✅ UI zeigt beim nächsten Öffnen gespeicherte Werte
- ❌ Daemon sieht diese Werte NICHT

### 2. **UI → config.json** (Optional, für Daemon)
```javascript
await writeConfigToFile(config)  // Versucht config.json zu schreiben
```

**Methoden (in dieser Reihenfolge versucht):**

#### A. REST API Write (Wenn implementiert)
```bash
PUT http://localhost:11987/plugins/coolerdash/config
Authorization: Basic <base64(admin:password)>
Content-Type: application/json

{ "daemon": { ... }, "display": { ... } }
```

#### B. File Download (Fallback)
- Erstellt `config.json` als Download
- User muss manuell kopieren:
  ```bash
  sudo cp ~/Downloads/config.json /etc/coolercontrol/plugins/coolerdash/
  ```

#### C. Console Output (Last Resort)
- Zeigt JSON in Browser-Konsole
- User kopiert manuell in Datei

### 3. **config.json → Daemon** (Beim Start)
```c
// src/main.c
const char *config_path = "/etc/coolercontrol/plugins/coolerdash/config.json";
load_plugin_config(&config, config_path);
```

## 🚀 Setup-Schritte

### 1. Installation
```bash
# Projekt bauen
make clean && make

# Plugin installieren
sudo make install
```

### 2. Plugin aktivieren
1. CoolerControl öffnen
2. Settings → Plugins
3. CoolerDash aktivieren
4. UI öffnen (Zahnrad-Symbol)

### 3. Konfiguration
1. Einstellungen in UI ändern
2. **Speichern** klicken
3. Optional: Automatischer Neustart akzeptieren

### 4. Daemon-Neustart (Manual)
```bash
# Via CoolerControl UI
Settings → Plugins → CoolerDash → Restart Button

# Via CLI
sudo systemctl restart coolercontrold
```

## 🔧 Technische Details

### manifest.toml
```toml
id = "coolerdash"
type = "integration"              # Integration-Plugin (läuft als Daemon)
executable = "coolerdash"         # Binary-Pfad
privileged = true                 # Root-Rechte (für LCD-Zugriff)
ui_page = "index.html"           # Web-UI
```

### cc-plugin-lib.js API
```javascript
// Config Management
await getPluginConfig()          // Lädt aus CoolerControl DB
await savePluginConfig(obj)      // Speichert in CoolerControl DB

// UI Lifecycle
runPluginScript(async () => {})  // Init-Wrapper
close()                          // Modal schließen

// System Info (optional)
await getModes()                 // CoolerControl Modes
await getDevices()               // Detected Devices
await getStatus()                // System Status
```

### CoolerControl REST API
```bash
# Auth erforderlich für POST/PUT
Authorization: Basic base64(admin:password)

# Relevante Endpoints
GET  /devices          # Liste aller Geräte
POST /status           # Sensor-Daten
GET  /plugins          # Plugin-Liste
POST /plugins/{id}/restart  # Plugin neu starten (wenn implementiert)
```

## 🐛 Debugging

### UI Probleme
```javascript
// Browser Console öffnen (F12)
console.log("Current config:", await getPluginConfig());
console.log("Default config:", DEFAULT_CONFIG);
```

### Daemon Probleme
```bash
# Logs anschauen
journalctl -u coolercontrold -f

# Manual starten
sudo /etc/coolercontrol/plugins/coolerdash/coolerdash --verbose

# Config-Pfad testen
sudo /etc/coolercontrol/plugins/coolerdash/coolerdash /path/to/config.json
```

### Config nicht übernommen
```bash
# 1. Prüfen ob config.json existiert
ls -la /etc/coolercontrol/plugins/coolerdash/config.json

# 2. Inhalt validieren
jq '.' /etc/coolercontrol/plugins/coolerdash/config.json

# 3. Rechte prüfen
sudo chown root:root /etc/coolercontrol/plugins/coolerdash/config.json
sudo chmod 644 /etc/coolercontrol/plugins/coolerdash/config.json

# 4. Daemon neu starten
sudo systemctl restart coolercontrold
```

## 📝 Best Practices

### 1. Immer Backups erstellen
```bash
sudo cp /etc/coolercontrol/plugins/coolerdash/config.json \
        /etc/coolercontrol/plugins/coolerdash/config.json.backup
```

### 2. JSON validieren vor dem Schreiben
```javascript
try {
    JSON.parse(JSON.stringify(config));  // Test serialization
} catch (error) {
    console.error("Invalid config:", error);
}
```

### 3. Fehlerbehandlung in UI
```javascript
try {
    await savePluginConfig(config);
} catch (error) {
    // Zeige User-freundliche Fehlermeldung
    alert("Fehler beim Speichern: " + error.message);
}
```

### 4. Graceful Degradation
```javascript
// Wenn config.json nicht geschrieben werden kann,
// trotzdem in CoolerControl speichern
const fileWritten = await writeConfigToFile(config);
if (!fileWritten) {
    console.warn("Using CoolerControl DB only");
}
```

## 🔐 Sicherheit

### Privileged Access
```toml
privileged = true  # Nur wenn nötig (z.B. für /dev/hidraw*)
```

### Password Handling
```javascript
// NIEMALS Passwords in Logs
console.log("Config saved");  // ❌ console.log(config)
```

### File Permissions
```bash
# config.json sollte nur von root lesbar sein (enthält API-Password)
sudo chmod 600 /etc/coolercontrol/plugins/coolerdash/config.json
```

## 🆘 Häufige Probleme

### "Config nicht gefunden"
```bash
# Daemon sucht in dieser Reihenfolge:
# 1. Kommandozeilen-Argument
# 2. /etc/coolercontrol/plugins/coolerdash/config.json
# 3. /etc/coolercontrol/plugins/coolerdash/plugin-config.json
# 4. ./config.json (CWD)
# 5. Hardcoded Defaults
```

### "UI zeigt alte Werte"
- Cache leeren: Browser-DevTools → Application → Clear Storage
- Oder: `location.reload(true)`

### "Daemon startet nicht"
```bash
# 1. Binary-Rechte prüfen
sudo chmod +x /etc/coolercontrol/plugins/coolerdash/coolerdash

# 2. Dependencies prüfen
ldd /etc/coolercontrol/plugins/coolerdash/coolerdash

# 3. Logs prüfen
journalctl -u coolercontrold --since "5 minutes ago"
```

## 📚 Weitere Ressourcen

- [CoolerControl Plugin Docs](https://coolercontrol.org/plugins/)
- [CoolerControl OpenAPI](https://coolercontrol.org/openapi/)
- [cc-plugin-lib.js Source](https://gitlab.com/coolercontrol/coolercontrol/-/blob/main/coolercontrold/resources/lib/cc-plugin-lib.js)
- [Plugin Examples](https://gitlab.com/coolercontrol/cc-plugins)
