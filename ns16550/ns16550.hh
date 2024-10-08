#pragma once

// ns16550 registers

// Reciever Holding Register
constexpr int RHR = 0x0;

constexpr int DATA_READY = 0x1;
constexpr int THR_EMPTY  = 0x2;

constexpr int THR = 0x0; // Transmitter Holding Register
constexpr int IER = 0x1; // Interrupt Enable Register

constexpr int IER_RLS      = 0x1; // Reciever Line Status
constexpr int IER_RX_DATA  = 0x2; // Reciever Data Available
constexpr int IER_TX_EMPTY = 0x4; // Transmitter Holding Register Empty
constexpr int IER_MS       = 0x8; // Modem Status

constexpr int ISR = 0x2; // Interuupt Status Register

constexpr int ISR_INT_PENDING = 0x1;
constexpr int ISR_MASK        = 0xf;

constexpr int FCR = 0x2; // FIFO Control Register

constexpr int FIFO_ENABLE   = 0x1;
constexpr int RX_FIFO_RESET = 0x2;
constexpr int TX_FIFO_RESET = 0x4;
constexpr int TRIGGER_8CHAR = 0b10 << 6;

constexpr int LCR = 0x3; // Line Control REgister
constexpr int MCR = 0x4; // Modem Control Register
constexpr int LSR = 0x5; // Line Status Register

constexpr int LSR_DATA_READY = 0x01;
constexpr int LSR_TX_EMPTY   = 0x40;

constexpr int DLAB_MASK = 0x80;

constexpr int MSR = 0x6; // Modem Status Register
constexpr int SCR = 0x7; // Scratch Pad Register

// DLAB = 1 registers
constexpr int DLL = 0x0;
constexpr int DLM = 0x1;
constexpr int PSD = 0x2;