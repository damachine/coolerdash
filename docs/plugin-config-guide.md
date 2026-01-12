# CoolerControl Plugin Configuration Guide

## Problem
Die UI speichert Einstellungen im Browser, aber der C-Daemon liest nur die config.ini. Änderungen in der UI werden nicht übernommen.

## Lösung: 3 Ansätze

### **Ansatz 1: CoolerControl Plugin-Config-API (EMPFOHLEN)**

#### Wie funktioniert es?
1. **UI speichert**: Web-Interface nutzt `savePluginConfig(config)` → CoolerControl speichert JSON
2. **Daemon liest**: C-Programm liest diese JSON-Config beim Start
3. **config.ini bleibt**: Als Fallback, wenn JSON nicht vorhanden

#### Vorteile
✅ Native CoolerControl-Integration
✅ Keine Root-Rechte für Benutzer nötig
✅ UI-Änderungen werden sofort gespeichert
✅ config.ini bleibt als Fallback

#### Implementierung

**1. ui.html anpassen** (Beispiel in `ui-plugin-example.html`):
```html
<script type="text/javascript" src="../../lib/cc-plugin-lib.js"></script>
<script>
const saveConfig = () => {
    const form = document.getElementById('pluginConfigForm');
    const formData = new FormData(form);
    const config = convertFormToObject(formData);
    savePluginConfig(config);  // ← CoolerControl API
};

runPluginScript(async () => {
    const config = await getPluginConfig();  // ← Lädt gespeicherte Config
    // Formular mit Werten füllen...
});
</script>
```

**2. C-Daemon erweitern**:
```c
// In src/main.c nach load_user_config():
#include "device/plugin_config.h"

// Plugin config hat Vorrang vor config.ini
if (load_plugin_config(config)) {
    log_message(LOG_INFO, "Using CoolerControl plugin configuration");
}
```

**3. Wo speichert CoolerControl?**
CoolerControl speichert Plugin-Configs wahrscheinlich hier:
- `/etc/coolercontrol/config/plugins/coolerdash.json` ODER
- `/etc/coolercontrol/plugins/coolerdash/plugin-config.json`

→ Prüfe mit: `find /etc/coolercontrol -name "*.json" 2>/dev/null`

---

### **Ansatz 2: config.ini beschreibbar machen**

#### Berechtigungen anpassen
**Aktuell** (Makefile Zeile 278):
```makefile
@install -Dm644 etc/coolerdash/config.ini "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/config.ini"
```
→ `644` = Read-only für Nicht-Root

**Ändern zu**:
```makefile
@install -Dm664 etc/coolerdash/config.ini "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/config.ini"
@chown root:coolercontrol "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/config.ini" 2>/dev/null || true
```
→ `664` = Gruppe kann schreiben

#### Backend-Service für UI nötig
Die UI kann nicht direkt Dateien schreiben! Du brauchst:
1. **Python/Node.js-Backend** im Plugin, das config.ini schreibt
2. **UI sendet Änderungen** via POST an Backend
3. **Backend validiert** und schreibt config.ini

**Nachteile**:
❌ Komplexer (Backend-Service erforderlich)
❌ Security-Risiko (Dateizugriff via Web-UI)
❌ Nicht der CoolerControl-Standard

---

### **Ansatz 3: Hybridlösung (SCHNELLSTE)**

Für **sofortige Lösung ohne Code-Änderung**:

#### Schritt 1: Berechtigungen temporär öffnen
```bash
sudo chmod 666 /etc/coolercontrol/plugins/coolerdash/config.ini
```

#### Schritt 2: Benutzer bearbeitet direkt
```bash
sudo nano /etc/coolercontrol/plugins/coolerdash/config.ini
```

#### Schritt 3: Plugin neu starten
```bash
sudo systemctl restart cc-plugin-coolerdash.service
# ODER
sudo systemctl restart coolercontrold.service
```

**Nachteil**: UI ist nur Anzeige, Änderungen manuell

---

## Empfohlene Umsetzung (Ansatz 1)

### Phase 1: Minimale Plugin-Config-Integration
1. ✅ `plugin_config.c/h` erstellt (siehe oben)
2. ⏳ `main.c` anpassen:
```c
// Nach load_user_config() in main.c einfügen:
#include "device/plugin_config.h"

// Zeile ~880 in main.c:
int user_config_result = load_user_config(config_path, config);
apply_system_defaults(config);

// NEU: Plugin-Config überschreibt config.ini-Werte
if (load_plugin_config(config)) {
    log_message(LOG_STATUS, "Applied CoolerControl plugin configuration");
}
```

3. ⏳ `ui.html` vereinfachen (nutze `ui-plugin-example.html` als Basis)
4. ⏳ Makefile anpassen (plugin_config.c kompilieren)

### Phase 2: config.ini als Readonly-Fallback
- config.ini bleibt mit `644` Permissions
- Wird nur gelesen, nie geschrieben
- UI nutzt ausschließlich CoolerControl Plugin-API

---

## Testen

### 1. CoolerControl Plugin-Config finden
```bash
find /etc/coolercontrol -name "*.json" 2>/dev/null
journalctl -u coolercontrold.service | grep -i config
```

### 2. JSON manuell erstellen (zum Testen)
```bash
sudo nano /etc/coolercontrol/plugins/coolerdash/plugin-config.json
```
```json
{
  "daemon_address": "http://localhost:11987",
  "display_mode": "circle",
  "refresh_interval": 3.0,
  "brightness": 90,
  "circle_switch_interval": 10
}
```

### 3. Plugin neu starten und Logs prüfen
```bash
sudo systemctl restart coolercontrold.service
journalctl -u coolercontrold.service -f | grep -i coolerdash
```

---

## Zusammenfassung

| Ansatz | Aufwand | Security | CoolerControl-Konform |
|--------|---------|----------|----------------------|
| **1: Plugin-API** | Mittel | ✅ Gut | ✅ Ja |
| **2: Writable INI** | Hoch | ⚠️ Mittel | ❌ Nein |
| **3: Manuell** | Niedrig | ✅ Gut | ⚠️ Workaround |

**Fazit**: Nutze **Ansatz 1** für professionelle Integration!

## Nächste Schritte

1. Finde heraus, wo CoolerControl Plugin-Configs speichert:
```bash
find /etc/coolercontrol -type f -name "*.json" 2>/dev/null
strings /usr/bin/coolercontrold | grep -i plugin | grep -i config
```

2. Teste mit manuell erstellter JSON (siehe oben)

3. Wenn es funktioniert: `plugin_config.c` in Makefile einbinden

4. UI auf Plugin-API umbauen (siehe `ui-plugin-example.html`)
