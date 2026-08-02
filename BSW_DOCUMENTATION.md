====== BSW (Basic Software) - Comprehensive Technical Documentation ======

===== Quick Reference =====

| Module | Purpose | Thread-Safe | Files |
|--------|---------|------------|-------|
| **CoreTask** | FreeRTOS wrapper | Yes | core_task.hpp/.cpp |
| **Gpio** | Individual pins, PWM | No | gpio.hpp/.cpp |
| **GpioController** | Pin management | Partial | gpio_controller.hpp/.cpp |
| **Uart** | Serial communication | Yes | uart.hpp/.cpp |
| **Spi** | SPI Master | Yes | spi.hpp/.cpp |
| **I2c** | I2C Master/Slave | Yes | i2c.hpp/.cpp |
| **Scheduler** | Periodic tasks | Yes | scheduler.hpp/.cpp |
| **SchedulerTask** | Task entry | N/A | scheduler_task.hpp/.cpp |
| **Nvram** | Flash storage | Yes | nvram.hpp/.cpp |
| **Wifi** | Network | Yes | wifi.hpp/.cpp |
| **Ota** | Firmware updates | N/A | ota.hpp/.cpp |
| **Time** | NTP/RTC | Yes | time.hpp/.cpp |
| **Watchdog** | Hardware protection | Yes | watchdog.hpp/.cpp |

---

===== 1. CoreTask - FreeRTOS Task Management =====

**Purpose:** Type-safe wrapper for FreeRTOS task creation.

**API:**
<code cpp>
class CoreTask {
public:
    bool create(void (*task_fn)(void*), const char* name,
                uint16_t stack_size, void* param,
                uint8_t priority, uint8_t core) noexcept;
    TaskHandle_t getHandle() const noexcept;
};
</code>

**Stack Size Reference:**
  * Simple GPIO control: 1024 bytes
  * UART communication: 1024-1536 bytes
  * Sensor reading (I2C/SPI): 2048 bytes
  * WiFi tasks: 4096-6144 bytes
  * Complex algorithms: 3072+ bytes

**Monitor Stack:**
<code cpp>
UBaseType_t free = uxTaskGetStackHighWaterMark(NULL);
printf("Remaining stack: %u bytes\n", free);
if (free < 512) printf("WARNING: Low stack\n");
</code>

**Common Priorities (0-24):**
  * 0: Idle tasks
  * 1-3: Background work
  * 5: Standard tasks (typical)
  * 10-15: Time-critical
  * 24: Highest priority (usually ISRs)

---

===== 2. Gpio & GpioController =====

**Purpose:** Control individual GPIO pins with digital I/O and PWM.

**GpioController Role:** Centralized pin management, create only ONE instance.

**Gpio API - Digital I/O:**
<code cpp>
void init();
void setDirection(GpioDirection dir);        // kInput / kOutput
void setPullMode(GpioPullMode mode);          // kNone / kUp / kDown
void setState(GpioState state);               // kLow / kHigh
GpioState getState();
void toggleGpioState();
</code>

**Gpio API - PWM:**
<code cpp>
void initPwm(uint32_t freq, uint8_t duty, uint8_t ch, uint8_t timer);
void setPwmDuty(uint8_t duty, bool brightness=false, bool inverted=false);
void setPwmFreq(uint32_t freq);
uint32_t getPwmFrequency() const;
uint8_t getPwmDutyCycle() const;
</code>

**Example - Toggle LED:**
<code cpp>
bsw::Gpio led(gpio_ctl, 27, bsw::GpioDirection::kOutput,
              bsw::GpioPullMode::kNone, bsw::GpioState::kHigh);
led.init();
led.setState(bsw::GpioState::kLow);   // Turn on
led.toggleGpioState();                 // Toggle
</code>

**Example - PWM (Brightness Control):**
<code cpp>
bsw::Gpio pwm_led(gpio_ctl, 27, bsw::GpioDirection::kOutput,
                  bsw::GpioPullMode::kNone, bsw::GpioState::kLow,
                  0, 0, 1000, 50);     // 1kHz, 50% duty
pwm_led.init();
pwm_led.setPwmDuty(100);              // 100% brightness
pwm_led.setPwmDuty(25);               // 25% brightness
</code>

**Thread-Safety:** NOT thread-safe. Protect with mutex for concurrent access:
<code cpp>
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

void gpio_safe_write(bsw::Gpio& pin, bsw::GpioState state) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    pin.setState(state);
    xSemaphoreGive(mutex);
}
</code>

---

===== 3. UART - Serial Communication =====

**Purpose:** Manage UART0/1/2 serial ports.

**Configuration Structure:**
<code cpp>
struct Config {
    Module module;          // kUart0, kUart1, kUart2
    DataBits data_bits;     // kDataBits5 through kDataBits8
    StopBits stop_bits;     // kStopBits1, kStopBits1_5, kStopBits2
    Parity parity;          // kParityDisable, kParityEven, kParityOdd
    uint32_t baud_rate;     // e.g., 115200
    FlowControl flow_ctrl;  // kNone or RTS/CTS
    uint8_t tx_pin;         // GPIO for TX
    uint8_t rx_pin;         // GPIO for RX
    uint16_t rx_buf_size;   // Receive buffer (default 256)
};
</code>

**API:**
<code cpp>
void init();
size_t write(const uint8_t* data, size_t len);    // Blocking
size_t read(uint8_t* buf, size_t len, uint32_t timeout_ms);
void flush();
void setBaudRate(uint32_t baud);
</code>

**Common Settings:**
  * **9600** - Modbus, GPS, legacy devices
  * **115200** - Debug, most microcontrollers (default)
  * **19200** - Industrial equipment
  * **38400, 57600** - High-speed devices

**Example:**
<code cpp>
bsw::Uart::Config cfg {
    .module = bsw::Uart::Module::kUart1,
    .data_bits = bsw::Uart::DataBits::kDataBits8,
    .stop_bits = bsw::Uart::StopBits::kStopBits1,
    .parity = bsw::Uart::Parity::kParityDisable,
    .baud_rate = 115200,
    .tx_pin = 17,
    .rx_pin = 16
};

bsw::Uart uart(cfg);
uart.init();
uart.write((uint8_t*)"Hello", 5);

uint8_t buf[64];
size_t n = uart.read(buf, 64, 1000);  // 1 second timeout
</code>

**Thread-Safety:** YES - Uses FreeRTOS queues internally.

---

===== 4. SPI - High-Speed Serial =====

**Purpose:** SPI Master communication for shift registers, SD cards, etc.

**SPI Modes (CPOL/CPHA):**

| Mode | Use | Clock Polarity | Phase |
|------|-----|----------------|-------|
| 0 | Shift registers, SD cards (**most common**) | 0 | 0 |
| 1 | Some sensors | 0 | 1 |
| 2 | Rare | 1 | 0 |
| 3 | Some displays | 1 | 1 |

**Clock Speed Guidance:**
  * 1 MHz - Maximum noise immunity, longest cables
  * 10 MHz - Standard (shift registers, most sensors)
  * 40 MHz - High-speed, on-board only

**Configuration:**
<code cpp>
struct Config {
    Host host;              // kSpi1, kSpi2, kSpi3
    Mode mode;              // 0-3
    uint8_t mosi_pin;       // Master Out (output)
    uint8_t miso_pin;       // Master In (input)
    uint8_t sck_pin;        // Serial Clock (output)
    int8_t cs_pin;          // Chip Select (-1 for manual)
    uint32_t clock_speed;   // Hz
    uint16_t queue_size;    // Transfer queue (7 typical)
};
</code>

**API:**
<code cpp>
void init();
void transmit(const uint8_t* tx, uint8_t* rx, size_t len, uint32_t timeout_ms);
void write(const uint8_t* data, size_t len);    // MISO ignored
void read(uint8_t* buf, size_t len);            // MOSI zeros
void setClockSpeed(uint32_t freq);
</code>

**Shift Register Example (8-bit valve control):**
<code cpp>
bsw::Spi spi{{.host = bsw::Spi::Host::kSpi3,
               .mode = bsw::Spi::Mode::kMode0,
               .mosi_pin = 23, .miso_pin = 19, .sck_pin = 18, .cs_pin = 5,
               .clock_speed = 10000000}};
spi.init();

uint8_t pattern = 0b10101010;  // Alternating valves
uint8_t dummy_rx[1];
spi.transmit(&pattern, dummy_rx, 1, 100);
</code>

**Thread-Safety:** YES - Semaphore-protected exclusive access.

---

===== 5. I2C - TWI Bus Communication =====

**Purpose:** I2C Master/Slave for sensors and peripherals.

**Clock Speeds:**
  * 100 kHz - Standard (long cables, all devices)
  * 400 kHz - Fast (most common)
  * 1 MHz - Fast+ (short cables only)

**Address Modes:**
  * 7-bit (standard): 0x00-0x7F (most common)
  * 10-bit (extended): 0x000-0x3FF (rare)

**Common Devices:**

| Device | Address | Clock | Type |
|--------|---------|-------|------|
| DS3231 RTC | 0x68 | 100kHz | Real-time clock |
| BMP280 | 0x76/0x77 | 400kHz | Pressure/temp sensor |
| ADS1115 ADC | 0x48-0x4B | 400kHz | 16-bit ADC |
| PCF8574 GPIO Exp | 0x20-0x27 | 100kHz | 8-bit I/O expander |
| MPU6050 IMU | 0x68/0x69 | 400kHz | Accelerometer/gyro |

**Configuration:**
<code cpp>
struct Config {
    Module module;          // kI2c0, kI2c1, kLpI2c0
    uint8_t sda_pin;
    uint8_t scl_pin;
    AddrMode addr_mode;     // kAddr7Bit or kAddr10Bit
    uint32_t clk_speed;     // 100k, 400k, or 1M Hz
    bool pull_up_enabled;   // Internal pull-ups
    BusMode bus_mode;       // kMaster or kSlave
};
</code>

**API:**
<code cpp>
void init();
esp_err_t master_write(uint8_t addr, const uint8_t* data, size_t len);
esp_err_t master_read(uint8_t addr, const uint8_t* reg, size_t reg_len,
                      uint8_t* buf, size_t buf_len);
bool probe(uint8_t addr);                       // Check device presence
void setClockSpeed(uint32_t freq);
</code>

**Example - Temperature Sensor:**
<code cpp>
bsw::I2c i2c{{.module = bsw::I2c::Module::kI2c0,
               .sda_pin = 21, .scl_pin = 22,
               .addr_mode = bsw::I2c::AddrMode::kAddr7Bit,
               .clk_speed = 100000}};
i2c.init();

uint8_t addr = 0x48;
uint8_t reg = 0x00;
uint8_t data[2];

if (i2c.master_read(addr, &reg, 1, data, 2) == ESP_OK) {
    int16_t raw = (data[0] << 8) | data[1];
    float temp = raw * 0.0625f;
    printf("Temperature: %.2f°C\n", temp);
}
</code>

**Thread-Safety:** YES - Mutex-protected bus access.

---

===== 6. Scheduler - Periodic Task Execution =====

**Purpose:** Manage up to 32 periodic tasks with watchdog protection.

**Architecture:**
```
Hardware Timer (1ms default)
    ↓
Scheduler ISR (increments tick)
    ↓
Scheduler Worker Task (checks due tasks)
    ↓
Execute Task Callbacks
    ↓
Feed Watchdog
```

**API:**
<code cpp>
Scheduler(uint32_t period_us, uint32_t watchdog_timeout_ms);
uint16_t init_timer();
bool start_on_core(uint8_t core, uint8_t id, uint8_t priority);
bool add_task(const SchedulerTask& task);
void suspend();
void resume();
</code>

**SchedulerTask:**
<code cpp>
SchedulerTask(uint8_t id, uint32_t period_ms, void (*callback)(void*));
</code>

**Example:**
<code cpp>
void sensor_read(void* param) {
    printf("Reading sensors\n");
}

void heartbeat(void* param) {
    printf("System alive\n");
}

// Setup
static bsw::Scheduler sched(1000, 100);  // 1ms ticks, 100ms watchdog
sched.init_timer();

bsw::SchedulerTask t1(1, 5000, sensor_read);    // 5 seconds
bsw::SchedulerTask t2(2, 10000, heartbeat);     // 10 seconds

sched.add_task(t1);
sched.add_task(t2);
sched.start_on_core(1, 100, 5);                 // Core 1, priority 5
</code>

**Key Points:**
  * Executes tasks on dedicated CPU core
  * Default 1ms period (configurable in microseconds)
  * Timing jitter: ±1 tick
  * Long-running tasks block subsequent tasks
  * Watchdog automatically fed after each task cycle

---

===== 7. NVRAM - Persistent Flash Storage =====

**Purpose:** Store configuration to flash using NVS (Non-Volatile Storage).

**Organization:**
  * Namespaces: Logical grouping
  * Key-value pairs: Keys are strings (max 15 chars)
  * Types: u8/16/32/64, i32/64, float/double, string, blob

**API:**
<code cpp>
static esp_err_t system_init();           // Call once at startup
Nvram(const char* namespace_name);
esp_err_t open();
esp_err_t close();

template<typename T> esp_err_t write(const char* key, T value);
template<typename T> esp_err_t read(const char* key, T& value);
esp_err_t commit();                        // Flush changes
esp_err_t erase_key(const char* key);
</code>

**Example:**
<code cpp>
// Initialize NVS (once at startup)
bsw::Nvram::system_init();

// Create namespace for WiFi credentials
bsw::Nvram wifi_nvs("wifi_config");
if (wifi_nvs.open() == ESP_OK) {
    // Write
    wifi_nvs.write_string("ssid", "MyNetwork");
    wifi_nvs.write_string("password", "SecurePass123");
    wifi_nvs.commit();
    
    // Read
    char ssid[32] = {0};
    wifi_nvs.read_string("ssid", ssid, 32);
    
    wifi_nvs.close();
}
</code>

**Performance:**
  * Reads: Fast (from cache)
  * Writes: Slow (~2ms per 4KB page)
  * Use batch writes: write multiple values, commit once

**Limits:**
  * Per-namespace: ~1.9 KB typical
  * Total NVS partition: 1 MB (usual)
  * Key name: max 15 characters

**Error Codes:**
  * ESP_OK - Success
  * ESP_ERR_NVS_NOT_FOUND - Key not found
  * ESP_ERR_NVS_NO_FREE_PAGES - Flash full
  * ESP_ERR_NVS_INVALID_LENGTH - Data too large

---

===== 8. WiFi - Network Connectivity =====

**Purpose:** WiFi STA (Station) and AP (Access Point) modes, provisioning.

**Operating Modes:**
  * **STA** - Connect to external network
  * **AP** - Create local hotspot
  * **Both** - Simultaneous STA + AP

**API - Connection:**
<code cpp>
void initialize();
bool connect(const char* ssid, const char* pwd, uint8_t max_attempts);
bool connect_from_nvram(uint8_t max_attempts);
void clear_wifi_credentials();
</code>

**API - Status:**
<code cpp>
bool is_connected() const;
bool is_ap_active() const;
bool has_wifi_credentials();
const std::string& get_ssid() const;
const std::string& get_password() const;
</code>

**API - Access Point:**
<code cpp>
bool start_local_access_ap();
bool get_ap_password(std::string& out_password);
bool reset_ap_password(std::string& out_password);
</code>

**API - Provisioning:**
<code cpp>
void start_provisioning_portal_blocking();   // Blocking UI
void set_pairing_pin_callback(std::function<void(const std::string&)>);
void set_operating_mode_callback(std::function<void(const std::string&)>);
void set_provisioning_html_callback(...);
</code>

**Typical Flow:**
<code cpp>
bsw::Wifi wifi;
wifi.initialize();

// Try stored credentials
if (wifi.has_wifi_credentials()) {
    if (wifi.connect_from_nvram(3)) {
        printf("Connected!\n");
        return;
    }
}

// Start provisioning portal
printf("No connection, starting provisioning...\n");
wifi.start_provisioning_portal_blocking();
printf("WiFi configured\n");
</code>

**Thread-Safety:** YES - All operations thread-safe.

---

===== 9. OTA - Over-The-Air Firmware Updates =====

**Purpose:** Download and install firmware updates remotely.

**API:**
<code cpp>
esp_err_t start_update(const char* url);
void cancel_rollback();
</code>

**Process:**
1. Download firmware from URL
2. Verify digital signature
3. Write to OTA partition
4. Flag as new boot partition
5. Reboot into new firmware
6. Rollback if fails to boot

**Example:**
<code cpp>
bsw::Ota ota;
esp_err_t err = ota.start_update("https://api.example.com/firmware-v2.bin");

if (err == ESP_OK) {
    printf("Update complete, rebooting...\n");
    // Device reboots automatically
} else {
    printf("Update failed: %s\n", esp_err_to_name(err));
}
</code>

**Security:**
  * Always use HTTPS
  * Implement firmware signing
  * Verify version before update
  * Test in staging first

---

===== 10. Time - NTP & System Clock =====

**Purpose:** Synchronize time via SNTP, manage timezone.

**API:**
<code cpp>
void init(const char* sntp_server = "pool.ntp.org");
bool isSynced() const;
</code>

**Default Timezone:** CET (Central European Time)
  * Standard: UTC+1
  * DST: UTC+2 (last Sunday March & October)

**Example:**
<code cpp>
bsw::Time time_mgr;
time_mgr.init("pool.ntp.org");

// Wait for sync (up to ~30 seconds)
int retry = 0;
while (!time_mgr.isSynced() && retry < 60) {
    vTaskDelay(pdMS_TO_TICKS(500));
    retry++;
}

if (time_mgr.isSynced()) {
    time_t now = time(NULL);
    struct tm* ti = localtime(&now);
    printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n",
           ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
           ti->tm_hour, ti->tm_min, ti->tm_sec);
}
</code>

**Popular NTP Servers:**
  * pool.ntp.org (geographically distributed, recommended)
  * time.nist.gov (US)
  * time.google.com (Google)
  * time.cloudflare.com (Cloudflare)

---

===== 11. Watchdog - System Protection =====

**Purpose:** Hardware watchdog prevents system hangs.

**API:**
<code cpp>
Watchdog(uint32_t timeout_ms);
void feed() noexcept;               // Conditional feed
void directFeed() noexcept;         // Unconditional feed
void setTimeout(uint32_t timeout);
</code>

**Timeout Recommendations:**

| Scenario | Timeout |
|----------|---------|
| Fast polling loop | 500ms - 1s |
| Sensor reading | 2-5s |
| WiFi connection attempt | 10-15s |
| OTA firmware update | 30-60s |
| File I/O operations | 5-10s |

**Example:**
<code cpp>
bsw::Watchdog watchdog(5000);  // 5-second timeout

while (1) {
    // Do application work
    do_important_work();
    
    // Reset watchdog if elapsed (conditional)
    watchdog.feed();
    
    vTaskDelay(pdMS_TO_TICKS(1000));
}
</code>

**Emergency Use:**
<code cpp>
void critical_error() {
    printf("CRITICAL ERROR - Emergency shutdown\n");
    
    // Force immediate watchdog reset
    // If not done within 5s, device will reboot
    watchdog.directFeed();
    
    // Perform critical cleanup
    cleanup_resources();
}
</code>

**Detect Watchdog Reset:**
<code cpp>
esp_reset_reason_t reason = esp_reset_reason();
if (reason == ESP_RST_WDT) {
    printf("Last reset was watchdog timeout\n");
}
</code>

---

===== System Initialization Template =====

**Recommended startup sequence:**

<code cpp>
void app_main() {
    printf("=== System Starting ===\n");
    
    // 1. Flash storage (required by WiFi)
    printf("→ NVS initialization...\n");
    bsw::Nvram::system_init();
    
    // 2. GPIO
    printf("→ GPIO initialization...\n");
    static bsw::GpioController gpio_ctl;
    gpio_ctl.init();
    
    // 3. UART (for logging)
    printf("→ UART initialization...\n");
    // uart.init();
    
    // 4. I2C/SPI (sensors/peripherals)
    printf("→ Peripheral initialization...\n");
    // i2c.init(); spi.init();
    
    // 5. WiFi
    printf("→ WiFi initialization...\n");
    static bsw::Wifi wifi;
    wifi.initialize();
    if (wifi.has_wifi_credentials()) {
        wifi.connect_from_nvram(3);
    }
    
    // 6. Time sync
    printf("→ Time synchronization...\n");
    static bsw::Time time_mgr;
    time_mgr.init("pool.ntp.org");
    
    // 7. Scheduler
    printf("→ Scheduler startup...\n");
    static bsw::Scheduler scheduler(1000, 100);
    scheduler.init_timer();
    scheduler.start_on_core(1, 100, 5);
    
    // 8. Watchdog
    printf("→ Watchdog initialization...\n");
    static bsw::Watchdog watchdog(5000);
    
    printf("=== System Ready ===\n");
    
    // Application loop
    while (1) {
        watchdog.feed();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
</code>

---

===== Error Handling Best Practices =====

**Always Check Return Values:**
<code cpp>
// ❌ Wrong - ignores error
wifi.connect("SSID", "pwd", 3);

// ✅ Correct - checks return
if (!wifi.connect("SSID", "pwd", 3)) {
    printf("WiFi connection failed\n");
    // Handle error
}
</code>

**Retry with Exponential Backoff:**
<code cpp>
bool connect_with_retry(const char* ssid, const char* pwd) {
    uint32_t delay = 100;
    for (int i = 0; i < 5; i++) {
        if (wifi.connect(ssid, pwd, 1)) return true;
        printf("Retry in %ldms\n", delay);
        vTaskDelay(pdMS_TO_TICKS(delay));
        delay = (delay < 5000) ? delay * 2 : 5000;
    }
    return false;
}
</code>

**Graceful Degradation:**
<code cpp>
void init_optional_modules() {
    // Critical
    bsw::Nvram::system_init();
    gpio_ctl.init();
    
    // Optional
    esp_err_t err = i2c.init();
    if (err != ESP_OK) {
        printf("I2C unavailable, continuing\n");
    }
}
</code>

---

===== Troubleshooting Matrix =====

| Problem | Diagnosis | Solution |
|---------|-----------|----------|
| **GPIO pin not responding** | Check mode, pin number, conflicts | Verify config, test with scope |
| **UART corrupt data** | Baud rate mismatch, noise | Use same baud, add caps, shorten cable |
| **I2C device not found** | Address wrong, missing pull-ups | Use probe(), add 4.7kΩ resistors |
| **WiFi intermittent** | Signal strength, interference | Move closer, check credentials |
| **Watchdog timeout resets** | Long-running task | Feed watchdog in long ops |
| **Out of memory** | Heap exhaustion | Reduce stacks, profile with esp_heap_trace |
| **System hangs** | Deadlock, infinite loop | Add debug output, use debugger |

---

====== Quick Navigation ======
  * GPIO & PWM: [[#3-gpio---gpio-control|Section 2-3]]
  * Communications: [[#4-uart---serial-communication|Sections 4-5]]
  * Sensors/I2C: [[#5-i2c---twi-bus-communication|Section 5]]
  * Periodic Tasks: [[#6-scheduler---periodic-task-execution|Section 6]]
  * Configuration: [[#7-nvram---persistent-flash-storage|Section 7]]
  * Network: [[#8-wifi---network-connectivity|Section 8]]
  * System Protection: [[#11-watchdog---system-protection|Section 11]]

====== Version 03a60c3 ======
Last Updated: April 22, 2026
