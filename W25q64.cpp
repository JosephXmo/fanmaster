#include "W25q64.h"

#include "AssetFormat.h"
#include "Config.h"
#include <new>
#include <SPIMemory.h>

W25q64::W25q64() = default;
W25q64::~W25q64() = default;

namespace {
// SPIMemory has long internal busy waits. Reject an absent/unselected device
// before entering them, especially when /CS is accidentally tied high.
void command(uint8_t opcode, uint8_t *result, size_t length)
{
    SPI.beginTransaction(SPISettings(Config::Storage::SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(Config::Pins::FLASH_CS, LOW);
    SPI.transfer(opcode);
    for (size_t i = 0; i < length; ++i) result[i] = SPI.transfer(0);
    digitalWrite(Config::Pins::FLASH_CS, HIGH);
    SPI.endTransaction();
}

bool idle()
{
    uint8_t status;
    command(0x05, &status, 1);
    return status != 0xFF && !(status & 1);
}

constexpr bool uniquePins(const uint8_t *pins, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            if (pins[i] == pins[j]) return false;
        }
    }
    return true;
}
constexpr uint8_t USED_PINS[] = {
    Config::Pins::FLASH_SCK, Config::Pins::FLASH_MOSI,
    Config::Pins::FLASH_MISO, Config::Pins::FLASH_CS,
    Config::Pins::FAN_PWM, Config::Pins::FAN_TACH, Config::Pins::RGB_LED,
    Config::Pins::I2C_SDA, Config::Pins::I2C_SCL,
    Config::Pins::BUTTON_1, Config::Pins::BUTTON_2,
    Config::Pins::BUTTON_3, Config::Pins::BUTTON_4,
};
static_assert(uniquePins(USED_PINS, sizeof(USED_PINS)), "GPIO assignments conflict");
}  // namespace

bool W25q64::begin()
{
    // Initialize the global bus explicitly BEFORE SPIMemory's begin(). ESP32
    // SPI.begin() is idempotent, so the library keeps these configured pins.
    ready_ = false;
    if (!SPI.begin(Config::Pins::FLASH_SCK, Config::Pins::FLASH_MISO,
                   Config::Pins::FLASH_MOSI, Config::Pins::FLASH_CS)) return false;
    pinMode(Config::Pins::FLASH_CS, OUTPUT);
    digitalWrite(Config::Pins::FLASH_CS, HIGH);
    command(0xAB, nullptr, 0);  // Release deep power-down before probing.
    delayMicroseconds(50);
    uint8_t id[3];
    command(0x9F, id, sizeof(id));
    if (id[0] != 0xEF || (id[1] != 0x40 && id[1] != 0x60) || id[2] != 0x17 ||
        !idle()) return false;
    flash_.reset(new (std::nothrow) SPIFlash(Config::Pins::FLASH_CS, &SPI));
    if (!flash_) return false;
    flash_->setClock(Config::Storage::SPI_CLOCK_HZ);
    ready_ = flash_->begin() && flash_->getCapacity() == AssetFormat::CAPACITY &&
             (flash_->getJEDECID() >> 16) == 0xEF;
    return ready_;
}

bool W25q64::read(uint32_t address, void *buffer, size_t length)
{
    if (!ready_ || address > AssetFormat::CAPACITY ||
        length > AssetFormat::CAPACITY - address) return false;
    if (!length) return true;
    if (!idle()) return false;
    return flash_->readByteArray(address, static_cast<uint8_t *>(buffer), length);
}

bool W25q64::eraseSector(uint32_t address)
{
    return ready_ && address % 4096 == 0 && address < AssetFormat::CAPACITY &&
           flash_->eraseSector(address);
}

bool W25q64::programPage(uint32_t address, const void *buffer, size_t length)
{
    if (!ready_ || !length || length > 256 - address % 256 ||
        address >= AssetFormat::CAPACITY || length > AssetFormat::CAPACITY - address) return false;
    return flash_->writeByteArray(address,
        const_cast<uint8_t *>(static_cast<const uint8_t *>(buffer)), length, true);
}
