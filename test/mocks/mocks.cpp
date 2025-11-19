#include "mocks.h"

#ifdef NATIVE_TEST

// ==================== Mock Serial Implementation ====================
MockSerial Serial;

// ==================== Mock Preferences Static Members ====================
std::map<std::string, double> MockPreferences::doubleStore;
std::map<std::string, float> MockPreferences::floatStore;
std::map<std::string, uint8_t> MockPreferences::ucharStore;
bool MockPreferences::nvs_accessible = true;

// Note: esp_timer_get_time() must be defined in each test file
// This allows each test to control time as needed

#endif // NATIVE_TEST
