# MixxxIA — Software DJ con Inteligencia Artificial Local

[![Última versión](https://img.shields.io/github/tag/mixxxdj/mixxx.svg)](https://mixxx.org/download)
[![Estado de paquetes](https://repology.org/badge/tiny-repos/mixxx.svg)](https://repology.org/metapackage/mixxx/versions)
[![Build](https://github.com/mixxxdj/mixxx/actions/workflows/build.yml/badge.svg)](https://github.com/mixxxdj/mixxx/actions/workflows/build.yml)
[![Cobertura](https://coveralls.io/repos/github/mixxxdj/mixxx/badge.svg)](https://coveralls.io/github/mixxxdj/mixxx)
[![Chat Zulip](https://img.shields.io/badge/zulip-join_chat-brightgreen.svg)](https://mixxx.zulipchat.com)
[![Donar](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://mixxx.org/donate)

**MixxxIA** es un fork de [Mixxx][mixxx] — software DJ gratuito y de código abierto — que incorpora un **copiloto de IA completamente local**: sin nube, sin suscripciones, sin conexión a internet.

Funciona en **GNU/Linux**, **Windows** y **macOS**.

---

## Nuevas funciones de IA

### AI Suggest — Recomendaciones en tiempo real

El panel **"AI Suggest"** aparece en el sidebar de la biblioteca junto a las pestañas habituales. En cuanto cargas un track en cualquier deck, muestra automáticamente los **10 temas más compatibles** de tu librería.

```
Sonando: Levels (Radio Edit) · 125 BPM · 12A
─────────────────────────────────────────────────────────
✅ Starships – Nicki Minaj      94%  BPM 96%  KEY 100%  house      uplifting
✅ I Love It – Icona Pop        91%  BPM 98%  KEY  90%  pop        energetic
✅ Pepas – Farruko              88%  BPM 92%  KEY 100%  reggaeton  groovy
✅ One Kiss – Dua Lipa          85%  BPM 88%  KEY  80%  dance      uplifting
   We Found Love – Rihanna      72%  BPM 85%  KEY  70%  pop        romantic
```

**Qué muestra por track:**
| Columna   | Descripción |
|-----------|-------------|
| **Match** | Compatibilidad total (0–100%) |
| **BPM**   | Tempo y % de cercanía (ej. `125.0 · 96%`) |
| **Key**   | Tonalidad y compatibilidad armónica (ej. `12A · 100%`) |
| **Género**| Género detectado por IA (house, techno, reggaeton…) |
| **Mood**  | Estado de ánimo (uplifting, dark, chill, energetic…) |
| **Energía**| Nivel de energía de 0 a 1 |

Doble click en cualquier recomendación → se carga directamente al deck disponible.

El panel se actualiza **solo** cada vez que cambia el track activo.

---

### AI AutoDJ — Mezcla automática por IA

Cuando el **AutoDJ** está activo con IA habilitada, el software **elige y ordena los temas de la cola automáticamente**, sin intervención. El algoritmo de selección combina cuatro factores:

| Factor | Peso por defecto | Descripción |
|--------|-----------------|-------------|
| Similitud de audio | 50% | Coseno de los embeddings CLAP de 512 dimensiones |
| Compatibilidad armónica | 20% | Rueda de Camelot / Círculo de quintas |
| Cercanía de BPM | 20% | Diferencia máxima configurable (por defecto ±8%) |
| Continuidad de energía | 10% | Evita saltos bruscos de energía |

Tracks cuyo BPM esté demasiado alejado se **descartan automáticamente** para garantizar que el beat-match sea viable.

**Modo híbrido:** Puedes tener el AutoDJ activo y aun así ver el panel AI Suggest para anticipar qué viene, o anular la selección cargando manualmente otro tema.

---

### Solicitudes en lenguaje natural (Fase 5)

En el panel de AutoDJ puedes escribir peticiones como texto libre:

> *"pon algo de reggaeton"* · *"más energía"* · *"cambia a techno"*

Un modelo de lenguaje (local u opcional en la nube) interpreta la petición y la convierte en un género objetivo de **tu propia librería** — nunca inventa géneros que no tengas.

**Proveedores disponibles:**

| Proveedor | Requiere clave | Descarga automática |
|-----------|---------------|---------------------|
| **local (Ollama)** | No | Sí (descarga el modelo sola) |
| Google Gemini | Sí | No (nube) |
| OpenAI GPT | Sí | No (nube) |
| Anthropic Claude | Sí | No (nube) |

---

## Cómo funciona la IA (arquitectura)

```
Importas tracks → "Analyze Library" (botón estándar de Mixxx)
        ↓
AnalyzerAiFeatures  ──HTTP──>  Sidecar Python (localhost:8765)
        ↓                              ↓
   Almacena en SQLite          CLAP (alta calidad)  o  librosa (fallback)
   track_ai_features           embedding 512-d + género + mood + energía
        ↓
AiAutomixSelector  →  ordena la cola de AutoDJ
AiTrackAdvisor     →  actualiza el panel AI Suggest
```

Todo corre **en tu máquina**. Los embeddings se calculan una sola vez por track y quedan guardados en la base de datos de Mixxx.

---

## Indexar tu librería

No hay botón especial. El flujo es:

1. Inicia el sidecar de IA (ver más abajo).
2. Abre Mixxx y ve a **Library → Analyze Library**.
3. El analizador de IA se ejecuta automáticamente junto al resto de analizadores (BPM, key, waveform).
4. Cuando termine, tus tracks ya aparecerán en el panel AI Suggest y el AutoDJ podrá usarlos.

Los tracks sin análisis simplemente no aparecen en las recomendaciones — no hay errores.

---

## Instalación del sidecar de IA

El sidecar es un servicio Python local que calcula los embeddings de audio.

```bash
cd tools/ai_sidecar

# Crear entorno virtual
python -m venv .venv
source .venv/bin/activate          # Windows: .venv\Scripts\activate

# Instalar dependencias base (backend librosa, siempre funciona)
pip install -r requirements.txt

# Opcional: backend CLAP de alta calidad (requiere PyTorch)
pip install laion-clap torch

# Iniciar el sidecar
python -m uvicorn server:app --host 127.0.0.1 --port 8765
```

**Backends disponibles:**

| Backend | Calidad | Dependencias | Género/Mood |
|---------|---------|-------------|-------------|
| **CLAP** | Alta | `laion-clap`, `torch` | Sí (clasificación zero-shot) |
| librosa | Básica | solo `librosa` | No |

El sidecar detecta automáticamente si PyTorch/CLAP están disponibles y elige el mejor backend. Si tienes GPU NVIDIA, usará CUDA automáticamente.

Forzar backend: `MIXXX_AI_BACKEND=clap|librosa|auto`

---

## Configuración de IA

Parámetros en el grupo `[AI]` del archivo `mixxx.cfg`:

| Clave | Valor por defecto | Descripción |
|-------|-------------------|-------------|
| `Enabled` | false | Interruptor maestro |
| `AnalyzeEnabled` | true | Calcular embeddings al analizar |
| `AutomixEnabled` | true | IA para selección en AutoDJ |
| `SidecarUrl` | http://127.0.0.1:8765 | URL del sidecar |
| `MaxBpmFraction` | 0.08 | Diferencia máxima de BPM (8%) |
| `WeightEmbedding` | 0.5 | Peso: similitud de audio |
| `WeightKey` | 0.2 | Peso: compatibilidad armónica |
| `WeightBpm` | 0.2 | Peso: cercanía de BPM |
| `WeightEnergy` | 0.1 | Peso: continuidad de energía |
| `LlmProvider` | local | Proveedor NL: local / google / openai / anthropic |
| `LlmApiKey` | (vacío) | Clave API para proveedor en la nube |
| `LlmModel` | (vacío) | Modelo específico (vacío = el sidecar elige) |

---

## Compilar MixxxIA

### Windows (recomendado para empezar)

Ejecuta **`build_ai_windows.bat`** en la raíz del repo (requiere Visual Studio 2022 con "Desktop development with C++"). Configura MSVC, descarga dependencias y compila automáticamente.

### Linux / macOS

```bash
git clone https://github.com/davidzeus/mixxxIA.git
cd mixxxIA

# Instalar dependencias del sistema
tools/debian_buildenv.sh setup        # Debian/Ubuntu
# tools/macos_buildenv.sh setup       # macOS
# tools/rpm_buildenv.sh setup         # Fedora

# Compilar
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

### Tabla de entornos de compilación

| Plataforma | Comando | Espacio aproximado |
|-----------|---------|-------------------|
| Windows | `tools\windows_buildenv.bat` | ~9 GB |
| macOS | `source tools/macos_buildenv.sh setup` | ~3 GB |
| Debian/Ubuntu | `tools/debian_buildenv.sh setup` | ~1 GB |
| Fedora | `tools/rpm_buildenv.sh setup` | ~1 GB |
| Flatpak | `tools/flatpak_buildenv.sh setup` | ~5 GB |

---

## Créditos

### Mixxx — Proyecto original

**MixxxIA** está construido sobre [Mixxx][mixxx], software DJ libre y de código abierto desarrollado por la comunidad Mixxx desde 2002.

- **Sitio oficial:** [mixxx.org](https://mixxx.org)
- **Repositorio original:** [github.com/mixxxdj/mixxx](https://github.com/mixxxdj/mixxx)
- **Licencia:** GPLv2
- **Comunidad:** [Zulip][zulip] · [Foro][discourse] · [Mastodon][mastodon]

Todos los derechos de Mixxx pertenecen a sus respectivos autores y colaboradores. Este fork respeta íntegramente la licencia GPLv2 del proyecto original.

### MixxxIA — Extensiones de IA

Las funciones de inteligencia artificial (AI Suggest, AI AutoDJ, sidecar Python, lenguaje natural) fueron desarrolladas como extensión de código abierto sobre Mixxx.

---

## Documentación

- [Manual de Mixxx][manual]
- [Wiki de Mixxx][wiki]
- [Compatibilidad de hardware][Hardware Compatibility]
- [Documentación del AI Automix](docs/ai_automix.md)
- [Documentación del sidecar de IA](tools/ai_sidecar/README.md)

## Comunidad Mixxx

- Chat: [Zulip][zulip]
- Redes: [Mastodon][mastodon] · [Twitter][twitter] · [Facebook][facebook]
- Blog: [mixxx.org/news][blog]
- Foro: [discourse.mixxx.group][discourse]

## Licencia

MixxxIA se distribuye bajo la **GPLv2**, la misma licencia que Mixxx. Ver el archivo `LICENSE` para el texto completo.

[mixxx]: https://mixxx.org
[download-stable]: https://mixxx.org/download/#stable
[download-testing]: https://mixxx.org/download/#testing
[issues]: https://github.com/mixxxdj/mixxx/issues
[fileabug]: https://github.com/mixxxdj/mixxx/issues/new/choose
[mastodon]: https://floss.social/@mixxx
[twitter]: https://twitter.com/mixxxdj
[facebook]: https://www.facebook.com/pages/Mixxx-DJ-Software/21723485212
[blog]: https://mixxx.org/news/
[manual]: https://manual.mixxx.org/
[wiki]: https://github.com/mixxxdj/mixxx/wiki
[visualstudio2022]: https://docs.microsoft.com/visualstudio/install/install-visual-studio?view=vs-2022
[easybugs]: https://github.com/mixxxdj/mixxx/issues?q=is%3Aopen+is%3Aissue+label%3Aeasy
[creating skins]: https://mixxx.org/wiki/doku.php/Creating-Skins
[help translate content]: https://explore.transifex.com/mixxx-dj-software/
[Mixxx i18n wiki]: https://github.com/mixxxdj/mixxx/wiki/Internationalization
[Mixxx localization forum]: https://mixxx.discourse.group/c/translation/13
[hardware compatibility]: https://manual.mixxx.org/2.3/en/hardware/manuals.html
[zulip]: https://mixxx.zulipchat.com/
[discourse]: https://mixxx.discourse.group/
