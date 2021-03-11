// Constants used for ATA PIO Mode
// https://wiki.osdev.org/ATA_PIO_Mode

#define SECTOR_SIZE           512

// Status register
#define IDE_STATUS_ERR        0x01  // error
#define IDE_STATUS_DF         0x20  // fault
#define IDE_STATUS_DRDY       0x40  // ready
#define IDE_STATUS_BSY        0x80  // busy

// Drive/head register
#define DRIVE_REG_BIT_5       0x20  // always set
#define DRIVE_REG_BIT_USE_LBA 0x40  // use LBA addressing
#define DRIVE_REG_BIT_7       0x80  // always set

#define USE_LBA_ADDR          DRIVE_REG_BIT_7 | DRIVE_REG_BIT_USE_LBA | DRIVE_REG_BIT_5

// Commands
#define IDE_CMD_READ          0x20
#define IDE_CMD_WRITE         0x30
#define IDE_CMD_RDMUL         0xc4
#define IDE_CMD_WRMUL         0xc5

// IO Ports
#define REG_DATA              0x1f0
#define REG_SECTOR_COUNT      0x1f2
#define REG_SECTOR_NUMBER     0x1f3
#define REG_LOW_ADDRESS       0x1f4
#define REG_HIGH_ADDRESS      0x1f5
#define REG_DRIVE_SELECT      0x1f6
#define REG_STATUS            0x1f7  // read
#define REG_CMD               0x1f7  // write

// Control ports
#define REG_DEVICE_CTRL       0x3f6
