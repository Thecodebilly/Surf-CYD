#pragma once
#ifndef SPI_H
#define SPI_H
// SPI is a no-op in the emulator
class SPIClass {
public:
    void begin(int, int, int, int) {}
};
extern SPIClass SPI;
#endif
