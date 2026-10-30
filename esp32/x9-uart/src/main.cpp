/* X9 UART bridge / sniffer — ESP32-S3
 *
 * Sits between the projector's 3-pad console header and the Mac.
 *
 *   X9 TX  ->  GPIO47  (UART1 RX)          [required]  (right header, "47")
 *   X9 RX  <-  GPIO17  (UART1 TX)          [optional, off in sniff mode; unwired]
 *   Probe trio (all inputs): GPIO47=blue, GPIO21=yellow, GPIO41=red.
 *   Right-header pins only: 35/36/37 = Octal PSRAM, 19/20 = USB, 0/45/46 = straps,
 *   48 = LED, 43/44 = console UART0. Free: 47, 21, 41, 42, 40, 39, 38, 1.
 *   GND    <-> GND                         [always]
 *   never connect the X9 3V3 to the ESP32
 *
 * Two modes:
 *   BRIDGE (default)  transparent 115200 8N1 passthrough, plus a RAM ring buffer of
 *                     everything the projector printed, so a boot captured while the
 *                     Mac was not attached can still be retrieved.
 *   SNIFF             RX only, never transmits. Reports activity/edge counts and a baud
 *                     estimate, so a candidate pad can be identified without a scope.
 *
 * Strap: hold GPIO0 low at reset (BOOT button) -> SNIFF mode. Otherwise BRIDGE.
 *
 * Escape on the USB side: Ctrl-] (0x1D) enters the command prompt. Command lines are
 * consumed by the ESP32 and NEVER forwarded to the projector, so nothing you type in
 * the prompt can disturb the console.
 */
#include <Arduino.h>
#include <esp_attr.h>
#include <string.h>

static const int PIN_RX_DEFAULT = 21;   // X9 TX wire is fixed on GPIO21 now
static int pin_rx = PIN_RX_DEFAULT; // <- X9 TX pad; runtime change via 'rx <gpio>'
static const int PIN_TX = 47;   // -> X9 RX pad (blue wire, silk-RX footprint)
static const int PIN_STRAP = 0; // BOOT button

static const uint32_t DEFAULT_BAUD = 115200;

enum Mode { MODE_BRIDGE, MODE_SNIFF };

/* ---------- capture ring ---------- */
static const size_t RING_SZ = 64 * 1024;
static uint8_t  ring[RING_SZ];
static size_t   ring_head = 0;      // next write
static size_t   ring_len = 0;
static uint32_t ring_overflow = 0;

static void ring_put(uint8_t b)
{
    ring[ring_head] = b;
    ring_head = (ring_head + 1) % RING_SZ;
    if (ring_len < RING_SZ) ring_len++;
    else ring_overflow++;
}

/* ---------- outbound queue (USB side) ----------
 * Serial.write() blocks when the host is absent or slow. If we forwarded inline, UART1
 * would never be drained and the capture ring would silently lose everything -- which is
 * the one job the ring exists for. So: always drain UART1 into the ring, and forward to
 * USB only as fast as the host actually accepts. */
static const size_t USBQ_SZ = 16 * 1024;
static uint8_t  usbq[USBQ_SZ];
static size_t   usbq_head = 0, usbq_len = 0;
static uint32_t usbq_dropped = 0;

static inline void usbq_put(uint8_t b)
{
    if (usbq_len < USBQ_SZ) { usbq[(usbq_head + usbq_len) % USBQ_SZ] = b; usbq_len++; }
    else usbq_dropped++;               /* host not reading; the ring still has it */
}

/* ---------- RTC-persistent capture ----------
 * The RAM ring dies on reset, and opening /dev/cu.* with screen asserts DTR/RTS -> the CH340
 * pulses the auto-reset circuit. That is precisely the moment you want the log. So mirror the
 * first bytes into RTC no-init memory: it survives reset (not power loss) and is replayed on
 * the next boot, before the bridge starts. */
#define RTC_MAGIC 0x58394C47u   /* "X9LG" */
static const size_t RTC_SZ = 7168;

RTC_NOINIT_ATTR static struct {
    uint32_t magic;
    uint32_t len;
    uint8_t  data[RTC_SZ];
} rtc_log;

/* Truncation must never look like a complete capture: these bytes become firmware evidence. */
RTC_NOINIT_ATTR static uint32_t rtc_dropped;

static void rtc_capture(uint8_t b)
{
    if (rtc_log.magic != RTC_MAGIC) { rtc_log.magic = RTC_MAGIC; rtc_log.len = 0; rtc_dropped = 0; }
    if (rtc_log.len < RTC_SZ) rtc_log.data[rtc_log.len++] = b;
    else rtc_dropped++;
}

/* ---------- stats ---------- */
static volatile uint32_t rx_bytes = 0, tx_bytes = 0;
static uint32_t baud = DEFAULT_BAUD;
static Mode mode = MODE_BRIDGE;

/* ================= bridge ================= */
static void probe_pin(uint32_t seconds);   /* defined below */
static void check_wiring();                /* defined below */

static bool cmd_mode = false;
static bool tx_enabled = false;
static char cmdline[96];
static size_t cmdlen = 0;

static const int PROBE_PINS[2] = { 21, 47 };   // new UART-footprint wires, TX/RX order unknown
static const char *PROBE_NAMES[2] = { "gpio21", "gpio47" };
/* Passive snapshot only: digital inputs cannot establish continuity, ground, or voltage. */
static void check_wiring()
{
    Serial1.end();
    pinMode(PIN_TX, INPUT);
    tx_enabled = false;
    for (int i = 0; i < 2; i++) pinMode(PROBE_PINS[i], INPUT);
    Serial.println(F("# check: passive inputs only; no pull-ups or pull-downs"));
    for (int i = 0; i < 2; i++) {
        unsigned high = 0;
        for (int n = 0; n < 64; n++) {
            high += digitalRead(PROBE_PINS[i]) ? 1 : 0;
            delayMicroseconds(20);
        }
        Serial.printf("# %s GPIO%d: HIGH samples=%u/64\n", PROBE_NAMES[i], PROBE_PINS[i], high);
    }
    Serial.println(F("# LOW may mean unpowered circuitry, a clamp, ground, or a floating input."));
    Serial.println(F("# This does NOT verify continuity or exclude shorts. Use an unpowered meter check."));
    Serial.println(F("# UART1 stays detached; probe/check do not capture UART bytes."));
}

static void print_help()
{
    Serial.println();
    Serial.println(F("commands: help | stat | clear | check | dump | dumprtc | clearrtc | probe [s] | baud <n> | sniff | bridge | reboot"));
    Serial.println(F("  dump   = replay captured projector output from the RAM ring"));
    Serial.println(F("  probe [s] = passive edge counts on GPIO47/21/41 (blue/yellow/red), not UART identification"));
    Serial.println(F("  check     = passive input snapshot; continuity needs a meter"));
    Serial.println(F("  sniff  = RX only, never transmit (safe pad probing)"));
    Serial.println(F("  Ctrl-] exits the prompt and resumes the bridge"));
    Serial.println();
}

static void dump_ring()
{
    if (ring_len == 0 && rtc_log.magic == RTC_MAGIC && rtc_log.len) {
        Serial.printf("# RAM ring empty; replaying %u bytes from RTC (previous boot)\n",
                      (unsigned)rtc_log.len);
        for (uint32_t i = 0; i < rtc_log.len; i++) Serial.write(rtc_log.data[i]);
        Serial.println();
        Serial.println(F("# end rtc"));
        return;
    }
    Serial.printf("# ring: %u bytes, overflow %u, baud %u%s\n",
                  (unsigned)ring_len, (unsigned)ring_overflow,
                  (unsigned)baud, cmd_mode ? "" : " (bridge)");
    size_t start = (ring_len == RING_SZ) ? ring_head : 0;
    for (size_t i = 0; i < ring_len; i++) {
        uint8_t b = ring[(start + i) % RING_SZ];
        Serial.write(b);                       /* raw: it is a console log, not hex */
    }
    Serial.println();
    Serial.println(F("# end of ring"));
}

static void print_stat()
{
    Serial.printf("# mode=%s%s baud=%u rxpin=%d rx=%u tx=%u ring=%u/%u ovf=%u usbq=%u drop=%u heap=%u\n",
                  mode == MODE_BRIDGE ? "bridge" : "sniff", cmd_mode ? "+PROMPT" : "",
                  (unsigned)baud, pin_rx,
                  (unsigned)rx_bytes, (unsigned)tx_bytes,
                  (unsigned)ring_len, (unsigned)RING_SZ, (unsigned)ring_overflow,
                  (unsigned)usbq_len, (unsigned)usbq_dropped,
                  (unsigned)ESP.getFreeHeap());
}

static void run_command(char *line)
{
    while (*line == ' ') line++;
    if (!*line) { return; }
    if (!strcmp(line, "help"))   { print_help(); return; }
    if (!strcmp(line, "stat"))   { print_stat(); return; }
    if (!strcmp(line, "check")) { check_wiring(); return; }
    if (!strncmp(line, "probe", 5)) {
        uint32_t secs = 15;
        if (line[5] == ' ') secs = strtoul(line + 6, NULL, 10);
        if (secs < 2 || secs > 120) secs = 15;
        probe_pin(secs);
        return;
    }
    if (!strcmp(line, "dumprtc")) {
        if (rtc_log.magic != RTC_MAGIC || !rtc_log.len) { Serial.println(F("# rtc log empty")); return; }
        Serial.printf("# rtc: %u bytes from the previous boot%s\n", (unsigned)rtc_log.len,
                      rtc_dropped ? " -- TRUNCATED, bytes were dropped" : "");
        if (rtc_dropped)
            Serial.printf("# rtc TRUNCATED: %u bytes dropped after the %u kept\n",
                          (unsigned)rtc_dropped, (unsigned)rtc_log.len);
        for (uint32_t i = 0; i < rtc_log.len; i++) Serial.write(rtc_log.data[i]);
        Serial.println();
        Serial.println(F("# end rtc"));
        return;
    }
    if (!strcmp(line, "dumprtcx")) {
        if (rtc_log.magic != RTC_MAGIC || !rtc_log.len) { Serial.println(F("# rtc log empty")); return; }
        Serial.printf("# rtc hex: %u bytes%s\n", (unsigned)rtc_log.len,
                      rtc_dropped ? " TRUNCATED" : "");
        for (uint32_t i = 0; i < rtc_log.len; i += 16) {
            char ln[8 + 48 + 18 + 2]; size_t o = 0;
            o += sprintf(ln + o, "%04x ", (unsigned)i);
            for (uint32_t j = 0; j < 16; j++)
                o += sprintf(ln + o, (i + j < rtc_log.len) ? "%02x " : "   ",
                             (i + j < rtc_log.len) ? rtc_log.data[i + j] : 0);
            for (uint32_t j = 0; j < 16 && i + j < rtc_log.len; j++) {
                uint8_t b = rtc_log.data[i + j];
                ln[o++] = (b >= 0x20 && b < 0x7f) ? b : '.';
            }
            ln[o] = 0;
            Serial.println(ln);
            if (!(i & 0x3FF)) delay(5);
        }
        Serial.println(F("# end rtc hex"));
        return;
    }
    if (!strcmp(line, "clearrtc")) { rtc_log.magic = 0; rtc_log.len = 0; rtc_dropped = 0; Serial.println(F("# rtc log cleared")); return; }
    if (!strcmp(line, "clear"))  { ring_head = ring_len = ring_overflow = 0; Serial.println(F("# cleared")); return; }
    if (!strcmp(line, "dump"))   { dump_ring(); return; }
    if (!strcmp(line, "dumpx")) {
        Serial.printf("# ring hex: %u bytes, overflow %u\n", (unsigned)ring_len, (unsigned)ring_overflow);
        size_t start = (ring_len == RING_SZ) ? ring_head : 0;
        for (size_t i = 0; i < ring_len; i += 16) {
            char line[8 + 16*3 + 18 + 2]; size_t o = 0;
            o += sprintf(line + o, "%04x ", (unsigned)i);
            for (size_t j = 0; j < 16; j++) {
                if (i + j < ring_len) o += sprintf(line + o, "%02x ", ring[(start + i + j) % RING_SZ]);
                else o += sprintf(line + o, "   ");
            }
            for (size_t j = 0; j < 16 && i + j < ring_len; j++) {
                uint8_t b = ring[(start + i + j) % RING_SZ];
                line[o++] = (b >= 0x20 && b < 0x7f) ? b : '.';
            }
            line[o] = 0;
            Serial.println(line);
            if (!(i & 0xFFF)) delay(5);   /* let USB drain */
        }
        Serial.println(F("# end ring hex"));
        return;
    }
    if (!strcmp(line, "sniff"))  { mode = MODE_SNIFF; cmd_mode = false; Serial1.end(); Serial.println(F("# sniff mode: RX only")); return; }
    if (!strcmp(line, "bridge")) { mode = MODE_BRIDGE; cmd_mode = false; Serial1.begin(baud, SERIAL_8N1, pin_rx, PIN_TX); Serial.println(F("# bridge mode, prompt OFF")); return; }
    if (!strcmp(line, "reboot")) { Serial.println(F("# rebooting")); delay(100); ESP.restart(); }
    if (!strcmp(line, "break")) {
        Serial1.end();
        pinMode(PIN_TX, OUTPUT);
        digitalWrite(PIN_TX, LOW);            /* break condition */
        delay(350);
        digitalWrite(PIN_TX, HIGH);
        delay(5);
        Serial1.begin(baud, SERIAL_8N1, pin_rx, PIN_TX);
        tx_enabled = false;
        Serial.println(F("# break sent (350ms)"));
        return;
    }
    if (!strncmp(line, "sysrq ", 6)) {
        char k = line[6];
        if (!k) { Serial.println(F("# sysrq <key>")); return; }
        Serial.printf("# sysrq '%c'\n", k);
        Serial1.end();
        pinMode(PIN_TX, OUTPUT);
        digitalWrite(PIN_TX, LOW);
        delay(350);
        digitalWrite(PIN_TX, HIGH);
        delay(5);
        Serial1.begin(baud, SERIAL_8N1, pin_rx, PIN_TX);
        tx_enabled = true;
        delay(20);
        Serial1.write((uint8_t)k);
        tx_bytes++;
        Serial.println(F("# sent"));
        return;
    }
    if (!strncmp(line, "rx ", 3)) {
        int p = atoi(line + 3);
        if (p == 20 || p == 21 || p == 47) {
            pin_rx = p;
            if (mode == MODE_BRIDGE) { Serial1.end(); Serial1.begin(baud, SERIAL_8N1, pin_rx, PIN_TX); }
            Serial.printf("# capture RX = GPIO%d\n", pin_rx);
        } else Serial.println(F("# rx: only 20, 21 or 47"));
        return;
    }
    if (!strncmp(line, "baud ", 5)) {
        uint32_t b = strtoul(line + 5, NULL, 10);
        if (b >= 300 && b <= 3000000) {
            baud = b;
            if (mode == MODE_BRIDGE) { Serial1.end(); Serial1.begin(baud, SERIAL_8N1, pin_rx, PIN_TX); }
            Serial.printf("# baud=%u\n", (unsigned)baud);
        } else Serial.println(F("# bad baud"));
        return;
    }
    Serial.printf("# unknown: %s\n", line);
}

/* ================= sniff ================= */
/* Sample the RX pin as fast as we can and describe what we see. This is the
 * "is this pad the console TX?" test that needs no scope: an idle UART sits high,
 * and a booting one bursts low. */
static void sniff_step()
{
    uint32_t level_changes = 0, lows = 0;
    uint32_t t0 = micros();
    int prev = digitalRead(pin_rx);
    uint32_t samples = 0;
    while (micros() - t0 < 100000) {          /* 100 ms window */
        int v = digitalRead(pin_rx);
        samples++;
        if (v != prev) { level_changes++; prev = v; }
        if (!v) lows++;
    }
    uint32_t dt = micros() - t0;
    Serial.printf("# sniff %6u samples/%uus  edges=%u  low=%u%%  %s\n",
                  (unsigned)samples, (unsigned)dt, (unsigned)level_changes,
                  (unsigned)((uint64_t)lows * 100 / (samples ? samples : 1)),
                  level_changes ? "<-- ACTIVITY" : "quiet");
}


/* Passive three-pin activity measurement. Edges and pulse widths are observations,
 * not evidence of UART framing or a reliable baud estimate at this polling rate.
 * No edges cannot exclude a quiet, disabled, or below-threshold console.
 * Wiring: GPIO47=blue, GPIO21=yellow, GPIO41=red.
 */

static void probe_pin(uint32_t seconds)
{
    Serial.printf("# probing GPIO21/47 (new RX/TX wires) for %us -- power-cycle the X9 now\n",
                  (unsigned)seconds);
    Serial1.end();
    pinMode(PIN_TX, INPUT);
    tx_enabled = false;
    for (int i = 0; i < 2; i++) pinMode(PROBE_PINS[i], INPUT);

    uint32_t edges[3] = {0,0,0}, hi[3] = {0,0,0}, samples = 0;
    uint32_t min_pulse_us[3] = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
    uint32_t first_edge_ms[3] = {0,0,0};
    uint32_t max_gap_ms[3] = {0,0,0};          /* longest quiet stretch */
    uint32_t last_edge_ms[3] = {0,0,0};
    uint32_t bucket[3][120] = {0};             /* edges per 1s bucket */
    int prev[3];
    uint32_t last_change[3];
    for (int i = 0; i < 2; i++) { prev[i] = digitalRead(PROBE_PINS[i]); last_change[i] = micros(); }

    uint32_t t0 = millis();
    uint32_t nsec = seconds < 120 ? seconds : 120;
    while (millis() - t0 < seconds * 1000) {
        uint32_t now = micros(), now_ms = millis();
        uint32_t sec = (now_ms - t0) / 1000;
        samples++;
        for (int i = 0; i < 2; i++) {
            int v = digitalRead(PROBE_PINS[i]);
            if (v) hi[i]++;
            if (v != prev[i]) {
                uint32_t w = now - last_change[i];
                if (edges[i] && w < min_pulse_us[i]) min_pulse_us[i] = w;
                last_change[i] = now;
                if (!edges[i]) first_edge_ms[i] = now_ms - t0;
                else {
                    uint32_t gap = (now_ms - t0) - last_edge_ms[i];
                    if (gap > max_gap_ms[i]) max_gap_ms[i] = gap;
                }
                last_edge_ms[i] = now_ms - t0;
                if (sec < nsec) bucket[i][sec]++;
                edges[i]++;
                prev[i] = v;
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        uint32_t max_b = 0;
        for (uint32_t k = 0; k < nsec; k++) if (bucket[i][k] > max_b) max_b = bucket[i][k];
        Serial.printf("# %s GPIO%u: edges=%u duty=%.1f%% first=%ums maxgap=%ums busiest_s=%u",
                      PROBE_NAMES[i], (unsigned)PROBE_PINS[i], (unsigned)edges[i],
                      100.0 * hi[i] / (samples ? samples : 1),
                      (unsigned)first_edge_ms[i], (unsigned)max_gap_ms[i], (unsigned)max_b);
        if (edges[i] > 1)
            Serial.printf(" min_observed_pulse=%uus", (unsigned)min_pulse_us[i]);
        Serial.println(F(" -> activity only; UART identity and baud unconfirmed"));
        Serial.printf("#   edges/s:");
        for (uint32_t k = 0; k < nsec && k < 30; k++) Serial.printf(" %u", (unsigned)bucket[i][k]);
        Serial.println();
    }
    Serial.println(F("# probe done: UART1 stays detached; all probe pins remain inputs."));
    Serial.println(F("# No UART bytes were captured during this probe. Silence does not rule out TX."));
}

/* ================= setup/loop ================= */
void setup()
{
    pinMode(PIN_STRAP, INPUT_PULLUP);
    for (int i = 0; i < 2; i++) pinMode(PROBE_PINS[i], INPUT);
    delay(30);
    bool strap_low = (digitalRead(PIN_STRAP) == LOW);

    Serial.begin(115200);                  /* USB/CH340 side */
    delay(120);

    Serial.println();
    Serial.println(F("=== X9 UART bridge (ESP32-S3) ==="));
    uint64_t ef = ESP.getEfuseMac();
    Serial.printf("MAC %02x:%02x:%02x:%02x:%02x:%02x  RX=GPIO%d TX=GPIO%d\n",
                  (unsigned)(ef) & 0xff, (unsigned)(ef >> 8) & 0xff,
                  (unsigned)(ef >> 16) & 0xff, (unsigned)(ef >> 24) & 0xff,
                  (unsigned)(ef >> 32) & 0xff, (unsigned)(ef >> 40) & 0xff,
                  pin_rx, PIN_TX);
    Serial.println(F("Ctrl-] for the command prompt; 'help' lists commands."));

    if (rtc_log.magic == RTC_MAGIC && rtc_log.len > 0 && rtc_log.len <= RTC_SZ) {
        Serial.printf("# --- previous boot, %u bytes from RTC%s ---\n", (unsigned)rtc_log.len,
                      rtc_dropped ? ", TRUNCATED" : "");
        for (uint32_t i = 0; i < rtc_log.len; i++) Serial.write(rtc_log.data[i]);
        Serial.println();
        if (rtc_dropped)
            Serial.printf("# TRUNCATED: %u bytes were dropped after the %u kept\n",
                          (unsigned)rtc_dropped, (unsigned)rtc_log.len);
        Serial.println(F("# --- end previous boot (send 'clearrtc' to drop it) ---"));
    }

    if (strap_low) {
        mode = MODE_SNIFF;
        pinMode(pin_rx, INPUT);            /* high-Z, listen only */
        Serial.println(F("strap held -> SNIFF mode (RX only, never transmits)"));
    } else {
        mode = MODE_BRIDGE;
        /* start with TX high-Z (input): an unpowered X9 must not be back-fed through its
         * RX protection diode before its own rail is up. Output is enabled lazily, on the
         * first byte we actually have to send. */
        pinMode(PIN_TX, INPUT);
        tx_enabled = false;
        Serial1.begin(baud, SERIAL_8N1, pin_rx, PIN_TX);
        Serial.printf("# bridge ready at %u 8N1 (TX high-Z until first send)\n", (unsigned)baud);
    }
}

void loop()
{
    if (mode == MODE_SNIFF) { sniff_step(); return; }

    /* ---- projector -> ring (always), then ring -> USB (only as fast as the host reads) ---- */
    while (Serial1.available()) {
        int c = Serial1.read();
        if (c < 0) break;
        ring_put((uint8_t)c);
        rtc_capture((uint8_t)c);
        usbq_put((uint8_t)c);
        rx_bytes++;
    }
    while (usbq_len && Serial.availableForWrite() > 0) {
        Serial.write(usbq[usbq_head]);               /* FIFO: consume at head ... */
        usbq_head = (usbq_head + 1) % USBQ_SZ;       /* ... and ADVANCE it */
        usbq_len--;
    }

    /* ---- USB -> projector (with the Ctrl-] escape) ---- */
    while (Serial.available()) {
        int c = Serial.read();
        if (c < 0) break;

        if (c == 0x1D) {                   /* Ctrl-] */
            cmd_mode = !cmd_mode;
            cmdlen = 0;
            Serial.println(cmd_mode ? F("\n# prompt (Ctrl-] to resume bridge)")
                                    : F("\n# bridge resumed"));
            if (cmd_mode) print_help();
            continue;
        }

        if (cmd_mode) {
            if (c == '\r' || c == '\n') {
                if (cmdlen == 0) continue;   /* bare CR/LF or the second half of CRLF */
                cmdline[cmdlen] = 0;
                run_command(cmdline);
                cmdlen = 0;
            } else if (cmdlen < sizeof cmdline - 1) {
                cmdline[cmdlen++] = (char)c;
                Serial.write((uint8_t)c);   /* local echo */
            }
            continue;
        }

        /* NOTE: do NOT pinMode() here — on core 3.x pinMode clears the pin's
         * peripheral bus and silently detaches UART1 TX. Serial1.begin() already
         * configured the pin; leaving the UART attached is what makes TX real. */
        tx_enabled = true;
        Serial1.write((uint8_t)c);          /* transparent */
        tx_bytes++;
    }
}
