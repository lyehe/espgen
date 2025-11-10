#include "mocks.h"

#ifdef NATIVE_TEST

// ==================== Mock Serial Implementation ====================
MockSerial Serial;

// ==================== Mock Preferences Static Members ====================
std::map<std::string, double> MockPreferences::doubleStore;
std::map<std::string, float> MockPreferences::floatStore;
std::map<std::string, uint8_t> MockPreferences::ucharStore;
bool MockPreferences::nvs_accessible = true;

// ==================== Mock ESP Timer (default implementation) ====================
// This is a weak implementation that can be overridden by tests
static uint64_t g_default_mock_time = 0;

__attribute__((weak)) uint64_t esp_timer_get_time() {
    return g_default_mock_time;
}

#endif // NATIVE_TEST
