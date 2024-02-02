/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_PLTFM_TYPES_H
#define _BF_PLTFM_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#define __USE_GNU /* See feature_test_macros(7) */
#include <pthread.h>

#ifndef EQ
#define EQ(a, b) (!(((a) > (b)) - ((a) < (b))))
#endif
#ifndef GT
#define GT(a, b) ((a) > (b))
#endif
#ifndef LT
#define LT(a, b) ((a) < (b))
#endif

/* Mainline SDE version used by bsp, set 9.5.0 as default.
 * Valid value in [891,900,910,930,950,970,990,9110,9120,9130 ...].
 * A sub version start from a given mainline is valid too, such as 931,952,971, etc. */
#define SDE_VERSION 950
#define SDE_VERSION_EQ(key) \
        EQ(SDE_VERSION, (key))

#define SDE_VERSION_GT(key) \
        GT(SDE_VERSION, (key))

#define SDE_VERSION_LT(key) \
        LT(SDE_VERSION, (key))

/* Mainline OS version, <= 9 or > 9. Valid value in [8,9,10,11]. */
#define OS_VERSION 9
#define OS_VERSION_EQ(key) \
        EQ(OS_VERSION, (key))

#define OS_VERSION_GT(key) \
        GT(OS_VERSION, (key))

#define OS_VERSION_LT(key) \
        LT(OS_VERSION, (key))

#if SDE_VERSION_LT(930)
#define FP2DP(dev,phdl,dp) bf_pm_port_front_panel_port_to_dev_port_get((dev), (phdl), (dp));
#define bfn_pp_rx_set(devid,phdl,ln,policy)
#define bfn_pp_tx_set(devid,phdl,ln,policy)
#else
#define FP2DP(dev,phdl,dp) bf_pm_port_front_panel_port_to_dev_port_get((phdl), &(dev), (dp));
#define bfn_pp_rx_set(devid,phdl,ln,policy) \
    bf_pm_port_precoding_rx_set((devid),(phdl),(ln),(policy))
#define bfn_pp_tx_set(devid,phdl,ln,policy) \
    bf_pm_port_precoding_tx_set((devid),(phdl),(ln),(policy))
#endif

#define bfn_fp2_dp FP2DP

#define sub_devid 0
#if SDE_VERSION_LT(980)
#define bfn_io_set_mode_i2c(devid,sub_devid,pin) bf_io_set_mode_i2c((devid),(pin))
#define bfn_i2c_set_clk(devid,sub_devid,pin,clk) bf_i2c_set_clk((devid),(pin),(clk))
#define bfn_i2c_set_submode(devid,sub_devid,pin,mode) bf_i2c_set_submode((devid),(pin),(mode))
#define bfn_i2c_reset(devid,sub_devid,pin) bf_i2c_reset((devid),(pin))
#define bfn_i2c_get_completion_status(devid,sub_devid,pin,is_complete)\
    bf_i2c_get_completion_status((devid),(pin),(is_complete))
#define bfn_i2c_rd_reg_blocking(devid,sub_devid,pin,i2caddr,regaddr,naddrbytes,buf,ndatabytes)\
    bf_i2c_rd_reg_blocking((devid),(pin),(i2caddr),(regaddr),(naddrbytes),(buf),(ndatabytes))
#define bfn_i2c_wr_reg_blocking(devid,sub_devid,pin,i2caddr,regaddr,naddrbytes,buf,ndatabytes)\
    bf_i2c_wr_reg_blocking((devid),(pin),(i2caddr),(regaddr),(naddrbytes),(buf),(ndatabytes))
#define bfn_i2c_issue_stateout(devid,sub_devid,pin,i2caddr,regaddr,naddrbytes,buf,ndatabytes)\
    bf_i2c_issue_stateout((devid),(pin),(i2caddr),(regaddr),(naddrbytes),(buf),(ndatabytes))
#define bfn_write_stateout_buf(devid,sub_devid,pin,offset,buf,ndatabytes) \
    bf_write_stateout_buf((devid),(pin),(offset),(buf),(ndatabytes))
/* For PCIe SPI porting with different SDE.  */
#define bfn_spi_eeprom_wr(devid,sub_devid,addr,buf,buf_size) \
    bf_spi_eeprom_wr((devid),(addr),(buf),(buf_size))
#define bfn_spi_eeprom_rd(devid,sub_devid,addr,buf,buf_size) \
    bf_spi_eeprom_rd((devid),(addr),(buf),(buf_size))
#else
#define bfn_io_set_mode_i2c(devid,sub_devid,pin) bf_io_set_mode_i2c((devid),(sub_devid),(pin))
#define bfn_i2c_set_clk(devid,sub_devid,pin,clk) bf_i2c_set_clk((devid),(sub_devid),(pin),(clk))
#define bfn_i2c_set_submode(devid,sub_devid,pin,mode) bf_i2c_set_submode((devid),(sub_devid),(pin),(mode))
#define bfn_i2c_reset(devid, sub_devid, pin) bf_i2c_reset((devid),(sub_devid),(pin))
#define bfn_i2c_get_completion_status(devid,sub_devid,pin,is_complete)\
    bf_i2c_get_completion_status((devid),(sub_devid),(pin),(is_complete))
#define bfn_i2c_rd_reg_blocking(devid,sub_devid,pin,i2caddr,regaddr,naddrbytes,buf,ndatabytes)\
    bf_i2c_rd_reg_blocking((devid),(sub_devid),(pin),(i2caddr),(regaddr),(naddrbytes),(buf),(ndatabytes))
#define bfn_i2c_wr_reg_blocking(devid,sub_devid,pin,i2caddr,regaddr,naddrbytes,buf,ndatabytes)\
    bf_i2c_wr_reg_blocking((devid),(sub_devid),(pin),(i2caddr),(regaddr),(naddrbytes),(buf),(ndatabytes))
#define bfn_i2c_issue_stateout(devid,sub_devid,pin,i2caddr,regaddr,naddrbytes,buf,ndatabytes)\
    bf_i2c_issue_stateout((devid),(sub_devid),(pin),(i2caddr),(regaddr),(naddrbytes),(buf),(ndatabytes))
#define bfn_write_stateout_buf(devid,sub_devid,pin,offset,buf,ndatabytes) \
        bf_write_stateout_buf((devid),(sub_devid),(pin),(offset),(buf),(ndatabytes))
/* For PCIe SPI porting with different SDE.  */
#define bfn_spi_eeprom_wr(devid,sub_devid,addr,buf,buf_size) \
        bf_spi_eeprom_wr((devid),(sub_devid),(addr),(buf),(buf_size))
#define bfn_spi_eeprom_rd(devid,sub_devid,addr,buf,buf_size) \
        bf_spi_eeprom_rd((devid),(sub_devid),(addr),(buf),(buf_size))
#endif


#if SDE_VERSION_LT(980)
#ifdef INC_PLTFM_UCLI
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>
#include <bfutils/map/map.h>
#endif
#include <bfsys/bf_sal/bf_sys_intf.h>
#include <bfsys/bf_sal/bf_sys_timer.h>
#include <bfsys/bf_sal/bf_sys_sem.h>
#else
#ifdef INC_PLTFM_UCLI
#include <target-utils/uCli/ucli.h>
#include <target-utils/uCli/ucli_argparse.h>
#include <target-utils/uCli/ucli_handler_macros.h>
#include <target-utils/map/map.h>
#include <target-utils/third-party/cJSON/cJSON.h>
#endif
#include <target-sys/bf_sal/bf_sys_intf.h>
#include <target-sys/bf_sal/bf_sys_timer.h>
#include <target-sys/bf_sal/bf_sys_sem.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_ERROR(...) \
  bf_sys_log_and_trace(BF_MOD_PLTFM, BF_LOG_ERR, __VA_ARGS__)
#define LOG_WARNING(...) \
  bf_sys_log_and_trace(BF_MOD_PLTFM, BF_LOG_WARN, __VA_ARGS__)
#define LOG_DEBUG(...) \
  bf_sys_log_and_trace(BF_MOD_PLTFM, BF_LOG_DBG, __VA_ARGS__)

#define MAX_CHAN_PER_CONNECTOR 8

// connectors are numbered starting from 1, Hence total connectors = 1
// unused + 64 + 1 CPU
#define MAX_CONNECTORS 66

// Excluding cpu port.
//
// Note really not necessary to have this macro, since non-qsfp ports
// are derived from internal-port. Keep it for backwardation
#define BF_PLAT_MAX_QSFP 65
#define BF_PLAT_MAX_SFP  (BF_PLAT_MAX_QSFP * MAX_CHAN_PER_CONNECTOR)

typedef enum  {
    INVALID_TYPE = 0xFF,
    /*****************************************************/
    /************* Will be removed later. ****************/
    /*****************************************************/
    X564P = 1,
    X532P,
    X308P,
    X312P,
    HC,
    X732Q,

    /*****************************************************
     * Product Name powered by Asterfusion Networks.
     *****************************************************/
    AFN_X732QT = X732Q,
    AFN_X564PT = X564P,
    AFN_X532PT = X532P,
    AFN_X308PT = X308P,
    AFN_X312PT = X312P,
    AFN_HC36Y24C = HC,
} bf_pltfm_type;

#define mkver(a,b) (((a) & 0x0F) << 4 | ((b) & 0x0F))
typedef enum {
    INVALID_SUBTYPE = 0xFF,

    /*****************************************************/
    /************* Will be removed later. ****************/
    /*****************************************************/
    v1dot0 = mkver (1, 0), v1dot1 = 0x11, v1dot2 = 0x12, v1dot3 = 0x13, v1dot4 = 0x14,
    v2dot0 = 0x20, v2dot1 = 0x21, v2dot2 = 0x22, v2dot3 = 0x23, v2dot4 = 0x24,
    v3dot0 = 0x30, v3dot1 = 0x31, v3dot2 = 0x32, v3dot3 = 0x33, v3dot4 = 0x34,
    v4dot0 = 0x40,
    v5dot0 = 0x50,

    /*****************************************************
     * Subversion for BD.
     *****************************************************/
    V1P0 = mkver (1, 0),
    V1P1 = mkver (1, 1),
    V1P2 = mkver (1, 2),
    V1P3 = mkver (1, 3),
    V1P4 = mkver (1, 4),
    V2P0 = mkver (2, 0),
    V2P1 = mkver (2, 1),
    V2P2 = mkver (2, 2),
    V2P3 = mkver (2, 3),
    V2P4 = mkver (2, 4),
    V3P0 = mkver (3, 0),
    V3P1 = mkver (3, 1),
    V3P2 = mkver (3, 2),
    V3P3 = mkver (3, 3),
    V3P4 = mkver (3, 4),
    V4P0 = mkver (4, 0),
    V5P0 = mkver (5, 0),
    V5P1 = mkver (5, 1),
    V6P0 = mkver (6, 0),
    V6P1 = mkver (6, 1),
} bf_pltfm_subtype;

/*
 * Identifies the type of the board
 */
typedef enum bf_pltfm_board_id_e {
    /*****************************************************/
    /************* Will be removed later. ****************/
    /*****************************************************/
    BF_PLTFM_BD_ID_X564PT_V1DOT0 = 0x5640,
    BF_PLTFM_BD_ID_X564PT_V1DOT1 = 0x5641,
    BF_PLTFM_BD_ID_X564PT_V1DOT2 = 0x5642,
    BF_PLTFM_BD_ID_X564PT_V2DOT0 = 0x5643,
    BF_PLTFM_BD_ID_X532PT_V1DOT0 = 0x5320,
    BF_PLTFM_BD_ID_X532PT_V1DOT1 = 0x5321,
    BF_PLTFM_BD_ID_X532PT_V2DOT0 = 0x5322,
    BF_PLTFM_BD_ID_X532PT_V3DOT0 = 0x5323,
    BF_PLTFM_BD_ID_X308PT_V1DOT0 = 0x3080,
    BF_PLTFM_BD_ID_X308PT_V1DOT1 = 0x3081,
    BF_PLTFM_BD_ID_X308PT_V2DOT0 = BF_PLTFM_BD_ID_X308PT_V1DOT1, /* Announced as HW V2 to customer. */
    BF_PLTFM_BD_ID_X308PT_V3DOT0 = 0x3083,  /* Announced as HW v3.0 with PTP hwcomp. */
    BF_PLTFM_BD_ID_X312PT_V1DOT0 = 0x3120,
    BF_PLTFM_BD_ID_X312PT_V1DOT1 = BF_PLTFM_BD_ID_X312PT_V1DOT0,
    BF_PLTFM_BD_ID_X312PT_V1DOT2 = 0x3122,
    BF_PLTFM_BD_ID_X312PT_V1DOT3 = 0x3123,
    BF_PLTFM_BD_ID_X312PT_V2DOT0 = BF_PLTFM_BD_ID_X312PT_V1DOT2,
    BF_PLTFM_BD_ID_X312PT_V3DOT0 = BF_PLTFM_BD_ID_X312PT_V1DOT3,
    BF_PLTFM_BD_ID_X312PT_V4DOT0 = 0x3124,
    BF_PLTFM_BD_ID_X312PT_V5DOT0 = 0x3125,
    BF_PLTFM_BD_ID_HC36Y24C_V1DOT0 = 0x2400,
    BF_PLTFM_BD_ID_HC36Y24C_V1DOT1 = 0x2401,
    BF_PLTFM_BD_ID_X732QT_V1DOT0 = 0x7320,  /* Since Dec 2023. */


    /*****************************************************
     * Board ID enum powered by Asterfusion Networks.
     *****************************************************/
    /* AFN_X564PT-T and its subtype. */
    AFN_BD_ID_X564PT_V1P0 = 0x5640,
    AFN_BD_ID_X564PT_V1P1 = 0x5641,
    AFN_BD_ID_X564PT_V1P2 = 0x5642,
    AFN_BD_ID_X564PT_V2P0 = 0x5643,
    /* AFN_X532PT-T and its subtype. */
    AFN_BD_ID_X532PT_V1P0 = 0x5320,
    AFN_BD_ID_X532PT_V1P1 = 0x5321,
    AFN_BD_ID_X532PT_V2P0 = 0x5322,
    AFN_BD_ID_X532PT_V3P0 = 0x5323,
    /* AFN_X308PT-T and its subtype. */
    AFN_BD_ID_X308PT_V1P0 = 0x3080,
    AFN_BD_ID_X308PT_V1P1 = 0x3081,
    AFN_BD_ID_X308PT_V2P0 = AFN_BD_ID_X308PT_V1P1, /* Announced as HW V2 to customer. */
    AFN_BD_ID_X308PT_V3P0 = 0x3083,  /* Announced as HW v3.0 with PTP hwcomp. */
    /* AFN_X312PT-T and its subtype. */
    AFN_BD_ID_X312PT_V1P0 = 0x3120,
    AFN_BD_ID_X312PT_V1P1 = AFN_BD_ID_X312PT_V1P0,
    AFN_BD_ID_X312PT_V1P2 = 0x3122,
    AFN_BD_ID_X312PT_V1P3 = 0x3123,
    AFN_BD_ID_X312PT_V2P0 = AFN_BD_ID_X312PT_V1P2,
    AFN_BD_ID_X312PT_V3P0 = AFN_BD_ID_X312PT_V1P3,
    AFN_BD_ID_X312PT_V4P0 = 0x3124,
    AFN_BD_ID_X312PT_V5P0 = 0x3125,
    AFN_BD_ID_X732QT_V1P0 = 0x7320,  /* Since Dec 2023. */
    /* HC36Y24C-T and its subtype. */
    AFN_BD_ID_HC36Y24C_V1P0 = 0x2400,
    AFN_BD_ID_HC36Y24C_V1P1 = 0x2401,


    BF_PLTFM_BD_ID_UNKNOWN = 0XFFFF
} bf_pltfm_board_id_t;
/*
 * Identifies the type of the QSFP connected
 */
typedef enum bf_pltfm_qsfp_type_t {

    BF_PLTFM_QSFP_CU_0_5_M = 0,
    BF_PLTFM_QSFP_CU_1_M = 1,
    BF_PLTFM_QSFP_CU_2_M = 2,
    BF_PLTFM_QSFP_CU_3_M = 3,
    BF_PLTFM_QSFP_CU_LOOP = 4,
    BF_PLTFM_QSFP_OPT = 5,
    BF_PLTFM_QSFP_UNKNOWN = 6
} bf_pltfm_qsfp_type_t;

/*
 * Identifies the type of the QSFP-DD connected
 */
typedef enum bf_pltfm_qsfpdd_type_t {
    BF_PLTFM_QSFPDD_CU_0_5_M = 0,
    BF_PLTFM_QSFPDD_CU_1_M = 1,
    BF_PLTFM_QSFPDD_CU_1_5_M = 2,
    BF_PLTFM_QSFPDD_CU_2_M = 3,
    BF_PLTFM_QSFPDD_CU_2_5_M = 4,
    BF_PLTFM_QSFPDD_CU_LOOP = 5,
    BF_PLTFM_QSFPDD_OPT = 6,

    // Keep this last
    BF_PLTFM_QSFPDD_UNKNOWN,
} bf_pltfm_qsfpdd_type_t;

typedef struct bf_pltfm_serdes_lane_tx_eq_ {
    int32_t tx_main;
    int32_t tx_pre1;
    int32_t tx_pre2;
    int32_t tx_post1;
    int32_t tx_post2;
} bf_pltfm_serdes_lane_tx_eq_t;

typedef enum bf_pltfm_encoding_type_ {
    BF_PLTFM_ENCODING_NRZ = 0,
    BF_PLTFM_ENCODING_PAM4
} bf_pltfm_encoding_type_t;
/*
 * Encapsulates the information of a port on the board
 */
typedef struct bf_pltfm_port_info_t {
    uint32_t conn_id;
    uint32_t chnl_id;
} bf_pltfm_port_info_t;

/*
 * Identifies an error code
 */
typedef int bf_pltfm_status_t;

#define BF_PLTFM_STATUS_VALUES                                         \
  BF_PLTFM_STATUS_(BF_PLTFM_SUCCESS, "Success"),                       \
      BF_PLTFM_STATUS_(BF_PLTFM_INVALID_ARG, "Invalid Arguments"),     \
      BF_PLTFM_STATUS_(BF_PLTFM_OBJECT_NOT_FOUND, "Object Not Found"), \
      BF_PLTFM_STATUS_(BF_PLTFM_COMM_FAILED,                           \
                       "Communication Failed with Hardware"),          \
      BF_PLTFM_STATUS_(BF_PLTFM_OBJECT_ALREADY_EXISTS,                 \
                       "Object Already Exists"),                       \
      BF_PLTFM_STATUS_(BF_PLTFM_OTHER, "Other")

enum bf_pltfm_status_enum {
#define BF_PLTFM_STATUS_(x, y) x
    BF_PLTFM_STATUS_VALUES,
    BF_PLTFM_STS_MAX
#undef BF_PLTFM_STATUS_
};

static const char
*bf_pltfm_err_strings[BF_PLTFM_STS_MAX + 1] = {
#define BF_PLTFM_STATUS_(x, y) y
    BF_PLTFM_STATUS_VALUES, "Unknown error"
#undef BF_PLTFM_STATUS_
};

static inline const char *bf_pltfm_err_str (
    bf_pltfm_status_t sts)
{
    if (BF_PLTFM_STS_MAX <= sts || 0 > sts) {
        return bf_pltfm_err_strings[BF_PLTFM_STS_MAX];
    } else {
        return bf_pltfm_err_strings[sts];
    }
}

/* State defined in CPLD. */
struct st_ctx_t {
    /* Index of CPLD <NOT addr of cpld> */
    uint8_t cpld_sel;
    /* offset to current <cpld_sel>. */
    uint8_t off;
    /* bit offset of <off> */
    uint8_t off_b;   /* bit offset in off */
};

typedef struct pltfm_mgr_info_s {
    const char
    *np_name;    /* The name of health monitor thread. */
    pthread_t health_mntr_t_id;
    const char
    *np_onlp_mntr_name;
    pthread_t onlp_mntr_t_id;

#define AF_PLAT_MNTR_POWER  (1 << 0)
#define AF_PLAT_MNTR_FAN    (1 << 1)
#define AF_PLAT_MNTR_TMP    (1 << 2)
#define AF_PLAT_MNTR_MODULE (1 << 3)
#define AF_PLAT_MNTR_IDLE   (1 << 14)
#define AF_PLAT_MNTR_CTRL   (1 << 15)
#define AF_PLAT_MNTR_QSFP_REALTIME_DDM      (1 << 16)
#define AF_PLAT_MNTR_QSFP_REALTIME_DDM_LOG  (1 << 17)
#define AF_PLAT_MNTR_SFP_REALTIME_DDM       (1 << 18)
#define AF_PLAT_MNTR_SFP_REALTIME_DDM_LOG   (1 << 19)
#define AF_PLAT_MNTR_DPU1_INSTALLED         (1 << 20)
#define AF_PLAT_MNTR_DPU2_INSTALLED         (1 << 21)
#define AF_PLAT_MNTR_PTPX_INSTALLED         (1 << 22)
#define AF_PLAT_CTRL_BMC_UART       (1 << 30)   /* Access BMC through UART, otherwise through i2c. */
#define AF_PLAT_CTRL_CPLD_CP2112    (1 << 31)   /* Access CPLD through CP2112, otherwise through i2c. */
    uint32_t flags;
    uint64_t ull_mntr_ctrl_date;

    uint8_t pltfm_type;
    uint8_t pltfm_subtype;

    /* Vary data based on real hardware which identified by this.pltfm_type. */
    uint32_t psu_count;
    /* Means sensors from BMC. */
    uint32_t sensor_count;
    uint32_t fan_group_count;
    uint32_t fan_per_group;
    /* Maximum accessiable syscplds of a platform. */
    uint32_t cpld_count;
    /* Maximum available DPUs, by tsihang, 2023/11/09. */
    uint32_t dpu_count;
} pltfm_mgr_info_t;

extern pltfm_mgr_info_t *bf_pltfm_mgr_ctx();
static inline bool platform_type_equal (uint8_t type)
{
    return (bf_pltfm_mgr_ctx()->pltfm_type == type);
}
static inline bool platform_subtype_equal (uint8_t subtype)
{
    return (bf_pltfm_mgr_ctx()->pltfm_subtype == subtype);
}
static inline bf_pltfm_status_t bf_pltfm_bd_type_set_priv (
    uint8_t type, uint8_t subtype) {
    bf_pltfm_mgr_ctx()->pltfm_type = type;
    bf_pltfm_mgr_ctx()->pltfm_subtype = subtype;
    return BF_PLTFM_SUCCESS;
}
static inline bf_pltfm_status_t bf_pltfm_bd_type_get_priv (
    uint8_t *type, uint8_t *subtype) {
    *type = bf_pltfm_mgr_ctx()->pltfm_type;
    *subtype = bf_pltfm_mgr_ctx()->pltfm_subtype;
    return BF_PLTFM_SUCCESS;
}

#ifdef __cplusplus
}
#endif /* C++ */

#endif
