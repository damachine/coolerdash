# CoolerControl Plugin-Typen: Integration vs. Device Service

## Aktueller Status: INTEGRATION Plugin ✅

**Dein coolerdash ist aktuell ein `type = "integration"` Plugin**

```toml
id = "coolerdash"
type = "integration"          # ← Aktuell
executable = "/etc/coolercontrol/plugins/coolerdash/coolerdash"
privileged = true
```

### Was ist ein INTEGRATION Plugin?

**Integration Plugins** sind eigenständige Programme, die:
- ✅ Direkt die CoolerControl **REST API** nutzen
- ✅ Eigenständig laufen (dein C-Daemon macht das bereits!)
- ✅ Keine gRPC-Kommunikation benötigen
- ✅ Einfacher zu implementieren (kein Proto/gRPC)
- ✅ Direkt auf Hardware zugreifen können
- ✅ Bereits funktionieren (dein aktueller Ansatz!)

**Beispiel**: Dein coolerdash:
```bash
coolerdash → REST API → CoolerControl → LCD Upload
```

---

## Alternative: DEVICE SERVICE Plugin 🔧

### Was ist ein DEVICE SERVICE Plugin?

**Device Service Plugins** erstellen **virtuelle Geräte** in CoolerControl:
- 📡 Kommunizieren via **gRPC** (Unix Domain Socket)
- 🔌 Stellen Hardware als CoolerControl-Device dar
- 📊 CoolerControl pollt Status-Informationen
- 🎮 Unterstützen Channels (Temp-Sensoren, Lüfter, LCD, etc.)

**gRPC API** (Proto):
```protobuf
service DeviceService {
  rpc ListDevices(...)        // Welche Geräte gibt es?
  rpc Status(...)             // Aktuelle Temperaturen/Speeds
  rpc Lcd(LcdRequest)         // LCD-Bild setzen
  rpc FixedDuty(...)          // Lüfter steuern
  // ...
}
```

**Beispiel**: Custom Device Plugin
```bash
CoolerControl → gRPC → Device Service → Hardware
                 ↑
              Status polling (1s Interval)
```

---

## Vergleich: Integration vs. Device Service

| Feature | **Integration** (dein aktueller) | **Device Service** (Alternative) |
|---------|----------------------------------|----------------------------------|
| **Komplexität** | ⭐ Einfach | ⭐⭐⭐ Komplex |
| **Sprache** | ✅ C99 (bereits fertig) | Rust/C++ (gRPC erforderlich) |
| **Kommunikation** | REST API (CURL) | gRPC (Protobuf) |
| **Eigenständig** | ✅ Ja, läuft autark | ❌ Wird von CoolerControl gepollt |
| **Hardware-Zugriff** | ✅ Direkt | ✅ Direkt |
| **LCD-Upload** | ✅ Manual (REST) | ✅ Via gRPC Lcd() |
| **Status-Polling** | ❌ Dein Daemon pollt | ✅ CoolerControl pollt dich |
| **UI-Integration** | Config-Seite | Vollständiges Device in UI |

---

## Für dein Szenario: Was ist besser?

### ✅ **BLEIB bei INTEGRATION** wenn:

1. **Du willst es einfach halten**
   - Dein C-Code funktioniert bereits
   - Keine gRPC-Komplexität
   - Kein Protobuf-Kompilierung

2. **Du kontrollierst den Workflow**
   - Dein Daemon entscheidet wann Updates erfolgen
   - Refresh-Interval in config.ini
   - Kein externes Polling

3. **Du brauchst keine virtuelle Hardware**
   - Dein LCD ist bereits via CoolerControl-Device verfügbar
   - Du nutzt nur die Upload-API

### 🔄 **WECHSEL zu DEVICE SERVICE** wenn:

1. **Du willst ein virtuelles Gerät erstellen**
   - "CoolerDash Virtual Sensor" in der Device-Liste
   - CoolerControl zeigt deine Temperaturen als Channels
   - Bessere UI-Integration

2. **Du willst, dass CoolerControl dich pollt**
   - CoolerControl fragt jede Sekunde: "Wie ist der Status?"
   - Du antwortest mit Temperaturen
   - Passt besser ins CoolerControl-Datenmodell

3. **Du willst Rust lernen** 😉
   - gRPC in C99 ist möglich, aber schmerzhaft
   - Template ist in Rust
   - Mehr Code zu schreiben

---

## Empfehlung für coolerdash

### **BLEIB bei INTEGRATION** ✅

**Warum?**
1. **Funktioniert bereits**: Dein Daemon läuft, LCD wird upgedatet
2. **Einfacher**: Kein gRPC-Overhead
3. **Performanter**: Du kontrollierst Update-Frequenz selbst
4. **Native**: C99 ist perfekt für embedded-ähnliche Hardware-Tasks

**Was du noch verbessern kannst:**
- ✅ Plugin-Config-API nutzen (wie vorhin besprochen)
- ✅ UI verbessern (Einstellungen speichern)
- ✅ Manifest erweitern (Version, URL bereits drin)

---

## Wann wäre DEVICE SERVICE sinnvoll?

**Theoretisches Szenario**: Wenn du ein **virtuelles Temperatur-Device** erstellen willst:

```toml
[Device: CoolerDash Virtual Monitor]
├── Channel 1: CPU Temperature (°C)
├── Channel 2: GPU Temperature (°C)
└── Channel 3: LCD Display (output)
```

Dann würde CoolerControl:
- Dein Device in der Liste zeigen
- Temp-Channels als Sensoren nutzen (für Profiles/Functions)
- Status jede Sekunde pollen

**ABER**: Du brauchst das nicht! Du nutzt bereits existierende Sensoren.

---

## Code-Vergleich

### Integration (dein aktueller Ansatz):
```c
// main.c
while (running) {
    get_temperature_data();      // REST API Call
    render_image();              // Cairo
    upload_to_lcd();             // REST API Call
    sleep(refresh_interval);
}
```

### Device Service (Alternative):
```rust
// service.rs (Rust)
async fn status(&self, _: Request<StatusRequest>) 
    -> Result<Response<StatusResponse>, Status> 
{
    // CoolerControl pollt jede Sekunde!
    let temps = read_temperatures();
    Ok(Response::new(StatusResponse {
        devices: vec![DeviceStatus {
            channels: vec![
                Channel { name: "CPU", temp: temps.cpu },
                Channel { name: "GPU", temp: temps.gpu },
            ],
        }],
    }))
}
```

---

## Zusammenfassung

| | Integration (✅ Empfohlen) | Device Service |
|--|---------------------------|----------------|
| **Aktuell** | ✅ Ja | ❌ Nein |
| **Aufwand** | 0 (fertig) | Komplett neu schreiben |
| **Komplexität** | Niedrig | Hoch |
| **Vorteile** | Funktioniert, einfach, performant | Bessere UI-Integration, virtuelle Devices |
| **Nachteile** | Kein virtuelles Device | gRPC, Rust, Protobuf, Polling |

---

## Fazit für coolerdash

**BLEIB bei `type = "integration"`** ✅

Dein Plugin ist perfekt als Integration:
1. ✅ Funktioniert bereits
2. ✅ Direkte Hardware-Kontrolle
3. ✅ Native C99-Performance
4. ✅ Einfache Wartung
5. ✅ Keine Abhängigkeit von Rust/Protobuf

**Verbessere stattdessen**:
- Plugin-Config-API Integration (siehe vorherige Dokumentation)
- UI für Einstellungen (savePluginConfig/getPluginConfig)
- Logging/Fehlerbehandlung

**Device Service wäre nur sinnvoll wenn**:
- Du ein komplett neues virtuelles Gerät erstellen willst
- Du willst, dass CoolerControl deine Daten als native Channels behandelt
- Du bereit bist, alles neu in Rust zu schreiben

---

## Quick Check: Brauchst du Device Service?

❓ **Willst du ein neues Gerät in der CoolerControl-Device-Liste?**
- Nein → INTEGRATION ✅
- Ja → Device Service

❓ **Soll CoolerControl deine Temperaturen als Channels nutzen können?**
- Nein, ich zeige nur auf LCD → INTEGRATION ✅
- Ja, für Profiles/Functions → Device Service

❓ **Ist dir Einfachheit wichtiger als UI-Integration?**
- Ja → INTEGRATION ✅
- Nein → Device Service

**Für coolerdash: 3x INTEGRATION ✅**
