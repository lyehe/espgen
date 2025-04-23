#ifndef RMT_DRIVER_H
#define RMT_DRIVER_H

#include <Arduino.h>

// RMT Driver placeholder
class RmtDriver {
public:
    void setupChannel(uint8_t channel, uint8_t pin);
    void transmitData(uint8_t channel /*, parameters */);
    void stopChannel(uint8_t channel);
};

#endif // RMT_DRIVER_H 