# Custom Device Plugin - Der bessere Mittelweg?

## Was ist das Custom Device Plugin?

Das **Custom Device Plugin** ist ein **DEVICE SERVICE** Plugin, das eine **Shell-Command-Bridge** bietet:

```
CoolerControl → gRPC → Custom Device Service → Shell Commands → Hardware
                         (Rust)                  (user-defined)
```

### Wie funktioniert es?

1. **Device Service** (in Rust, mit gRPC)
2. **User konfiguriert Shell-Commands** via Web-UI
3. **Service führt Commands aus** für jede Status-Abfrage
4. **Erscheint als natives Device** in CoolerControl

---

## Vergleich: 3 Ansätze für coolerdash

### 1. INTEGRATION Plugin (✅ Aktuell - EMPFOHLEN)

```toml
type = "integration"
```

**Workflow**:
```
coolerdash (C-Daemon) → REST API → CoolerControl → Hardware
     ↓ eigenständig
   - Liest Temps
   - Rendert PNG
   - Uploaded zu LCD
   - Eigenes Timing (2.5s)
```

**Pro**:
- ✅ Volle Kontrolle (Timing, Rendering, Logic)
- ✅ Keine Abhängigkeiten (kein gRPC/Rust)
- ✅ Performant (kein Polling-Overhead)
- ✅ Funktioniert bereits perfekt

**Contra**:
- ❌ Kein virtuelles Device in UI
- ❌ Channels nicht als Sensoren nutzbar

---

### 2. CUSTOM DEVICE Plugin (🔧 Hybrid-Ansatz)

```toml
type = "device"
```

**Workflow**:
```
CoolerControl → gRPC → Custom Device Service → Shell Commands
                        (vorgefertigt)           (du definierst)
                                                     ↓
                                               coolerdash Binary
```

**Wie würde das aussehen?**

**Config in UI** (JSON via Plugin Settings):
```json
{
  "devices": [
    {
      "name": "CoolerDash Virtual",
      "temp_sensors": [
        {
          "name": "CPU",
          "command": "/etc/coolercontrol/plugins/coolerdash/coolerdash --get-cpu-temp"
        },
        {
          "name": "GPU",
          "command": "/etc/coolercontrol/plugins/coolerdash/coolerdash --get-gpu-temp"
        }
      ],
      "lcd_channels": [
        {
          "name": "Dashboard",
          "update_command": "/etc/coolercontrol/plugins/coolerdash/coolerdash --update-lcd"
        }
      ]
    }
  ]
}
```

**Dein coolerdash müsste dann**:
```bash
# Neue Modi hinzufügen:
coolerdash --get-cpu-temp         # Output: 45000 (45°C in millidegrees)
coolerdash --get-gpu-temp         # Output: 62000 (62°C)
coolerdash --update-lcd           # Rendert + Uploaded PNG (wie bisher)
```

**Pro**:
- ✅ Erscheint als natives Device in UI
- ✅ Temp-Channels nutzbar für Profiles/Functions
- ✅ Du musst kein gRPC implementieren
- ✅ Rust-Service ist vorgefertigt

**Contra**:
- ❌ Shell-Command-Overhead (jede Sekunde!)
- ❌ Dein Binary wird jede Sekunde 2x aufgerufen (CPU, GPU)
- ❌ Ineffizient (neuer Prozess statt Daemon)
- ❌ Zusätzliche Dependency (Custom Device Service)
- ❌ Komplexität steigt

---

### 3. EIGENES Device Service (❌ Nicht empfohlen)

```toml
type = "device"
```

**Workflow**:
```
CoolerControl → gRPC → coolerdash-service (Rust) → Hardware
                        (von dir neu geschrieben)
```

**Pro**:
- ✅ Maximale Integration
- ✅ Keine Shell-Command-Overhead
- ✅ Native gRPC Performance

**Contra**:
- ❌ Komplett neu schreiben in Rust
- ❌ gRPC/Protobuf lernen
- ❌ Wartungsaufwand

---

## Performance-Vergleich

### Integration (aktuell):
```
1x Daemon läuft permanent
└─ Alle 2.5s: Temp lesen + Render + Upload
   └─ 1x API Call für Temps
   └─ 1x API Call für Upload
   └─ Gesamt: ~2-3 API Calls pro Zyklus

CPU-Last: Minimal (schläft zwischen Updates)
```

### Custom Device Plugin:
```
Custom Device Service läuft permanent
└─ CoolerControl pollt jede Sekunde:
   ├─ Shell: coolerdash --get-cpu-temp  (neuer Prozess!)
   ├─ Shell: coolerdash --get-gpu-temp  (neuer Prozess!)
   └─ Shell: coolerdash --update-lcd    (neuer Prozess!)
      └─ Jeder Aufruf: Binary laden, init, API call, exit

CPU-Last: DEUTLICH höher (3x Prozess-Spawns pro Sekunde!)
```

**Performance-Problem**:
```bash
# Mit Custom Device:
Jede Sekunde:
  fork() → exec(/coolerdash --get-cpu-temp)
  fork() → exec(/coolerdash --get-gpu-temp)  
  fork() → exec(/coolerdash --update-lcd)

= 3 Prozesse/Sekunde × Startup-Zeit × API-Initialisierung
= Massive Overhead!
```

---

## Für coolerdash: Was ist besser?

### ✅ **BLEIB bei INTEGRATION** (klar empfohlen!)

**Warum?**

1. **Performance**: Daemon vs. Shell-Commands jede Sekunde
   ```
   Integration:  1 Daemon, schläft zwischen Updates
   Custom Device: 3 neue Prozesse JEDE SEKUNDE
   ```

2. **Effizienz**: Eigenes Timing
   ```
   Integration:  Update alle 2.5s (konfigurierbar)
   Custom Device: CoolerControl pollt jede 1s (fix)
   ```

3. **Einfachheit**: Funktioniert bereits
   ```
   Integration:  0 Änderungen nötig
   Custom Device: Binary umschreiben + neue Modi
   ```

4. **Wartung**: Native C99
   ```
   Integration:  Nur dein Code
   Custom Device: + Custom Device Service Dependency
   ```

### 🤔 Wann wäre Custom Device sinnvoll?

**Nur wenn du willst**:
- ❌ Temperaturen als native Channels in Profiles nutzen
- ❌ "CoolerDash Device" in der Device-Liste
- ❌ Bereit bist, Performance zu opfern

**ABER**: Du nutzt bereits existierende Hardware (Kraken)!
Die Temps kommen von echten Sensoren, nicht von dir.

---

## Konkrete Empfehlung

### Phase 1: Jetzt (✅ Empfohlen)
**Bleib bei Integration, verbessere UI-Config**:

1. Plugin-Config-API Integration (wie besprochen)
   ```javascript
   // ui.html
   savePluginConfig(config);  // Statt Browser-Storage
   ```

2. C-Daemon liest Plugin-Config
   ```c
   // main.c
   load_plugin_config(config);  // Überschreibt config.ini
   ```

3. config.ini als Fallback behalten

**Aufwand**: 1-2 Tage  
**Benefit**: UI-Einstellungen funktionieren  
**Risiko**: Niedrig  

---

### Phase 2: Später (⚠️ Nur wenn wirklich nötig)
**Custom Device Integration** (NUR wenn du virtuelle Channels brauchst):

1. Daemon-Binary erweitern:
   ```c
   // main.c - Neue Modi
   if (argc == 2 && strcmp(argv[1], "--get-cpu-temp") == 0) {
       printf("%d\n", get_cpu_temp() * 1000);  // millidegrees
       exit(0);
   }
   ```

2. Custom Device Plugin installieren:
   ```bash
   curl -fsSL https://gitlab.com/.../install.sh | sh
   ```

3. In UI konfigurieren:
   ```json
   {
     "temp_sensors": [
       {"name": "CPU", "command": "coolerdash --get-cpu-temp"}
     ]
   }
   ```

**Aufwand**: 1 Woche  
**Benefit**: Temps als Channels, Device in UI  
**Risiko**: Performance-Overhead, mehr Dependencies  

---

## Fazit

```
╔════════════════════════════════════════════════════════════════╗
║  EMPFEHLUNG: BLEIB BEI INTEGRATION ✅                          ║
╚════════════════════════════════════════════════════════════════╝

Gründe:
1. Funktioniert perfekt
2. Performant (Daemon vs. Shell-Spawns)
3. Einfach wartbar (C99, keine Dependencies)
4. Du brauchst keine virtuellen Channels

Custom Device Plugin ist:
✅ Clever für Shell-Command-Hardware
❌ Overkill für deinen Use-Case
❌ Performance-Nachteil (3 Prozesse/Sekunde)
❌ Du hast bereits bessere Lösung (Daemon)

Nächster Schritt:
→ Plugin-Config-API Integration (UI-Einstellungen)
→ NICHT Custom Device Migration
```

---

## Zusammenfassung: 3 Ansätze

| | Integration (✅) | Custom Device (⚠️) | Device Service (❌) |
|--|------------------|---------------------|---------------------|
| **Jetzt** | Fertig | Umbau nötig | Komplett neu |
| **Sprache** | C99 | C99 + Rust Service | Rust + gRPC |
| **Performance** | ⭐⭐⭐ Optimal | ⭐ Schlecht | ⭐⭐ Gut |
| **Aufwand** | 0 | Mittel | Hoch |
| **Für coolerdash** | ✅ Perfekt | ⚠️ Unnötig | ❌ Overkill |

**Custom Device ist interessant, ABER nicht für dich!**

Es ist gedacht für:
- Hardware ohne native Treiber
- Schnelles Prototyping
- Shell-Script-basierte Sensoren

**Du hast bereits eine bessere Lösung!** 🎯
