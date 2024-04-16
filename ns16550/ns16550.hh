#pragma once

// ns16550 registers

// Reciever Holding Register
constexpr int RHR = 0x0;

constexpr int DATA_READY = 0x1;
constexpr int THR_EMPTY = 0x2;

constexpr int THR = 0x0; // Transmitter Holding Register
constexpr int IER = 0x1; // Interrupt Enable Register
constexpr int ISR = 0x2; // Interuupt Status Register
constexpr int FCR = 0x2; // FIFO Control Register

constexpr int FIFO_ENABLE = 0x1;
constexpr int RX_FIFO_RESET = 0x2;
constexpr int TX_FIFO_RESET = 0x4;
constexpr int TRIGGER_8CHAR = 0b10 << 6;

constexpr int LCR = 0x3; // Line Control REgister
constexpr int MCR = 0x4; // Modem Control Register
constexpr int LSR = 0x5; // Line Status Register

constexpr int LSR_TX_EMPTY = 0x40;

constexpr int DLAB_MASK = 0x80;

constexpr int MSR = 0x6; // Modem Status Register
constexpr int SCR = 0x7; // Scratch Pad Register

// DLAB = 1 registers
constexpr int DLL = 0x0;
constexpr int DLM = 0x1;
constexpr int PSD = 0x2;