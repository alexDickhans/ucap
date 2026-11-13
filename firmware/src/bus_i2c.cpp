#include "bus_i2c.h"

namespace {
UcapI2cBuses g_buses;
}

UcapI2cBuses &ucap_i2c() { return g_buses; }

TwoWire &UcapI2cBuses::operator[](unsigned index) {
  switch (index) {
  case 0:
    return bus0;
  case 1:
    return bus1;
  case 2:
    return bus2;
  case 3:
  default:
    return bus3;
  }
}

void UcapI2cBuses::beginAll(uint32_t hz) {
  bus0.begin();
  bus0.setClock(hz);
  bus1.begin();
  bus1.setClock(hz);
  bus2.begin();
  bus2.setClock(hz);
  bus3.begin();
  bus3.setClock(hz);
}

TwoWire &ucap_bus(unsigned index) { return g_buses[index]; }

void ucap_i2c_init() { g_buses.beginAll(400000); }
