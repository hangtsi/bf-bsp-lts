/*!
 * @file bf_pltfm_master_i2c.h
 * @date 2020/03/18
 *
 * TSIHANG (tsihang@asterfusion.com)
 */


#ifndef _BF_PLTFM_MASTER_I2C_H
#define _BF_PLTFM_MASTER_I2C_H
#include <bfutils/uCli/ucli.h>

#if 0
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
/* Debian 10, smbus IO APIs are located in /usr/include/i2c/smbus.h */
#include <i2c/smbus.h>
#endif

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

#define BF_MAV_MASTER_PCA9548_ADDR  0x70

extern bf_sys_rmutex_t master_i2c_lock;
#define MASTER_I2C_LOCK   \
    bf_sys_rmutex_lock(&master_i2c_lock)
#define MASTER_I2C_UNLOCK \
    bf_sys_rmutex_unlock(&master_i2c_lock)

int bf_pltfm_master_i2c_read_byte (
    uint8_t slave,
    uint8_t offset,
    uint8_t *value);

int bf_pltfm_master_i2c_read_block (
    uint8_t slave,
    uint8_t offset,
    uint8_t *rdbuf,
    uint8_t  rdlen);

int bf_pltfm_master_i2c_write_byte (
    uint8_t slave,
    uint8_t offset,
    uint8_t value);

int bf_pltfm_master_i2c_write_block (
    uint8_t slave,
    uint8_t offset,
    uint8_t *wrbuf,
    uint8_t  wrlen);

int bf_pltfm_bmc_write (
    uint8_t read_write_slave,
    uint8_t write_offset,
    uint8_t *write_buf,
    uint8_t write_len);

int bf_pltfm_bmc_write_read (
    uint8_t read_write_slave,
    uint8_t write_offset,
    uint8_t *write_buf,
    uint8_t write_len,
    uint8_t read_offset,
    uint8_t *read_buf,
    int usec);

int bf_pltfm_master_i2c_init();
int bf_pltfm_master_i2c_de_init();


#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_MASTER_I2C_H */

