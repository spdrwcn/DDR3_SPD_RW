## DDR3_SPD_RW

    This is a simple DDR3 SPD register read/write module.

## Setup

```
#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_SCL_IO 19
#define I2C_SLAVE_ADDR 0x50
```

## Dependency

```
git clone https://github.com/spdrwcn/DDR3_SPD_RW.git
cd DDR3_SPD_RW
idf.py menuconfig
idf.py build
```

## Send data via serial port

### read

```
{
    "cmd": "read",
    "value": ""  // Can be empty
}

### write

```
{
    "cmd": "write",
    "value": ""  // SPD data 256 bytes
}