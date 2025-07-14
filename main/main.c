#include <stdio.h> 
#include <stdint.h> 
#include <string.h>
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h" 
#include "driver/uart.h"
#include "cJSON.h"
#include "driver/i2c_master.h"

#define BUF_SIZE (1024)
static uint8_t buf[BUF_SIZE];

#define I2C_BUS_PORT 0
#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_SCL_IO 19
#define I2C_SLAVE_ADDR 0x50
i2c_master_bus_handle_t bus_handle = NULL;
i2c_master_dev_handle_t dev_handle = NULL;
void uart_init() {
    
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(UART_NUM_0, &uart_config);

    uart_driver_install(
        UART_NUM_0,
        BUF_SIZE,
        BUF_SIZE,
        0,
        NULL,
        0
    );

    uart_enable_pattern_det_baud_intr(UART_NUM_0, '\n', 1, 9, 0, 0);
}

int uart_read_line(uint8_t *buf, uint32_t timeout_ms) {
    
    int len = uart_read_bytes(UART_NUM_0, buf, BUF_SIZE - 1, pdMS_TO_TICKS(timeout_ms));
    if (len > 0) {
        buf[len] = '\0'; 
    }
    return len;
}

void i2c_init() {
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_SLAVE_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
}

void i2c_read_all() {
    uint8_t write_buf[1] = {0};
    uint8_t read_buf[1] = {0};
    
    printf("    00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");

    for (int address = 0x00; address <= 0xFF; address++) {
        write_buf[0] = address;
        esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, 1, -1);
        
        if (address % 16 == 0) {
            printf("%02X: ", address);
        }

        if (ret != ESP_OK) {
            printf("-- ");
        } else {
            ret = i2c_master_receive(dev_handle, read_buf, 1, -1);
            if (ret == ESP_OK) {
                printf("%02X ", read_buf[0]);
            } else {
                printf("-- ");
            }
        }
        
        if (address % 16 == 15) {
            printf("\n");
        }
        
        vTaskDelay(1);
    }
}

void i2c_write_all(char *value) {
    if (strlen(value) != 512) {
        printf("❌ 错误: 输入数据长度必须为 512 个字符 (256 字节)\n");
        return;
    }

    uint8_t write_buf[2] = {0};
    int bytes_written = 0;

    printf("开始写入数据...\n");
    
    for (int address = 0x00; address <= 0xFF; address++) {
        
        char hex_byte[3] = {
            value[address * 2],
            value[address * 2 + 1],
            '\0'
        };
        
        uint8_t data = (uint8_t)strtol(hex_byte, NULL, 16);
        
        write_buf[0] = address;
        write_buf[1] = data;
        
        esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, 2, -1);
        if (ret != ESP_OK) {
            printf("[ERROR] 写入失败 @ 0x%02X (数据: 0x%02X)\n", address, data);
        } else {
            bytes_written++;
        }
        
        vTaskDelay(1);
    }
    
    printf("写入完成: %d/%d 字节成功\n", bytes_written, 256);
}

static void process_json(const uint8_t *buf) {
    cJSON *root = cJSON_Parse((const char *)buf);
    if (root == NULL) {
        printf("Error: Invalid JSON format.\n");
        return;
    }

    const char *required_fields[] = {"cmd", "value"};

    for (int i = 0; i < 2; i++) {
        if (!cJSON_GetObjectItemCaseSensitive(root, required_fields[i])) {
            printf("❌ 错误: 缺少 %s 字段\n", required_fields[i]);
            cJSON_Delete(root);
            return;
        }
    }

    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");

    if (strcmp(cmd->valuestring, "read") == 0) {
        i2c_read_all();
    } else if (strcmp(cmd->valuestring, "write") == 0) {
        cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
        i2c_write_all(value->valuestring);
    } else {
        printf("❌ 错误: 不支持的命令 %s\n", cmd->valuestring);
    }

    cJSON_Delete(root);
}

void app_main(void) {
    uart_init();
    i2c_init();

    while (1) {
        int len = uart_read_line(buf, 50);
        if (len > 0) {
            process_json(buf);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}