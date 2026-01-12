# Plugin-Config Migration: INI → JSON

## ✅ Was wurde implementiert

### 1. **config.json** - Neue JSON-Konfiguration
**Pfad**: `/etc/coolercontrol/plugins/coolerdash/config.json`

```json
{
  "daemon_address": "http://localhost:11987",
  "daemon_password": "",
  "display_mode": "dual",
  "refresh_interval": 2.5,
  "brightness": 80,
  "circle_switch_interval": 5,
  "orientation": 0,
  "width": 240,
  "height": 240
}
```

### 2. **index.html** - Moderne Web-UI
**Features**:
- ✅ CoolerControl Plugin-API Integration (`savePluginConfig`, `getPluginConfig`)
- ✅ Responsive Design mit Gradient-Hintergrund
- ✅ Toast-Benachrichtigungen
- ✅ Brightness-Slider mit Live-Vorschau
- ✅ Dynamische Circle-Interval-Anzeige
- ✅ Validation & Error-Handling
- ✅ Auto-Load beim Öffnen
- ✅ Auto-Close nach Speichern

### 3. **plugin_config.c/h** - JSON-Parser für C
**Funktionalität**:
- Liest JSON von mehreren möglichen Pfaden:
  1. `/etc/coolercontrol/plugins/coolerdash/config.json` (primär)
  2. `/etc/coolercontrol/plugins/coolerdash/plugin-config.json` (CoolerControl-managed)
  3. `/etc/coolercontrol/config/plugins/coolerdash.json` (alternativ)
- Parst alle Einstellungen (daemon, display, advanced)
- Überschreibt config.ini-Werte
- Validiert Eingaben (Ranges, Enums)

### 4. **main.c Integration**
**Load-Reihenfolge** (4 Stages):
```c
1. init_system_defaults()      // Hardcoded Defaults
2. load_user_config()           // config.ini (Fallback)
3. apply_system_defaults()      // Fill Missing
4. load_plugin_config()         // config.json (Überschreibt INI!) ✨
```

---

## 📁 Konfigurationshierarchie

### Priorität (höchste zuerst):

1. **config.json** (UI-gespeichert) ← **HÖCHSTE PRIORITÄT**
2. **config.ini** (Manuell bearbeitet)
3. **Hardcoded Defaults** (sys.c)

### Beispiel:

**config.ini**:
```ini
[display]
brightness=90
```

**config.json** (von UI):
```json
{
  "brightness": 80
}
```

**Resultat**: `brightness = 80` (JSON gewinnt!)

---

## 🚀 Wie es funktioniert

### User-Workflow:

1. **Öffnet Plugin-Settings** in CoolerControl UI
2. **Ändert Einstellungen** in der Web-UI (index.html)
3. **Klickt "Save"**
   - UI ruft `savePluginConfig(config)` auf
   - CoolerControl speichert JSON
4. **Restart Plugin**:
   ```bash
   sudo systemctl restart coolercontrold
   ```
5. **Daemon lädt config.json** automatisch beim Start

### Technischer Flow:

```
┌────────────────┐
│  Web UI        │ (index.html)
│  (Browser)     │
└────────┬───────┘
         │
         │ savePluginConfig(config)
         ▼
┌────────────────────┐
│  CoolerControl     │
│  Plugin-API        │
└────────┬───────────┘
         │
         │ Speichert als JSON
         ▼
┌──────────────────────────────────┐
│  config.json                      │
│  /etc/coolercontrol/plugins/...  │
└────────┬─────────────────────────┘
         │
         │ Daemon startet
         ▼
┌────────────────────┐
│  coolerdash        │
│  (C-Daemon)        │
│                    │
│  1. load_user_config(config.ini)   │
│  2. load_plugin_config(config.json) ← Überschreibt!
└────────────────────┘
```

---

## 🔧 Installation

### Build:
```bash
make clean && make
```

### Install:
```bash
sudo make install
```

**Installiert**:
- ✅ `coolerdash` Binary
- ✅ `config.ini` (Fallback)
- ✅ `config.json` (Leer, wird von UI befüllt)
- ✅ `index.html` (Web-UI)
- ✅ `manifest.toml` (mit `ui_page = "index.html"`)

### Restart:
```bash
sudo systemctl restart coolercontrold
```

---

## 🎨 Web-UI Features

### Sections:

1. **🌐 Daemon Settings**
   - CoolerControl API Address
   - API Password

2. **🖥️ Display Mode**
   - Mode: Dual / Circle
   - Circle Switch Interval (nur bei Circle-Modus sichtbar)

3. **📊 Display Settings**
   - Refresh Interval
   - Brightness (Slider mit Live-Anzeige)
   - Orientation (0°/90°/180°/270°)

4. **🔧 Advanced Settings**
   - Display Width (pixels)
   - Display Height (pixels)

### UI-Highlights:

- **Toast Notifications** (✅ Success, ❌ Error)
- **Auto-Load**: Lädt gespeicherte Config beim Öffnen
- **Auto-Close**: Schließt UI nach erfolgreichem Speichern
- **Validation**: Input-Ranges werden überprüft
- **Defaults**: Reset-Button stellt Standardwerte wieder her

---

## 🧪 Testing

### 1. UI testen (lokal):
```bash
# Web-UI im Browser öffnen
xdg-open file:///etc/coolercontrol/plugins/coolerdash/index.html
```

### 2. JSON manuell erstellen:
```bash
sudo nano /etc/coolercontrol/plugins/coolerdash/config.json
```
```json
{
  "display_mode": "circle",
  "brightness": 100,
  "refresh_interval": 1.0
}
```

### 3. Daemon starten mit Verbose:
```bash
/etc/coolercontrol/plugins/coolerdash/coolerdash --verbose
```

**Erwartete Logs**:
```
[INFO] Reading plugin config from: /etc/coolercontrol/plugins/coolerdash/config.json
[INFO] Loaded 3 settings from plugin config
[STATUS] Applied CoolerControl plugin configuration (config.json)
```

### 4. Prüfen, welche Config geladen wurde:
```bash
journalctl -u coolercontrold.service -f | grep -i coolerdash
```

---

## 🐛 Troubleshooting

### UI speichert nicht

**Problem**: `savePluginConfig()` ist undefined

**Lösung**: Prüfe ob `cc-plugin-lib.js` geladen wird:
```html
<script type="text/javascript" src="../../lib/cc-plugin-lib.js"></script>
```

**Pfad muss relativ sein** (CoolerControl injiziert die Library)

---

### config.json wird nicht geladen

**Check 1**: Datei existiert?
```bash
ls -la /etc/coolercontrol/plugins/coolerdash/config.json
```

**Check 2**: Valid JSON?
```bash
cat /etc/coolercontrol/plugins/coolerdash/config.json | jq .
```

**Check 3**: Daemon-Logs:
```bash
journalctl -u coolercontrold.service | grep "plugin config"
```

---

### UI zeigt alte Werte

**Problem**: Browser-Cache

**Lösung**: Hard-Refresh (Ctrl+F5) oder:
```bash
# config.json löschen und neu erstellen
sudo rm /etc/coolercontrol/plugins/coolerdash/config.json
echo '{}' | sudo tee /etc/coolercontrol/plugins/coolerdash/config.json
sudo systemctl restart coolercontrold
```

---

## 📋 Checkliste für Deployment

- [x] plugin_config.c kompiliert
- [x] main.c lädt plugin_config
- [x] Makefile installiert config.json + index.html
- [x] manifest.toml hat `ui_page = "index.html"`
- [x] index.html nutzt `savePluginConfig/getPluginConfig`
- [x] Default config.json vorhanden
- [x] Validierung in C-Code (Ranges)
- [x] Logging für Debug

---

## 🎯 Nächste Schritte

1. **Test im echten CoolerControl**:
   ```bash
   sudo make install
   sudo systemctl restart coolercontrold
   ```

2. **UI über CoolerControl öffnen**:
   - Plugin-Settings → CoolerDash → ⚙️ Settings Button

3. **Einstellungen ändern** und speichern

4. **Daemon neu starten** und Logs prüfen:
   ```bash
   journalctl -u coolercontrold.service -f
   ```

5. **Verify**: Sind die Änderungen aktiv?
   - LCD zeigt korrekte Brightness?
   - Refresh-Intervall stimmt?

---

## 📚 Weitere Dokumentation

- **[docs/plugin-types-comparison.md](../plugin-types-comparison.md)** - Plugin-Typen Vergleich
- **[docs/plugin-config-guide.md](../plugin-config-guide.md)** - Config-API Details
- **[docs/custom-device-analysis.md](../custom-device-analysis.md)** - Custom Device vs Integration

---

## ✨ Zusammenfassung

**Vorher** (config.ini only):
- ❌ User muss config.ini manuell bearbeiten
- ❌ Keine Web-UI
- ❌ Root-Rechte für Änderungen

**Jetzt** (config.json + UI):
- ✅ User bearbeitet über Web-UI
- ✅ Speichern via CoolerControl Plugin-API
- ✅ Automatisches Laden beim Start
- ✅ config.ini bleibt als Fallback
- ✅ Professionelle Integration

**Migration vollständig!** 🎉
