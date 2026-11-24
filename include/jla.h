#ifndef HW_MISC_JLA_H
#define HW_MISC_JLA_H

// pci identity
#define JLA_VENDOR_ID 0x1234
#define JLA_DEVICE_ID 0x4a4c

// address space for registers
#define JLA_BAR0_SIZE 0x1000

// identification values
#define JLA_ID_VALUE 0x4A4C4101      // "JLA"
#define JLA_VERSION_VALUE 0x00010000 // major version 1, minor 0

// register offsets
#define JLA_REG_ID 0x00      // read magic number
#define JLA_REG_VERSION 0x04 // read interface version
#define JLA_REG_STATUS 0x08  // read state
#define JLA_REG_CONTROL 0x0C // read/write commands

#define JLA_REG_SQ_BASE_LO 0x10
#define JLA_REG_SQ_BASE_HI 0x14
#define JLA_REG_RING_SIZE 0x18
#define JLA_REG_SQ_TAIL 0x1C // doorbell
#define JLA_REG_SQ_HEAD 0x20

#define JLA_REG_SCRATCH 0x30 // read/write tester

// status bits
#define JLA_STATUS_READY 0x01
#define JLA_STATUS_BUSY 0x10
#define JLA_STATUS_ERROR 0x100

// control bits
#define JLA_CONTROL_ENABLE 0x01
#define JLA_CONTROL_RESET 0x10

#define JLA_RING_MAX 4096

#define JLA_MAX_DURATION 1000000000ULL // 1 second

struct jla_job {
  uint32_t job_id;
  uint16_t kernel_id; // always 0 for now
  uint16_t flags;

  uint64_t duration;
  uint64_t estimate_duration;

  uint64_t args[4];

  uint8_t _padding[8];
};
static_assert(sizeof(struct jla_job) == 64, "jla_job must be 64 bytes");

#endif // HW_MISC_JLA_H
