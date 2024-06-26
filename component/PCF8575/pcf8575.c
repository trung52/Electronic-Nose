/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (c) 2024 Nguyen Nhu Hai Long <long27032002@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file pcf8575.c
 *
 * ESP-IDF driver for PCF8575 remote 16-bit I/O expander for I2C-bus
 *
 * Copyright (c) 2019 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (c) 2024 Nguyen Nhu Hai Long <long27032002@gmail.com>
 *
 * MIT Licensed as described in the file LICENSE
 */
#include <esp_err.h>
#include <esp_idf_lib_helpers.h>
#include "pcf8575.h"

#define I2C_FREQ_HZ 400000

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)
#define BV(x) (1 << (x))

static uint16_t *pcf8575_gpio_status;

static esp_err_t read_port(i2c_dev_t *dev, uint16_t *val)
{
    CHECK_ARG(dev && val);

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, i2c_dev_read(dev, NULL, 0, val, 2));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

static esp_err_t write_port(i2c_dev_t *dev, uint16_t *val)
{
    CHECK_ARG(dev);

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, i2c_dev_write(dev, NULL, 0, val, 2));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

///////////////////////////////////////////////////////////////////////////////

esp_err_t pcf8575_init_desc(i2c_dev_t *dev, uint8_t addr, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio, gpio_num_t interrupt_gpio, Interrupt_handle interrupt_handle)
{
    CHECK_ARG(dev);
    CHECK_ARG(addr & 0x20);

    dev->port = port;
    dev->addr = addr;
    dev->cfg.sda_io_num = sda_gpio;
    dev->cfg.scl_io_num = scl_gpio;
#if HELPER_TARGET_IS_ESP32
    dev->cfg.master.clk_speed = CONFIG_PCF8575_I2C_FREQ_HZ;
#endif

    pcf8575_gpio_status = (uint16_t *)malloc(sizeof(uint16_t));
    *pcf8575_gpio_status = 0x0000;

    if (interrupt_gpio != -1)
    {
        gpio_config_t config = {
            .intr_type = GPIO_INTR_ANYEDGE,
            .pin_bit_mask = BIT64(interrupt_gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE
        };

        // Configure GPIO
        ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&config));
        //install gpio isr service
        ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_install_isr_service(0));
        //hook isr handler for specific gpio pin
        ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_isr_handler_add(interrupt_gpio, interrupt_handle, NULL));
    }

    esp_err_t retrunError = i2c_dev_create_mutex(dev);

    if (retrunError != ESP_OK)
    {
        return retrunError;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(pcf8575_pin_write(dev, PCF8575_GPIO_PIN_17, 0));
    return ESP_OK;
}

esp_err_t pcf8575_free_desc(i2c_dev_t *dev)
{
    CHECK_ARG(dev);

    return i2c_dev_delete_mutex(dev);
}

esp_err_t pcf8575_port_read(i2c_dev_t *dev, uint16_t *val)
{
    return read_port(dev, val);
}

esp_err_t pcf8575_port_write(i2c_dev_t *dev, uint16_t *val)
{
    return write_port(dev, val);
}

esp_err_t pcf8575_pin_write(i2c_dev_t *dev, pcf8575_pinMap_et pin, uint8_t value)
{
    *pcf8575_gpio_status = (*pcf8575_gpio_status) & ~(1 << pin);
    *pcf8575_gpio_status = (*pcf8575_gpio_status) | (uint16_t)(value << pin);
    return write_port(dev, pcf8575_gpio_status);
}

esp_err_t pcd8575_enableInterruptGPIO(i2c_dev_t *dev, gpio_num_t interrupt_gpio)
{
    CHECK_ARG(dev);
    return gpio_intr_enable(interrupt_gpio);
}

esp_err_t pcd8575_disableInterruptGPIO(i2c_dev_t *dev, gpio_num_t interrupt_gpio)
{
    CHECK_ARG(dev);
    return gpio_intr_disable(interrupt_gpio);
}
