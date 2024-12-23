/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>

#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_bd_cfg/bf_bd_cfg.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_qsfp.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>

#define BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, prefix) \
    if ((first_port) > (last_port)) {   \
        (port)      = (last_port);      \
        (last_port) = (first_port);     \
        (first_port)= (port);           \
    }                                   \
    if ((first_port) < 1 || (last_port) > (max_port)) { \
        aim_printf (&uc->pvs, "%s must be 1-%d\n",      \
                    (prefix), (max_port));              \
        return 0;                                       \
    }

#define BF_UCLI_CH_VALID(ch, first_ch, last_ch, max_ch, prefix) \
    if ((first_ch) > (last_ch)) {   \
        (ch)      = (last_ch);      \
        (last_ch) = (first_ch);     \
        (first_ch)= (ch);           \
    }                                   \
    if ((first_ch) < 0 || (last_ch) > (max_ch)) {       \
        aim_printf (&uc->pvs, "%s must be 0-%d\n",      \
                    (prefix), ((max_ch) - 1));          \
        return 0;                                       \
    }

/* See bf_qsfp.h */
static char *bf_qsfp_ctrlmask_str[] = {
    "BF_TRANS_CTRLMASK_RX_CDR_OFF       ",//(1 << 0)
    "BF_TRANS_CTRLMASK_TX_CDR_OFF       ",//(1 << 1)
    "BF_TRANS_CTRLMASK_LASER_OFF        ",//(1 << 2)
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "BF_TRANS_CTRLMASK_OVERWRITE_DEFAULT",//(1 << 7)
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "BF_TRANS_CTRLMASK_IGNORE_RX_LOS    ",//(1 << 16)
    "BF_TRANS_CTRLMASK_IGNORE_RX_LOL    ",//(1 << 17)
    "BF_TRANS_CTRLMASK_FSM_LOG_ENA      ",//(1 << 18)
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ", //(1 << 32, never reach here)
};

extern int sff_db_get (sff_db_entry_t **entries,
                       int *count);
extern int bf_pltfm_get_qsfp_ctx (struct
                                  qsfp_ctx_t
                                  **qsfp_ctx);

extern int bf_qsfp_get_application0 (int port,
                             int ApSel,
                             qsfp_application_info_t *app_info);

static void
sff_info_show (sff_info_t *info,
               ucli_context_t *uc)
{
    aim_printf (&uc->pvs,
                "Vendor: %s Model: %s SN: %s Type: %s Module: %s Media: %s Length: %d %s\n",
                info->vendor, info->model, info->serial,
                info->sfp_type_name,
                info->module_type_name, info->media_type_name,
                info->length,
                info->length_desc);
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_pres (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "get-pres", 0,
        "get module present state");

    uint32_t lower_ports, upper_ports, cpu_ports;

    if (bf_qsfp_get_transceiver_pres (&lower_ports,
                                      &upper_ports, &cpu_ports)) {
        aim_printf (&uc->pvs,
                    "error getting qsfp presence\n");
        return 0;
    }
    aim_printf (&uc->pvs,
                "qsfp presence lower: 0x%08x upper: 0x%08x cpu: 0x%08x\n",
                lower_ports,
                upper_ports,
                cpu_ports);
    return 0;
}
static ucli_status_t bf_pltfm_ucli_ucli__qsfp_sw_get_lpmode(
    ucli_context_t *uc) {

    UCLI_COMMAND_INFO (uc,
        "sw-get-lpmode", -1,
        "[sport] [dport] ");

    int lpmode = -1;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    for (port = first_port; port <= last_port;
         port ++) {
        if (bf_qsfp_is_present (port)) {
            lpmode = bf_qsfp_get_pwr_ctrl(port);
            if (lpmode == -1) {
                aim_printf(&uc->pvs, "error getting the lpmode\n");
            } else {
                aim_printf(&uc->pvs, "qsfp port %2d lpmode %d\n", port, lpmode);
            }
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_int (ucli_context_t
                                  *uc)
{
    UCLI_COMMAND_INFO (uc,
        "get-int", 0,
        "get module interrupt");

    uint32_t lower_ports, upper_ports, cpu_ports;

    bf_qsfp_get_transceiver_int (&lower_ports,
                                 &upper_ports, &cpu_ports);
    aim_printf (&uc->pvs,
                "qsfp interrupt lower: 0x%08x upper: 0x%08x cpu: 0x%08x\n",
                lower_ports,
                upper_ports,
                cpu_ports);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_lpmode (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "get-lpmode", 0,
        "get module lpmode");

    uint32_t lower_ports, upper_ports, cpu_ports;

    if (bf_qsfp_get_transceiver_lpmode (&lower_ports,
                                        &upper_ports, &cpu_ports)) {
        aim_printf (&uc->pvs,
                    "error getting the lpmode\n");
    } else {
        aim_printf (&uc->pvs,
                    "qsfp lpmode lower: 0x%08x upper: 0x%08x cpu: 0x%08x\n",
                    lower_ports,
                    upper_ports,
                    cpu_ports);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_set_lpmode (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "set-lpmode", 2,
        "<port> <1: lpmode 0 : no lpmode>");

    int lpmode;
    int port, max_port = bf_qsfp_get_max_qsfp_ports();

    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }
    lpmode = atoi (uc->pargs->args[1]);
    /* Not supported */
    aim_printf (&uc->pvs,
                        "It's not supported to set qsfp lpmode via hardware pin\n");
    aim_printf (&uc->pvs,
                        "Please try command qsfp-lpmode-sw.\n");
    return 0;

    if (bf_qsfp_set_transceiver_lpmode (port,
                                        (lpmode ? true : false))) {
        aim_printf (&uc->pvs,
                    "error setting the lpmode\n");
    } else {
        aim_printf (&uc->pvs,
                    "qsfp port %d lpmode set to %d\n", port, lpmode);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_init (ucli_context_t
                               *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-init", 0,
        "reinit qsfps");

    bf_pltfm_qsfp_init (NULL);

    return 0;
}

#if 0
// This hangs. Disable until debugged - TBD
static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_xver_info (
    ucli_context_t *uc)
{
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    qsfp_transciever_info_t info;

    UCLI_COMMAND_INFO (uc, "get-xver-info", 1,
                       "get-xver-info <port>");
    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    bf_qsfp_get_transceiver_info (port, &info);

    return 0;
}
#endif

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_tx_disable_set (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "tx-disable-set", 3,
        "<port OR -1 for all ports> "
        "<ch OR -1 for all channels within given port> "
        "<0: laser on, 1: laser off>");

    int val;
    int max_ch = 4;//Be 4 for tof1 and 8 for tof2.
    int ch, first_ch = 0, last_ch = max_ch, allch = 1;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    if (platform_type_equal(AFN_X732QT)) {
        last_ch = max_ch = 8;
    }

    ch = atoi (uc->pargs->args[1]);
    if (ch >= 0) {
        allch = 0;
        first_ch = last_ch = ch;
    }
    BF_UCLI_CH_VALID(ch, first_ch, last_ch, max_ch, "ch");

    val = atoi (uc->pargs->args[2]);
    if ((val < 0) || (val > 1)) {
        aim_printf (&uc->pvs,
                    "val must be 0-1 for port %d\n", port);
        return 0;
    }

    for (port = first_port; port <= last_port;
         port ++) {
        if (!bf_qsfp_is_present (port))
            continue;

        max_ch = bf_qsfp_get_media_ch_cnt (port);
        /*
         * max_ch is real avaliable chnum in module. Valid value could be one of the list:
         * 1 - DR1,
         * 4 - SR4/DR4,
         * 8 - SR8
         *
         * Forcely overwrite last_ch to real chnum if all channels required.
         *
         * by Hang Tsi, 2024/04/09.
         */
        if (allch) {
            last_ch = max_ch;
        } else {
            if (first_ch > max_ch) {
                aim_printf (&uc->pvs, "port %d ch must be 0-%d. Skipping ...\n",
                        port, (max_ch - 1));
                continue;
            }
        }
        if (last_ch > max_ch) {
             last_ch = max_ch;
        }
        for (ch = first_ch; ch <= last_ch; ch ++) {
            if (val) {
                bf_qsfp_tx_disable_single_lane (port, ch, true);
            } else {
                bf_qsfp_tx_disable_single_lane (port, ch, false);
            }

            //aim_printf (&uc->pvs, "port %2d ch %d\n",
            //        port, ch);
        }
    }

    return 0;
}

/*****************************************************************
 * enable/disable both TX and RX CDRs - only use for sff-8636 modules
 *****************************************************************/
static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_cdr_disable_set (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "cdr-disable-set", 3,
        "<port OR -1 for all ports> "
        "<ch OR -1 for all channels within given port> "
        "<0: cdr on, 1: cdr off>");

    int val;
    int max_ch = 4;//Be 4 for tof1 and 8 for tof2.
    int ch, first_ch = 0, last_ch = max_ch, allch = 1;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    if (platform_type_equal(AFN_X732QT)) {
        last_ch = max_ch = 8;
    }

    ch = atoi (uc->pargs->args[1]);
    if (ch >= 0) {
        allch = 0;
        first_ch = last_ch = ch;
    }
    BF_UCLI_CH_VALID(ch, first_ch, last_ch, max_ch, "ch");

    val = atoi (uc->pargs->args[2]);
    if ((val < 0) || (val > 1)) {
        aim_printf (&uc->pvs,
                    "val must be 0-1 for port %d\n", port);
        return 0;
    }

    for (port = first_port; port <= last_port;
         port ++) {
        if (!bf_qsfp_is_present (port))
            continue;

        max_ch = bf_qsfp_get_media_ch_cnt (port);
        /*
         * max_ch is real avaliable chnum in module. Valid value could be one of the list:
         * 1 - DR1,
         * 4 - SR4/DR4,
         * 8 - SR8
         *
         * Forcely overwrite last_ch to real chnum if all channels required.
         *
         * by Hang Tsi, 2024/04/09.
         */
        if (allch) {
            last_ch = max_ch;
        } else {
            if (first_ch > max_ch) {
                aim_printf (&uc->pvs, "port %d ch must be 0-%d. Skipping ...\n",
                        port, (max_ch - 1));
                continue;
            }
        }
        if (last_ch > max_ch) {
             last_ch = max_ch;
        }
        for (ch = first_ch; ch <= last_ch; ch ++) {
            if (val) {
                bf_qsfp_cdr_disable_single_lane (port, ch, true);
            } else {
                bf_qsfp_cdr_disable_single_lane (port, ch, false);
            }

            //aim_printf (&uc->pvs, "port %2d ch %d\n",
            //        port, ch);
        }
    }

    return 0;
}

ucli_context_t *dump_uc;
void dump_qsfp_oper_array (uint8_t *arr)
{
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    for (port = 1; port <= max_port; port++) {
        if (arr[port]) {
            aim_printf (&dump_uc->pvs, "%2x|", arr[port]);
        } else {
            aim_printf (&dump_uc->pvs, "  |");
        }
    }
    aim_printf (&dump_uc->pvs, "\n");
}

static ucli_status_t qsfp_dump_pg0_info_cmis (
    ucli_context_t *uc, bool detail)
{
    uint8_t pg0_lower[128] = {0}, pg0_upper[128] = {0};
    bool present[66] = {0}, pg1_present[66] = {0}, pg16_present[66] = {0}, pg17_present[66] = {0};
    uint8_t pg1[128] = {0}, pg16[128] = {0}, pg17[128] = {0};
    uint8_t intl[66] = {0};
    uint8_t pwrctrl[66] = {0};
    uint8_t module_state[66] = {0};
    uint8_t dp_state_ln21[66] = {0}, dp_state_ln43[66] = {0}, dp_state_ln65[66] = {0}, dp_state_ln87[66] = {0};
    uint8_t dp_conf_state_ln21[66] = {0}, dp_conf_state_ln43[66] = {0}, dp_conf_state_ln65[66] = {0}, dp_conf_state_ln87[66] = {0};
    uint8_t tx_dis[66] = {0}, rx_dis[66] = {0};
    uint8_t tx_cdr_en[66] = {0};
    uint8_t rx_cdr_en[66] = {0};
    uint8_t output_state_rx[66] = {0}, output_state_tx[66] = {0}, dp_stat_chg[66] = {0};
    uint8_t dp_deinit[66] = {0};
    uint8_t tx_los[66] = {0};
    uint8_t tx_lol[66] = {0};
    uint8_t tx_flt[66] = {0};
    uint8_t tx_eq_flt[66] = {0};
    uint8_t rx_los[66] = {0};
    uint8_t rx_lol[66] = {0};
    uint8_t apply_dpinit[66] = {0};
    bool dump_summary = false;
    MemMap_Format memmap_format = MMFORMAT_UNKNOWN;

    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    dump_uc = uc;  // hack

    for (port = first_port; port <= last_port;
         port++) {
        int byte;
        /* by Hang Tsi, 2024/02/27. */
        if (!bf_qsfp_is_cmis (port)) {
            continue;
        }
        /* Dump summary if at least one port is CMIS. */
        dump_summary = true;
        qsfp_oper_info_get (port, &present[port],
                            pg0_lower, pg0_upper);

        if (!present[port]) {
            continue;
        }

        // get CMIS major and minor revison.
        memmap_format = bf_qsfp_get_memmap_format(port);

        if (detail) {
            aim_printf (&uc->pvs, "\nQSFP-DD %d:\n", port);
        }

        /* pg1 & pg16 & pg17 start with offset 128. */
        qsfp_oper_info_get_pg1 (port, &pg1_present[port], pg1);
        qsfp_oper_info_get_pg16 (port, &pg16_present[port], pg16);
        qsfp_oper_info_get_pg17 (port, &pg17_present[port], pg17);

        // dump ports QSFP-DD oper state
        // pg0 lower 3
        if (1 || pg0_lower[3] & 0xF) {
            intl[port] = (pg0_lower[3] >> 0) & 1;
            module_state[port] = (pg0_lower[3] >> 1) & 7;

            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte  3: [0:0]: %d : IntL\n",
                            intl[port]);
                if (bf_qsfp_is_flat_mem(port)) {
                    /* Flat memory modules always report ModuleReady. Forcely overwrite it. */
                    module_state[port] = 3;
                }
                aim_printf (&uc->pvs,
                            "Byte  3: [3:1]: %d : %s\n",
                            module_state[port],
                            bf_cmis_get_module_state_str(port, module_state[port]));
            }
        }

        // pg0 lower 26
        if (1 || pg0_lower[26] & 0xFF) {
            pwrctrl[port] = pg0_lower[26];

            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 26: [7:0]: %d : PwrCtrl\n",
                            pwrctrl[port]);
            }
        }

        // pg16 128, Data Path initialization control
        if (1 || pg16[128 - 128] & 0xFF) {
            dp_deinit[port] = (pg16[128 - 128] >> 0) & 0xFF;
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg16 Byte 128: [7:0]: %02x : DP8-1 Deinit CTRL\n",
                            dp_deinit[port]);
            }
        }
        // pg16 130, Tx Output Disable Function
        if (1 || pg16[130 - 128] & 0xFF) {
            tx_dis[port] = (pg16[130 - 128] >> 0) & 0xFF;
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg16 Byte 130: [7:0]: %02x : Tx8-1 Disable\n",
                            tx_dis[port]);
            }
        }
        // pg16 138, Rx Output Disable Function
        if (1 || pg16[138 - 128] & 0xFF) {
            rx_dis[port] = (pg16[138 - 128] >> 0) & 0xFF;
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg16 Byte 138: [7:0]: %02x : Rx8-1 Disable\n",
                            rx_dis[port]);
            }
        }
        // pg16 143, Apply DPInit
        if (1 || pg16[143 - 128] & 0xFF) {
            apply_dpinit[port] = (pg16[143 - 128] >> 0) & 0xFF;
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg16 Byte 143: [7:0]: %02x : DP8-1 Apply CTRL\n",
                            apply_dpinit[port]);
            }
        }
        // pg16 160/161, Rx/Tx CDR CTRL
        if (1) {
            tx_cdr_en[port] = (pg16[160 - 128] >> 0) & 0xFF;
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg16 Byte 160: [7:0]: %02x : Tx8-1 CDR CTRL\n",
                            tx_cdr_en[port]);
            }
            rx_cdr_en[port] = (pg16[161 - 128] >> 0) & 0xFF;
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg16 Byte 161: [7:0]: %02x : Rx8-1 CDR CTRL\n",
                            rx_cdr_en[port]);
            }
        }

        // pg17 128-131, Data Path States.
        if (1) {
            if (detail) {
                /* two lanes per byte. */
                dp_state_ln21[port] = pg17[128 - 128];
                dp_state_ln43[port] = pg17[129 - 128];
                dp_state_ln65[port] = pg17[130 - 128];
                dp_state_ln87[port] = pg17[131 - 128];
                aim_printf (&uc->pvs,
                            "Pg17 Byte 128: [7:0]: %01x %01x : DP2-1 STAT : %s : %s\n",
                            (dp_state_ln21[port] >> 4) & 0xF, (dp_state_ln21[port] >> 0) & 0xF,
                            bf_cmis_get_datapath_state_str (port, 1, (dp_state_ln21[port] >> 4) & 0xF),
                            bf_cmis_get_datapath_state_str (port, 0, (dp_state_ln21[port] >> 0) & 0xF));
                aim_printf (&uc->pvs,
                            "Pg17 Byte 129: [7:0]: %01x %01x : DP4-3 STAT : %s : %s\n",
                            (dp_state_ln43[port] >> 4) & 0xF, (dp_state_ln43[port] >> 0) & 0xF,
                            bf_cmis_get_datapath_state_str (port, 3, (dp_state_ln21[port] >> 4) & 0xF),
                            bf_cmis_get_datapath_state_str (port, 2, (dp_state_ln21[port] >> 0) & 0xF));
                aim_printf (&uc->pvs,
                            "Pg17 Byte 130: [7:0]: %01x %01x : DP6-5 STAT : %s : %s\n",
                            (dp_state_ln65[port] >> 4) & 0xF, (dp_state_ln65[port] >> 0) & 0xF,
                            bf_cmis_get_datapath_state_str (port, 5, (dp_state_ln21[port] >> 4) & 0xF),
                            bf_cmis_get_datapath_state_str (port, 4, (dp_state_ln21[port] >> 0) & 0xF));
                aim_printf (&uc->pvs,
                            "Pg17 Byte 131: [7:0]: %01x %01x : DP8-7 STAT : %s : %s\n",
                            (dp_state_ln87[port] >> 4) & 0xF, (dp_state_ln87[port] >> 0) & 0xF,
                            bf_cmis_get_datapath_state_str (port, 7, (dp_state_ln21[port] >> 4) & 0xF),
                            bf_cmis_get_datapath_state_str (port, 6, (dp_state_ln21[port] >> 0) & 0xF));

                dp_stat_chg[port] = pg17[134 - 128];
                aim_printf (&uc->pvs,
                            "Pg17 Byte 134: [7:0]: %01x : L-DP8-1 STAT Changed\n",
                            dp_stat_chg[port]);
            }
        }
        // pg17 132-133, Lane Output Status available since CMIS 5.0 or later.
        if (1 && (memmap_format >= MMFORMAT_CMIS5P0)) {
            output_state_rx[port] = (pg17[132 - 128] >> 0) & 0xFF;
            output_state_tx[port] = (pg17[133 - 128] >> 0) & 0xFF;
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg17 Byte 132: [7:0]: %02x : Rx8-1 Output STAT\n",
                            output_state_rx[port]);
                aim_printf (&uc->pvs,
                            "Pg17 Byte 133: [7:0]: %02x : Tx8-1 Output STAT\n",
                            output_state_tx[port]);
            }
        }
        //pg17 135-153, Lane-specific Flags
        if (1) {
            tx_flt[port] = (pg17[135 - 128] >> 0) & 0xFF;
            tx_los[port] = (pg17[136 - 128] >> 0) & 0xFF;
            tx_lol[port] = (pg17[137 - 128] >> 0) & 0xFF;
            tx_eq_flt[port] = (pg17[138 - 128] >> 0) & 0xFF;
            rx_los[port] = (pg17[147 - 128] >> 0) & 0xFF;;
            rx_lol[port] = (pg17[148 - 128] >> 0) & 0xFF;

            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg17 Byte 135: [7:0]: %02x : L-Tx8-1 Fault\n",
                            tx_flt[port]);
                aim_printf (&uc->pvs,
                            "Pg17 Byte 136: [7:0]: %02x : L-Tx8-1 LOS\n",
                            tx_los[port]);
                aim_printf (&uc->pvs,
                            "Pg17 Byte 137: [7:0]: %02x : L-Tx8-1 LOL\n",
                            tx_lol[port]);
                aim_printf (&uc->pvs,
                            "Pg17 Byte 138: [7:0]: %02x : L-Tx4-1 Adapt EQ Fault\n",
                            tx_eq_flt[port]);
                aim_printf (&uc->pvs,
                            "Pg17 Byte 147: [7:0]: %02x : L-Rx8-1 LOS\n",
                            rx_los[port]);
                aim_printf (&uc->pvs,
                            "Pg17 Byte 148: [7:0]: %02x : L-Rx8-1 LOL\n",
                            rx_lol[port]);
            }
        }

        // pg17 202, DP Config State
        if (1 || pg17[202 - 128] & 0xFF) {
            /* two lanes per byte. */
            dp_conf_state_ln21[port] = pg17[202 - 128];
            dp_conf_state_ln43[port] = pg17[203 - 128];
            dp_conf_state_ln65[port] = pg17[204 - 128];
            dp_conf_state_ln87[port] = pg17[205 - 128];
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg17 Byte 202: [7:0]: %01x %01x : DP2-1 CONF STAT : %s : %s\n",
                            (dp_conf_state_ln21[port] >> 4) & 0xF, (dp_conf_state_ln21[port] >> 0) & 0xF,
                            bf_cmis_get_datapath_config_state_str (port, 1, (dp_conf_state_ln21[port] >> 4) & 0xF),
                            bf_cmis_get_datapath_config_state_str (port, 0, (dp_conf_state_ln21[port] >> 0) & 0xF));
                aim_printf (&uc->pvs,
                            "Pg17 Byte 203: [7:0]: %01x %01x : DP4-3 CONF STAT : %s : %s\n",
                            (dp_conf_state_ln43[port] >> 4) & 0xF, (dp_conf_state_ln43[port] >> 0) & 0xF,
                            bf_cmis_get_datapath_config_state_str (port, 3, (dp_conf_state_ln43[port] >> 4) & 0xF),
                            bf_cmis_get_datapath_config_state_str (port, 2, (dp_conf_state_ln43[port] >> 0) & 0xF));
                aim_printf (&uc->pvs,
                            "Pg17 Byte 204: [7:0]: %01x %01x : DP6-5 CONF STAT : %s : %s\n",
                            (dp_conf_state_ln65[port] >> 4) & 0xF, (dp_conf_state_ln65[port] >> 0) & 0xF,
                            bf_cmis_get_datapath_config_state_str (port, 5, (dp_conf_state_ln65[port] >> 4) & 0xF),
                            bf_cmis_get_datapath_config_state_str (port, 4, (dp_conf_state_ln65[port] >> 0) & 0xF));
                aim_printf (&uc->pvs,
                            "Pg17 Byte 205: [7:0]: %01x %01x : DP8-7 CONF STAT : %s : %s\n",
                            (dp_conf_state_ln87[port] >> 4) & 0xF, (dp_conf_state_ln65[port] >> 0) & 0xF,
                            bf_cmis_get_datapath_config_state_str (port, 7, (dp_conf_state_ln87[port] >> 4) & 0xF),
                            bf_cmis_get_datapath_config_state_str (port, 6, (dp_conf_state_ln87[port] >> 0) & 0xF));
            }
        }

        if (detail) {
            aim_printf (&uc->pvs, "\nPage  0:\n");
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg0_lower[byte]);
            }
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", 128 + byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg0_upper[byte]);
            }
            aim_printf (&uc->pvs, "\n");
            aim_printf (&uc->pvs, "\nPage  1:\n");
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", 128 + byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg1[byte]);
            }
            aim_printf (&uc->pvs, "\n");
            aim_printf (&uc->pvs, "\nPage 16:\n");
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", 128 + byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg16[byte]);
            }
            aim_printf (&uc->pvs, "\n");
            aim_printf (&uc->pvs, "\nPage 17:\n");
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", 128 + byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg17[byte]);
            }
            aim_printf (&uc->pvs, "\n");
        }
    }
    if (!dump_summary) { return 0;}
    aim_printf (&uc->pvs, "\n=== QSFP-DD CMIS Summary ===\n");
    aim_printf (&uc->pvs,
                "                                  :                            1 "
                " 1  1  1  1  1  1  1  1  1  2  2  2  2  2  2  2  2  2  2  3  3  "
                "3  3  3  3  3  3  3  3  4  4  4  4  4  4  4  4  4  4  5  5  5  5 "
                " 5  5  5  5  5  5  6  6  6  6  6  6\n");
    aim_printf (&uc->pvs,
                "Byte Bit(s) Field                 : 1  2  3  4  5  6  7  8  9  0 "
                " 1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  "
                "2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  2  3 "
                " 4  5  6  7  8  9  0  1  2  3  4  5\n");
    aim_printf (&uc->pvs,
                "                                  "
                ":---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+"
                "--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--"
                "+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+-"
                "-+\n");
    aim_printf (&uc->pvs, "  3: [0:0]: IntL                  : ");
    dump_qsfp_oper_array(intl);
    aim_printf (&uc->pvs, "  3: [3:1]: Module STAT           : ");
    dump_qsfp_oper_array(module_state);
    aim_printf (&uc->pvs, " 26: [7:0]: Power CTRL            : ");
    dump_qsfp_oper_array (pwrctrl);

    //pg17
    //  128-131 DP STAT, skip
    //  132 & 133, Lane Output Status available since CMIS 5.0 or later.
    aim_printf (&uc->pvs, "128: [7:0]: DP2-1 STAT            : ");
    dump_qsfp_oper_array (dp_state_ln21);
    aim_printf (&uc->pvs, "129: [7:0]: DP4-3 STAT            : ");
    dump_qsfp_oper_array (dp_state_ln43);
    aim_printf (&uc->pvs, "130: [7:0]: DP6-5 STAT            : ");
    dump_qsfp_oper_array (dp_state_ln65);
    aim_printf (&uc->pvs, "131: [7:0]: DP8-7 STAT            : ");
    dump_qsfp_oper_array (dp_state_ln87);

    aim_printf (&uc->pvs, "132: [7:0]: Rx8-1 Output STAT     : ");
    dump_qsfp_oper_array (output_state_rx);
    aim_printf (&uc->pvs, "133: [7:0]: Tx8-1 Output STAT     : ");
    dump_qsfp_oper_array (output_state_tx);
    aim_printf (&uc->pvs, "134: [7:0]: L-DP8-1 STAT Changed  : ");
    dump_qsfp_oper_array (dp_stat_chg);
    aim_printf (&uc->pvs, "135: [7:0]: L-Tx8-1 Fault         : ");
    dump_qsfp_oper_array (tx_flt);
    aim_printf (&uc->pvs, "136: [7:0]: L-Tx8-1 LOS           : ");
    dump_qsfp_oper_array (tx_los);
    aim_printf (&uc->pvs, "137: [7:0]: L-Tx8-1 LOL           : ");
    dump_qsfp_oper_array (tx_lol);
    aim_printf (&uc->pvs, "138: [7:0]: L-Tx8-1 Adapt EQ Fault: ");
    dump_qsfp_oper_array (tx_eq_flt);
    aim_printf (&uc->pvs, "147: [7:0]: L-Rx8-1 LOS           : ");
    dump_qsfp_oper_array (rx_los);
    aim_printf (&uc->pvs, "148: [7:0]: L-Rx8-1 LOL           : ");
    dump_qsfp_oper_array (rx_lol);

    //pg16
    aim_printf (&uc->pvs, "128: [7:0]: DP8-1 Deinit CTRL     : ");
    dump_qsfp_oper_array (dp_deinit);
    aim_printf (&uc->pvs, "130: [7:0]: Tx8-1 Disable         : ");
    dump_qsfp_oper_array (tx_dis);
    aim_printf (&uc->pvs, "138: [7:0]: Rx8-1 Disable         : ");
    dump_qsfp_oper_array (rx_dis);
    aim_printf (&uc->pvs, "143: [7:0]: DP8-1 Apply CTRL      : ");
    dump_qsfp_oper_array (apply_dpinit);
    aim_printf (&uc->pvs, "160: [7:0]: Tx8-1 CDR CTRL        : ");
    dump_qsfp_oper_array (tx_cdr_en);
    aim_printf (&uc->pvs, "161: [7:0]: Rx8-1 CDR CTRL        : ");
    dump_qsfp_oper_array (rx_cdr_en);

    /* More Control/Status here. */

    return 0;
}

static ucli_status_t qsfp_dump_pg0_info (
    ucli_context_t *uc, bool detail)
{
    uint8_t pg0_lower[128], pg0_upper[128];
    bool present[66], pg3_present;
    uint8_t pg3[128];
    uint8_t intl[66] = {0};
    uint8_t dnr[66] = {0};
    uint8_t tx_los[66] = {0};
    uint8_t rx_los[66] = {0};
    uint8_t tx_eq_flt[66] = {0};
    uint8_t rx_eq_flt[66] = {0};
    uint8_t tx_lol[66] = {0};
    uint8_t rx_lol[66] = {0};
    uint8_t tx_dis[66] = {0};
    uint8_t pwr[66] = {0};
    uint8_t tx_cdr_en[66] = {0};
    uint8_t rx_cdr_en[66] = {0};
    uint8_t out_emph_lo[66] = {0};
    uint8_t out_emph_hi[66] = {0};
    uint8_t in_eq_lo[66] = {0};
    uint8_t in_eq_hi[66] = {0};
    uint8_t out_amp_lo[66] = {0};
    uint8_t out_amp_hi[66] = {0};
    bool dump_summary = false;

    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    dump_uc = uc;  // hack

    for (port = first_port; port <= last_port;
         port++) {
        int byte;
        /* by Hang Tsi, 2024/02/27. */
        if (!bf_qsfp_is_sff8636 (port)) {
            continue;
        }
        /* Dump summary if at least one port is SFF-8636. */
        dump_summary = true;
        qsfp_oper_info_get (port, &present[port],
                            pg0_lower, pg0_upper);

        if (!present[port]) {
            continue;
        }

        qsfp_oper_info_get_pg3 (port, &pg3_present, pg3);

        if (detail) {
            aim_printf (&uc->pvs, "\nQSFP %d:\n", port);
        }
        // dump ports QSFP oper state
        if (1 || pg0_lower[2] & 0x3) {
            if (detail) {
                aim_printf (
                    &uc->pvs, "Byte 2: [1:1]: %d : IntL\n",
                    (pg0_lower[2] >> 1) & 1);
                aim_printf (&uc->pvs,
                            "Byte 2: [0:0]: %d : Data_Not_Ready\n",
                            (pg0_lower[2] >> 0) & 1);
            }
            intl[port] = (pg0_lower[2] >> 1) & 1;
            dnr[port] = (pg0_lower[2] >> 0) & 1;
        }
        if (1 || pg0_lower[3] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 3: [7:4]: %01x : L-Tx4-1 LOS\n",
                            (pg0_lower[3] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Byte 3: [3:0]: %01x : L-Rx4-1 LOS\n",
                            (pg0_lower[3] >> 0) & 0xF);
            }
            tx_los[port] = (pg0_lower[3] >> 4) & 0xF;
            rx_los[port] = (pg0_lower[3] >> 0) & 0xF;
        }
        if (1 || pg0_lower[4] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 4: [7:4]: %01x : L-Tx4-1 Adapt EQ Fault\n",
                            (pg0_lower[4] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Byte 4: [3:0]: %01x : L-Rx4-1 Adapt EQ Fault\n",
                            (pg0_lower[4] >> 0) & 0xF);
            }
            tx_eq_flt[port] = (pg0_lower[4] >> 4) & 0xF;
            rx_eq_flt[port] = (pg0_lower[4] >> 0) & 0xF;
        }
        if (1 || pg0_lower[5] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 5: [7:4]: %01x : L-Tx4-1 LOL\n",
                            (pg0_lower[5] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Byte 5: [3:0]: %01x : L-Rx4-1 LOL\n",
                            (pg0_lower[5] >> 0) & 0xF);
            }
            tx_lol[port] = (pg0_lower[5] >> 4) & 0xF;
            rx_lol[port] = (pg0_lower[5] >> 0) & 0xF;
        }
        if (1 || pg0_lower[86] & 0xF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 86: [3:0]: %01x : Tx4-1 Disable\n",
                            (pg0_lower[86] >> 0) & 0xF);
            }
            tx_dis[port] = (pg0_lower[86] >> 0) & 0xF;
        }
        if (1 || pg0_lower[93] & 0xFF) {
            if (detail) {
                aim_printf (
                    &uc->pvs,
                    "Byte 93: [2:2]: %01x : High Power Class Enable (Classes 5-7)\n",
                    (pg0_lower[93] >> 2) & 0x1);
                aim_printf (&uc->pvs,
                            "Byte 93: [1:1]: %01x : Power set\n",
                            (pg0_lower[93] >> 1) & 0x1);
                aim_printf (&uc->pvs,
                            "Byte 93: [0:0]: %01x : Power override\n",
                            (pg0_lower[93] >> 0) & 0x1);
            }
            pwr[port] = pg0_lower[93] & 0x7;
        }
        if (1 || pg0_lower[98] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 98: [7:4]: %01x : Tx4-1_CDR_control\n",
                            (pg0_lower[98] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Byte 98: [3:0]: %01x : Rx4-1_CDR_control\n",
                            (pg0_lower[98] >> 0) & 0xF);
            }
            tx_cdr_en[port] = (pg0_lower[98] >> 4) & 0xF;
            rx_cdr_en[port] = (pg0_lower[98] >> 0) & 0xF;
        }

        // Page 03 info
        if (1 || pg3[234 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 234: [7:4]: %01x : Input Eq (dB)\n",
                            (pg3[234 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 234: [3:0]: %01x : Input Eq (dB)\n",
                            (pg3[234 - 128] >> 0) & 0xF);
            }
            in_eq_lo[port] = (pg3[234 - 128] >> 4) & 0xF;
            in_eq_lo[port] = (pg3[234 - 128] >> 0) & 0xF;
        }
        if (1 || pg3[235 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 235: [7:4]: %01x : Input Eq (dB)\n",
                            (pg3[235 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 235: [3:0]: %01x : Input Eq (dB)\n",
                            (pg3[235 - 128] >> 0) & 0xF);
            }
            in_eq_hi[port] = (pg3[235 - 128] >> 4) & 0xF;
            in_eq_hi[port] = (pg3[235 - 128] >> 0) & 0xF;
        }

        if (1 || pg3[236 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 236: [7:4]: %01x : Output Emphasis (dB)\n",
                            (pg3[236 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 236: [3:0]: %01x : Output Emphasis (dB)\n",
                            (pg3[236 - 128] >> 0) & 0xF);
            }
            out_emph_lo[port] = (pg3[236 - 128] >> 4) & 0xF;
            out_emph_lo[port] = (pg3[236 - 128] >> 0) & 0xF;
        }
        if (1 || pg3[237 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 237: [7:4]: %01x : Output Emphasis (dB)\n",
                            (pg3[237 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 237: [3:0]: %01x : Output Emphasis (dB)\n",
                            (pg3[237 - 128] >> 0) & 0xF);
            }
            out_emph_hi[port] = (pg3[237 - 128] >> 4) & 0xF;
            out_emph_hi[port] = (pg3[237 - 128] >> 0) & 0xF;
        }

        if (1 || pg3[238 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 238: [7:4]: %01x : Output Amplitude (dB)\n",
                            (pg3[238 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 238: [3:0]: %01x : Output Amplitude (dB)\n",
                            (pg3[238 - 128] >> 0) & 0xF);
            }
            out_amp_lo[port] = (pg3[238 - 128] >> 4) & 0xF;
            out_amp_lo[port] = (pg3[238 - 128] >> 0) & 0xF;
        }
        if (1 || pg3[239 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 239: [7:4]: %01x : Output Emphasis (dB)\n",
                            (pg3[239 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 239: [3:0]: %01x : Output Emphasis (dB)\n",
                            (pg3[239 - 128] >> 0) & 0xF);
            }
            out_amp_hi[port] = (pg3[239 - 128] >> 4) & 0xF;
            out_amp_hi[port] = (pg3[239 - 128] >> 0) & 0xF;
        }

        if (detail) {
            aim_printf (&uc->pvs, "\nPage 0:\n");
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg0_lower[byte]);
            }
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", 128 + byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg0_upper[byte]);
            }
            aim_printf (&uc->pvs, "\n");
            aim_printf (&uc->pvs, "\nPage 3:\n");
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", 128 + byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg3[byte]);
            }
            aim_printf (&uc->pvs, "\n");
        }
    }
    if (!dump_summary) { return 0;}
    aim_printf (&uc->pvs, "\n=== QSFP SFF-8636 Summary ===\n");
    aim_printf (&uc->pvs,
                "                                  :                            1 "
                " 1  1  1  1  1  1  1  1  1  2  2  2  2  2  2  2  2  2  2  3  3  "
                "3  3  3  3  3  3  3  3  4  4  4  4  4  4  4  4  4  4  5  5  5  5 "
                " 5  5  5  5  5  5  6  6  6  6  6  6\n");
    aim_printf (&uc->pvs,
                "Byte Bit(s) Field                 : 1  2  3  4  5  6  7  8  9  0 "
                " 1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  "
                "2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  2  3 "
                " 4  5  6  7  8  9  0  1  2  3  4  5\n");
    aim_printf (&uc->pvs,
                "                                  "
                ":---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+"
                "--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--"
                "+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+-"
                "-+\n");
    aim_printf (&uc->pvs,
                "  2: [1:1]: IntL                  : ");
    dump_qsfp_oper_array (intl);
    aim_printf (&uc->pvs,
                "  2: [0:0]: Data_Not_Ready        : ");
    dump_qsfp_oper_array (dnr);
    aim_printf (&uc->pvs,
                "  3: [7:4]: L-Tx4-1 LOS           : ");
    dump_qsfp_oper_array (tx_los);
    aim_printf (&uc->pvs,
                "  3: [3:0]: L-Rx4-1 LOS           : ");
    dump_qsfp_oper_array (rx_los);
    aim_printf (&uc->pvs,
                "  4: [7:4]: L-Tx4-1 Adapt EQ Fault: ");
    dump_qsfp_oper_array (tx_eq_flt);
    aim_printf (&uc->pvs,
                "  4: [3:0]: L-Rx4-1 Adapt EQ Fault: ");
    dump_qsfp_oper_array (rx_eq_flt);
    aim_printf (&uc->pvs,
                "  5: [7:4]: L-Tx4-1 LOL           : ");
    dump_qsfp_oper_array (tx_lol);
    aim_printf (&uc->pvs,
                "  5: [3:0]: L-Rx4-1 LOL           : ");
    dump_qsfp_oper_array (rx_lol);
    aim_printf (&uc->pvs,
                " 86: [3:0]: Tx4-1 Disable         : ");
    dump_qsfp_oper_array (tx_dis);
    aim_printf (&uc->pvs,
                " 93: [1:1]: Power                 : ");
    dump_qsfp_oper_array (pwr);
    aim_printf (&uc->pvs,
                " 98: [7:4]: Tx4-1_CDR_control     : ");
    dump_qsfp_oper_array (tx_cdr_en);
    aim_printf (&uc->pvs,
                " 98: [3:0]: Rx4-1_CDR_control     : ");
    dump_qsfp_oper_array (rx_cdr_en);
    aim_printf (&uc->pvs,
                "234: [7:0]: Input Eq    (dB)      : ");
    dump_qsfp_oper_array (in_eq_lo);
    aim_printf (&uc->pvs,
                "235: [7:0]: Input Eq    (dB)      : ");
    dump_qsfp_oper_array (in_eq_hi);
    aim_printf (&uc->pvs,
                "236: [7:0]: Output Emph (dB)      : ");
    dump_qsfp_oper_array (out_emph_lo);
    aim_printf (&uc->pvs,
                "237: [7:0]: Output Emph (dB)      : ");
    dump_qsfp_oper_array (out_emph_hi);
    aim_printf (&uc->pvs,
                "238: [7:0]: Output Amp  (dB)      : ");
    dump_qsfp_oper_array (out_amp_lo);
    aim_printf (&uc->pvs,
                "239: [7:0]: Output Amp  (dB)      : ");
    dump_qsfp_oper_array (out_amp_hi);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_pg0 (ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "pg0", -1, "pg0 [sport] [dport]");
    qsfp_dump_pg0_info (uc, true /*detail*/);
    qsfp_dump_pg0_info_cmis (uc, true);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_oper (ucli_context_t
                               *uc)
{
    UCLI_COMMAND_INFO (uc, "oper", -1, "oper [sport] [dport]");
    qsfp_dump_pg0_info (uc, false /*summary*/);
    qsfp_dump_pg0_info_cmis (uc, false);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_pg3 (ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "pg3", -1, "pg3 [sport] [dport]");

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_dump_idprom_cache (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "dump-idprom-cache", 2,
        "<port> <page>");

    uint8_t buf[MAX_QSFP_PAGE_SIZE];
    int rc, port, pg;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    pg = atoi (uc->pargs->args[1]);

    rc = bf_qsfp_get_cached_info (port, pg, buf);
    if (rc < 0) {
        aim_printf (&uc->pvs,
                    "bf_qsfp_get_cached_info returned error\n");
        return 0;
    }

    for (int i = 0; i < MAX_QSFP_PAGE_SIZE; i++) {
        if ((i % 16) == 0) {
            aim_printf (&uc->pvs, "\n0x%02x: ", i);
        }
        aim_printf (&uc->pvs, "0x%02x ", buf[i]);
    }

    aim_printf (&uc->pvs, "\n");
    return 0;
}

uint8_t *sfp_string (uint8_t *buf, size_t offset,
                     size_t len)
{
    uint8_t *start = buf + offset;
    while (len > 0 && start[len - 1] == ' ') {
        --len;
    }
    * (start + len - 1) = 0; /* NULL terminate it */

    return start;
}

/* copy  len bytes from src+offset to dst and adds a NULL to dst
 * dst must have len+1 space in it
 */
static void sfp_copy_string (uint8_t *dst,
                             uint8_t *src,
                             size_t offset,
                             size_t len)
{
    memcpy (dst, src + offset, len);
    dst[len] = '\0';
}

void printChannelMonitor (ucli_context_t *uc,
                          unsigned int index,
                          const uint8_t *buf,
                          unsigned int rxMSB,
                          unsigned int rxLSB,
                          unsigned int txMSB,
                          unsigned int txLSB,
                          unsigned int txpwrMSB,
                          unsigned int txpwrLSB)
{
    /* Notes about DDM register values:
        - MSB in lower byte
        - For Tx/Rx power, each unit is 1uW
        - Converting to dBm is 10*log(mW)
     */
    uint16_t rxValue = (buf[rxMSB] << 8) | buf[rxLSB];
    uint16_t txValue = (buf[txMSB] << 8) | buf[txLSB];
    uint16_t txpwrValue = (buf[txpwrMSB] << 8) |
                          buf[txpwrLSB];

    /* RX power ranges from 0mW to 6.5535mW*/
    double rxPower = 10 * log10 (0.0001 * rxValue);

    /* TX power ranges from 0mW to 6.5535mW */
    double txPower = 10 * log10 (0.0001 * txpwrValue);

    /* TX bias ranges from 0mA to 131mA */
    double txBias = (131.0 * txValue) / 65535;

    aim_printf (&uc->pvs,
                "    Channel %d:   %9.2fdBm  %9.2fdBm  %10.2fmA\n",
                index,
                rxPower,
                txPower,
                txBias);
}

void printDdm_for_cmis (ucli_context_t *uc,
                        unsigned int port)
{
    double temp;
    double volt;
    double scale;
    double txBias;
    double txPower;
    double rxPower;
    uint16_t txBiasValue;
    uint16_t txValue;
    uint16_t rxValue;
    uint8_t scaleValue;
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2] = {0};
    uint8_t vcc[2], tem[2];

    bf_qsfp_field_read_onebank (port, TEMPERATURE, 0,
                                0, 2, &tem[0]);
    bf_qsfp_field_read_onebank (port, VCC, 0, 0, 2,
                                &vcc[0]);

    // TBD - use onebank to get flags
    bf_qsfp_module_read (
        port, QSFP_BANKNA, QSFP_PAGE1, 160, 1,
        &scaleValue);
    bf_qsfp_module_read (
        port, QSFP_BANK0, QSFP_PAGE17, 128, 128,
        buf + MAX_QSFP_PAGE_SIZE);

    temp = (tem[0]) + (tem[1] / 256.0);
    volt = ((vcc[0] << 8) | vcc[1]) / 10000.0;
    scale = pow(2.0, (scaleValue >> 3) & 3);

    uint8_t max_ch = bf_qsfp_get_media_ch_cnt (
                               port);

    for (uint8_t i = 0; i < max_ch; i++) {
        txBiasValue = (buf[170 + i * 2] << 8) | buf[171 +
                      i * 2];
        txValue = (buf[154 + i * 2] << 8) | buf[155 + i *
                                                2];
        rxValue = (buf[186 + i * 2] << 8) | buf[187 + i *
                                                2];

        txBias = scale * (131.0 * txBiasValue) / 65535;
        txPower = 10 * log10 (0.0001 * txValue);
        rxPower = 10 * log10 (0.0001 * rxValue);

        aim_printf (&uc->pvs,
                    "      %d/%d %10.1f %15.2f %10.2f %16.2f %16.2f\n",
                    port,
                    i,
                    temp,
                    volt,
                    txBias,
                    txPower,
                    rxPower);
    }
}

void printVdm_for_cmis (ucli_context_t *uc,
                        unsigned int port)
{
    cmis_vdm_info_t vdm_info;
    uint8_t vdm_groups_pages[MAX_QSFP_PAGE_SIZE * 12] = {0};
    uint8_t vdm_flags_page[MAX_QSFP_PAGE_SIZE] = {0};
    uint8_t vdm_masks_page[MAX_QSFP_PAGE_SIZE] = {0};

    // non-zero retval means vdm unsupported
    if (0 != bf_cmis_get_vdm_pages (port,
                                    vdm_groups_pages,
                                    vdm_flags_page,
                                    vdm_masks_page)) {
        return;
    }

    aim_printf (
        &uc->pvs, "CMIS VDM Info for module on port %d\n", port);
    aim_printf (&uc->pvs,
            "%10s %15s %17s %16s %16s %15s %16s %15s %15s %14s\n",
            "-------",
            "--------------",
            "----------------",
            "---------------",
            "---------------",
            "--------------",
            "---------------",
            "--------------",
            "--------------",
            "-------------");
    aim_printf (&uc->pvs,
            "%10s %15s %17s %16s %16s %15s %16s %15s %15s %14s\n",
            "Port/Ch",
            "Value",
            "High Alarm Thres",
            "Low Alarm Thres",
            "High Warn Thres",
            "Low Warn Thres",
            "High Alarm Trig",
            "Low Alarm Trig",
            "High Warn Trig",
            "Low Warn Trig");

    /*
    By SunZheng, 2024-11-06.
      Refer to CMIS 5.1 Table 8-153 for detailed support adv.
      Odd Adress:
      VDM observable type ID, real-time monitored value in Page + 4
      Even Address:
      Bit 7-4: Threshold set ID in Page + 8, in group of 8 bytes, 16 sets/page
      Bit 3-0: n. Monitored lane n+1
    */
    uint8_t type_id;
    uint8_t last_type_id = 0;
    uint8_t lane, thres_id;
    uint8_t msb_val, lsb_val;
    uint8_t msb_ha, lsb_ha;
    uint8_t msb_la, lsb_la;
    uint8_t msb_hw, lsb_hw;
    uint8_t msb_lw, lsb_lw;
    double val;
    double ha_val, la_val;
    double hw_val, lw_val;
    bool ha_trig, la_trig;
    bool hw_trig, lw_trig;

    for (int i = 0; i < 4; i++) {
        for (int j = 0, k = 0; j <= MAX_QSFP_PAGE_SIZE; j += 2, k++) {
            type_id = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * i + j + 1];
            if (0 == bf_cmis_get_vdm_observable_types(port,
                                                      type_id,
                                                      &vdm_info)) {
                // Print each observable_type only once by checking last_type_id
                if (last_type_id != type_id) {
                    aim_printf (&uc->pvs,
                            "%10s %15s %17s %16s %16s %15s %16s %15s %15s %14s\n",
                            "-------",
                            "--------------",
                            "----------------",
                            "---------------",
                            "---------------",
                            "--------------",
                            "---------------",
                            "--------------",
                            "--------------",
                            "-------------");
                    aim_printf (&uc->pvs,
                            "   %s %s\n",
                            vdm_info.observable_type,
                            vdm_info.unit);
                }

                // Find the corresponding address and fetch the value
                lane = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * i + j] & 0xf;
                thres_id = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * i + j] >> 4;
                msb_val = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 4) + j];
                lsb_val = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 4) + j + 1];
                msb_ha = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 8) + thres_id * 8];
                lsb_ha = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 8) + thres_id * 8 + 1];
                msb_la = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 8) + thres_id * 8 + 2];
                lsb_la = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 8) + thres_id * 8 + 3];
                msb_hw = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 8) + thres_id * 8 + 4];
                lsb_hw = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 8) + thres_id * 8 + 5];
                msb_lw = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 8) + thres_id * 8 + 6];
                lsb_lw = vdm_groups_pages[MAX_QSFP_PAGE_SIZE * (i + 8) + thres_id * 8 + 7];

                // Print VDM values, thresholds, warning/alarm flags
                if (strcmp(vdm_info.data_type, "S16") == 0) {
                    val = (int16_t)((msb_val << 8) | lsb_val) * vdm_info.unit_scale;
                    ha_val = (int16_t)((msb_ha << 8) | lsb_ha) * vdm_info.unit_scale;
                    la_val = (int16_t)((msb_la << 8) | lsb_la) * vdm_info.unit_scale;
                    hw_val = (int16_t)((msb_hw << 8) | lsb_hw) * vdm_info.unit_scale;
                    lw_val = (int16_t)((msb_lw << 8) | lsb_lw) * vdm_info.unit_scale;
                } else if (strcmp(vdm_info.data_type, "U16") == 0) {
                    val = (uint16_t)((msb_val << 8) | lsb_val) * vdm_info.unit_scale;
                    ha_val = (uint16_t)((msb_ha << 8) | lsb_ha) * vdm_info.unit_scale;
                    la_val = (uint16_t)((msb_la << 8) | lsb_la) * vdm_info.unit_scale;
                    hw_val = (uint16_t)((msb_hw << 8) | lsb_hw) * vdm_info.unit_scale;
                    lw_val = (uint16_t)((msb_lw << 8) | lsb_lw) * vdm_info.unit_scale;
                } else if (strcmp(vdm_info.data_type, "F16") == 0) {
                    uint16_t val_u16 = (uint16_t)((msb_val << 8) | lsb_val);
                    uint16_t ha_val_u16 = (uint16_t)((msb_ha << 8) | lsb_ha);
                    uint16_t la_val_u16 = (uint16_t)((msb_la << 8) | lsb_la);
                    uint16_t hw_val_u16 = (uint16_t)((msb_hw << 8) | lsb_hw);
                    uint16_t lw_val_u16 = (uint16_t)((msb_lw << 8) | lsb_lw);

                    val = (val_u16 & 0x7ff) * pow(10, ((val_u16 >> 11) & 0x1f) - 24);
                    ha_val = (ha_val_u16 & 0x7ff) * pow(10, ((ha_val_u16 >> 11) & 0x1f) - 24);
                    la_val = (la_val_u16 & 0x7ff) * pow(10, ((la_val_u16 >> 11) & 0x1f) - 24);
                    hw_val = (hw_val_u16 & 0x7ff) * pow(10, ((hw_val_u16 >> 11) & 0x1f) - 24);
                    lw_val = (lw_val_u16 & 0x7ff) * pow(10, ((lw_val_u16 >> 11) & 0x1f) - 24);
                } else {
                    continue;
                }
                ha_trig = (vdm_flags_page[i * 32 + k] >> ((k % 2) * 4)) & 0x1;
                la_trig = (vdm_flags_page[i * 32 + k] >> ((k % 2) * 4 + 1)) & 0x1;
                hw_trig = (vdm_flags_page[i * 32 + k] >> ((k % 2) * 4 + 2)) & 0x1;
                lw_trig = (vdm_flags_page[i * 32 + k] >> ((k % 2) * 4 + 3)) & 0x1;

                aim_printf (&uc->pvs,
                        "      %2d/%d %15.2f %17.2f %16.2f %16.2f %15.2f %16s %15s %15s %14s\n",
                        port, lane, val, ha_val, la_val, hw_val, lw_val,
                        ha_trig ? "true" : "false", la_trig ? "true" : "false",
                        hw_trig ? "true" : "false", lw_trig ? "true" : "false");

                // Update the latest parsed type id
                last_type_id = type_id;
            }
        }
    }
    aim_printf (&uc->pvs, "\n");
}

void printThreshold (ucli_context_t *uc,
                     unsigned int port)
{
    qsfp_alarm_threshold_t thresh;

    if (bf_qsfp_get_threshold_info (port, &thresh)) {
        aim_printf (&uc->pvs,
                    "\n## ModuleThresholdValues\n");

        aim_printf (&uc->pvs, "\n");
        aim_printf (
            &uc->pvs, "%-20s = %fC\n", "TempHighAlarm",
            thresh.temp.alarm.high);
        aim_printf (
            &uc->pvs, "%-20s = %fC\n", "TempLowAlarm",
            thresh.temp.alarm.low);
        aim_printf (
            &uc->pvs, "%-20s = %fC\n", "TempHighWarning",
            thresh.temp.warn.high);
        aim_printf (
            &uc->pvs, "%-20s = %fC\n", "TempLowWarning",
            thresh.temp.warn.low);

        aim_printf (&uc->pvs, "\n");
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fVolts\n",
                    "VccHighAlarm",
                    thresh.vcc.alarm.high);
        aim_printf (
            &uc->pvs, "%-20s = %0.2fVolts\n", "VccLowAlarm",
            thresh.vcc.alarm.low);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fVolts\n",
                    "VccHighWarning",
                    thresh.vcc.warn.high);
        aim_printf (
            &uc->pvs, "%-20s = %0.2fVolts\n", "VccLowWarning",
            thresh.vcc.warn.low);

        aim_printf (&uc->pvs, "\n");
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fdbm\n",
                    "RxPowerHighAlarm",
                    thresh.rx_pwr.alarm.high);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fdbm\n",
                    "RxPowerLowAlarm",
                    thresh.rx_pwr.alarm.low);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fdbm\n",
                    "RxPowerHighWarning",
                    thresh.rx_pwr.warn.high);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fdbm\n",
                    "RxPowerLowWarning",
                    thresh.rx_pwr.warn.low);

        aim_printf (&uc->pvs, "\n");
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fmA\n",
                    "TxBiasHighAlarm",
                    thresh.tx_bias.alarm.high);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fmA\n",
                    "TxBiasLowAlarm",
                    thresh.tx_bias.alarm.low);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fmA\n",
                    "TxBiasHighWarning",
                    thresh.tx_bias.warn.high);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fmA\n",
                    "TxBiasLowWarning",
                    thresh.tx_bias.warn.low);

        aim_printf (&uc->pvs, "\n");
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fdbm\n",
                    "TxPowerHighAlarm",
                    thresh.tx_pwr.alarm.high);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fdbm\n",
                    "TxPowerLowAlarm",
                    thresh.tx_pwr.alarm.low);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fdbm\n",
                    "TxPowerHighWarning",
                    thresh.tx_pwr.warn.high);
        aim_printf (&uc->pvs,
                    "%-20s = %0.2fdbm\n",
                    "TxPowerLowWarning",
                    thresh.tx_pwr.warn.low);
    }
}

void printDdm (ucli_context_t *uc,
               unsigned int port, const uint8_t *buf)
{
    double temp;
    double volt;
    double txBias;
    double txPower;
    double rxPower;
    uint16_t txBiasValue;
    uint16_t txValue;
    uint16_t rxValue;

    temp = (buf[22]) + (buf[23] / 256.0);
    volt = ((buf[26] << 8) | buf[27]) / 10000.0;

    for (int i = 0; i < 4; i++) {
        txBiasValue = (buf[42 + i * 2] << 8) | buf[43 + i
                      * 2];
        txValue = (buf[50 + i * 2] << 8) | buf[51 + i *
                                               2];
        rxValue = (buf[34 + i * 2] << 8) | buf[35 + i *
                                               2];
        /*
         * Measured Tx bias current is represented in mA as a 16-bit unsigned integer with the
         * current defined as the full 16-bit value (0 to 65535) with LSB equal to 2 µA.
         * TX bias ranges from 0mA to 131mA.
         */
        txBias = (131.0 * txBiasValue) / 65535;
        /*
         * Measured Rx optical power and Tx optical power.
         * RX/Tx power ranges from 0mW to 6.5535mW
         */
        txPower = 10 * log10 (0.0001 * txValue);
        rxPower = 10 * log10 (0.0001 * rxValue);

        aim_printf (&uc->pvs,
                    "      %d/%d %10.1f %15.2f %10.2f %16.2f %16.2f\n",
                    port,
                    i,
                    temp,
                    volt,
                    txBias,
                    txPower,
                    rxPower);
    }
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_ddm (ucli_context_t
                                  *uc)
{
    UCLI_COMMAND_INFO (uc,
        "get-ddm", -1,
        "[sport] [dport]");

    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    aim_printf (
        &uc->pvs, "QSFP DDM Info for ports %d to %d\n",
        first_port, last_port);
    aim_printf (&uc->pvs,
                "%10s %10s %15s %10s %16s %16s\n",
                "-------",
                "--------",
                "-----------",
                "--------",
                "--------------",
                "--------------");
    aim_printf (&uc->pvs,
                "%10s %10s %15s %10s %16s %16s\n",
                "Port/Ch",
                "Temp (C)",
                "Voltage (V)",
                "Bias(mA)",
                "Tx Power (dBm)",
                "Rx Power (dBm)");
    aim_printf (&uc->pvs,
                "%10s %10s %15s %10s %16s %16s\n",
                "-------",
                "--------",
                "-----------",
                "--------",
                "--------------",
                "--------------");

    for (port = first_port; port <= last_port; port++) {
        if (!bf_qsfp_is_present (port) ||
            (!bf_qsfp_is_optical (port))) {
            continue;
        }

        if (bf_qsfp_is_cmis (port)) {
            printDdm_for_cmis (uc, port);
        } else {
            memset (buf, 0, sizeof (buf));
            // TBD - use onebank to get flags
            bf_qsfp_module_read (
                port, QSFP_BANKNA, QSFP_PAGE0_LOWER, 0,
                MAX_QSFP_PAGE_SIZE_255, buf);
            printDdm (uc, port, buf);
        }

        aim_printf (&uc->pvs,
                    "%10s %10s %15s %10s %16s %16s\n",
                    "-------",
                    "--------",
                    "-----------",
                    "--------",
                    "--------------",
                    "--------------");
    }

    return 0;
}

static void qsfp_type_to_display_get (
    bf_pltfm_qsfp_type_t qsfp_type,
    char *str,
    int len)
{
    switch (qsfp_type) {
        case BF_PLTFM_QSFP_CU_0_5_M:
            strcpy (str, "Copper 0.5 m");
            break;
        case BF_PLTFM_QSFP_CU_1_M:
            strcpy (str, " Copper 1 m ");
            break;
        case BF_PLTFM_QSFP_CU_2_M:
            strcpy (str, " Copper 2 m ");
            break;
        case BF_PLTFM_QSFP_CU_3_M:
            strcpy (str, " Copper 3 m ");
            break;
        case BF_PLTFM_QSFP_CU_LOOP:
            strcpy (str, " Copper Loop");
            break;
        case BF_PLTFM_QSFP_OPT:
            strcpy (str, "   Optical  ");
            break;
        default:
            strcpy (str, "   Unknown  ");
    }
    str[len - 1] = '\0';
}
char *qsfp_power_class (uint8_t B129)
{
    if ((B129 & 0x3) == 3) {
        return " 7 (5.0 W max.)";
    }
    if ((B129 & 0x3) == 2) {
        return " 6 (4.5 W max.)";
    }
    if ((B129 & 0x3) == 1) {
        return " 5 (4.0 W max.)";
    }
    if ((B129 & 0xC0) == 0xC0) {
        return " 4 (3.5 W max.)";
    }
    if ((B129 & 0xC0) == 0x80) {
        return " 3 (2.5 W max.)";
    }
    if ((B129 & 0xC0) == 0x40) {
        return " 2 (2.0 W max.)";
    }
    if ((B129 & 0xC0) == 0x00) {
        return " 1 (1.5 W max.)";
    }
    return "...           ";
}

char *qsfpdd_power_class (uint8_t B200)
{
    switch (B200 >> 5) {
        case 0:
            return "1 (1.5 W max.)";
        case 1:
            return "2 (3.5 W max.)";
        case 2:
            return "3 (7.0 W max.)";
        case 3:
            return "4 (8.0 W max.)";
        case 4:
            return "5 (10.0 W max.)";
        case 5:
            return "6 (12.0 W max.)";
        case 6:
            return "7 (14.0 W max.)";
        case 7:
            return "8 (>14 W. )";
    }
    return "...";
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfpdd_summary (
    ucli_context_t *uc)
{
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int port;
    uint8_t conn_serial_nbr[BF_PLAT_MAX_QSFP][16] = {{0}};
    int max_port = bf_qsfp_get_max_qsfp_ports();
    char module_type_str[40];

    UCLI_COMMAND_INFO (uc, "info", 0, "info");

    aim_printf (
        &uc->pvs,
        "-----------------------------------------------------------------"
        "-----------------------------------------------------------------\n");
    aim_printf (&uc->pvs,
                "                                                             "
                "Date     Nominal                 Power\n");
    aim_printf (&uc->pvs,
                "Port  Vendor           PN               rev Serial#          "
                "code     Bit Rate     OUI        Class            Media\t "
                "Module-type \n");
    aim_printf (
        &uc->pvs,
        "-----------------------------------------------------------------"
        "-----------------------------------------------------------------\n");

    memset ((char *)conn_serial_nbr, 0,
            sizeof (conn_serial_nbr));

    for (port = 1; port <= max_port; port++) {
        if (!bf_qsfp_is_present (port)) {
            continue;
        }

        if (bf_qsfp_get_cached_info (port,
                                     QSFP_PAGE0_LOWER, buf)) {
            continue;
        }
        if (bf_qsfp_get_cached_info (
                port, QSFP_PAGE0_UPPER,
                buf + MAX_QSFP_PAGE_SIZE)) {
            continue;
        }

        if (!bf_qsfp_is_cmis (port)) {
            continue;
        }

        bf_qsfp_get_module_type_str (port,
                                     module_type_str, false);

        /* print lower page data */
        aim_printf (&uc->pvs, " %2d:  ", port);
        uint8_t vendor[17];
        uint8_t vendor_pn[17];
        uint8_t vendor_rev[3];
        uint8_t vendor_sn[17];
        uint8_t vendor_date[9];

        sfp_copy_string (vendor, buf, 129, 16);
        sfp_copy_string (vendor_pn, buf, 148, 16);
        sfp_copy_string (vendor_rev, buf, 164, 2);
        sfp_copy_string (vendor_sn, buf, 166, 16);
        sfp_copy_string (vendor_date, buf, 182, 8);

        // save serial # for connection check
        memcpy (conn_serial_nbr[port], vendor_sn, 16);

        aim_printf (&uc->pvs, "%s ", vendor);
        aim_printf (&uc->pvs, "%s ", vendor_pn);
        aim_printf (&uc->pvs, "%s  ", vendor_rev);
        aim_printf (&uc->pvs, "%s ", vendor_sn);
        aim_printf (&uc->pvs, "%s ", vendor_date);
        // aim_printf(&uc->pvs, "%5d MBps ", buf[200] * 250);
        aim_printf (&uc->pvs, "---- MBPS ");
        aim_printf (&uc->pvs, "  %02x:%02x:%02x",
                    buf[145], buf[146], buf[147]);
        aim_printf (&uc->pvs, "  %s ",
                    qsfpdd_power_class (buf[200]));

        aim_printf (&uc->pvs, "   %s\t",
                    bf_cmis_get_media_type_string (port));
        aim_printf (&uc->pvs, " %s", module_type_str);
        aim_printf (&uc->pvs, "\n");
    }

    // dump any local connections
    aim_printf (&uc->pvs, "\nLocal connections:\n");
    for (port = 1; port <= max_port; port++) {
        int other_port;
        for (other_port = port + 1; other_port < max_port;
             other_port++) {
            // skip missing qsfps
            if (conn_serial_nbr[port][0] == 0) {
                continue;
            }
            if (conn_serial_nbr[other_port][0] == 0) {
                continue;
            }

            if (strncmp ((char *)conn_serial_nbr[port],
                         (char *)conn_serial_nbr[other_port],
                         16) == 0) {
                aim_printf (&uc->pvs, " Port %2d <--> Port %2d\n",
                            port, other_port);
            }
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_summary (ucli_context_t
                                  *uc)
{
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    uint8_t conn_serial_nbr[max_port + 1][16];

    UCLI_COMMAND_INFO (uc, "info", 0, "info");

    aim_printf (&uc->pvs, "\n================\n");
    aim_printf (&uc->pvs, "Info for QSFP  :\n");
    aim_printf (&uc->pvs, "================\n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "----------------------------------------------------\n");
    aim_printf (&uc->pvs,
                "                                                             "
                "Date     Nominal                 Power\n");
    aim_printf (&uc->pvs,
                "Port  Vendor           PN               rev Serial#          "
                "code     Bit Rate     OUI        Class            Media\n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "----------------------------------------------------\n");

    memset ((char *)conn_serial_nbr, 0,
            sizeof (conn_serial_nbr));

    for (port = 1; port <= max_port; port++) {
        if (!bf_qsfp_is_present (port)) {
            continue;
        }

        if (bf_qsfp_get_cached_info (port,
                                     QSFP_PAGE0_LOWER, buf)) {
            continue;
        }
        if (bf_qsfp_get_cached_info (
                port, QSFP_PAGE0_UPPER,
                buf + MAX_QSFP_PAGE_SIZE)) {
            continue;
        }

        if (bf_qsfp_is_cmis (port)) {
            continue;
        }

        /* print lower page data */
        aim_printf (&uc->pvs, " %2d:  ", port);
        uint8_t vendor[17];
        uint8_t vendor_pn[17];
        uint8_t vendor_rev[3];
        uint8_t vendor_sn[17];
        uint8_t vendor_date[9];

        sfp_copy_string (vendor, buf, 148, 16);
        sfp_copy_string (vendor_pn, buf, 168, 16);
        sfp_copy_string (vendor_rev, buf, 184, 2);
        sfp_copy_string (vendor_sn, buf, 196, 16);
        sfp_copy_string (vendor_date, buf, 212, 8);

        // save serial # for connection check
        memcpy (conn_serial_nbr[port], vendor_sn, 16);

        aim_printf (&uc->pvs, "%s ", vendor);
        aim_printf (&uc->pvs, "%s ", vendor_pn);
        aim_printf (&uc->pvs, "%s  ", vendor_rev);
        aim_printf (&uc->pvs, "%s ", vendor_sn);
        aim_printf (&uc->pvs, "%s ", vendor_date);
        aim_printf (&uc->pvs, "%5d MBps ",
                    buf[140] * 100);
        aim_printf (&uc->pvs,
                    "  %02x:%02x:%02x",
                    buf[165 - 128],
                    buf[166 - 128],
                    buf[167 - 128]);
        aim_printf (&uc->pvs, "  %s ",
                    qsfp_power_class (buf[129]));
        if (bf_qsfp_is_optical (port)) {
            if (buf[146]) {
                aim_printf (&uc->pvs, "  (Optical): %d m ",
                            buf[146]);
            }
        } else {
            if (buf[146]) {
                aim_printf (&uc->pvs, "  (Copper): %d m ",
                            buf[146]);
            }
        }
        if (buf[142]) {
            aim_printf (&uc->pvs, "  (SMF): %d km ",
                        buf[142]);
        }
        if (buf[143]) {
            aim_printf (&uc->pvs, "  (OM3): %d m ",
                        buf[143] * 2);
        }
        if (buf[144]) {
            aim_printf (&uc->pvs, "  (OM2): %d m ",
                        buf[144]);
        }
        if (buf[145]) {
            aim_printf (&uc->pvs, "  (OM1): %d m ",
                        buf[145]);
        }
        aim_printf (&uc->pvs, "\n");
    }

    // dump any local connections
    aim_printf (&uc->pvs, "\nLocal connections:\n");
    for (port = 1; port <= max_port; port++) {
        int other_port;
        for (other_port = port + 1; other_port < max_port;
             other_port++) {
            // skip missing qsfps
            if (conn_serial_nbr[port][0] == 0) {
                continue;
            }
            if (conn_serial_nbr[other_port][0] == 0) {
                continue;
            }

            if (strncmp ((char *)conn_serial_nbr[port],
                         (char *)conn_serial_nbr[other_port],
                         16) == 0) {
                aim_printf (&uc->pvs, " Port %2d <--> Port %2d\n",
                            port, other_port);
            }
        }
    }
    aim_printf (&uc->pvs, "\n================\n");
    aim_printf (&uc->pvs, "Info for CMIS:\n");
    aim_printf (&uc->pvs, "================\n");
    bf_pltfm_ucli_ucli__qsfpdd_summary (uc);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfpdd_show_for_copper (
    ucli_context_t *uc)
{
    qsfp_vendor_info_t vendor;
    char module_type_str[40];
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    aim_printf (&uc->pvs, "\n================\n");
    aim_printf (&uc->pvs, "Show for CMIS:\n");
    aim_printf (&uc->pvs, "================\n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");
    aim_printf (&uc->pvs,
                "port  module-type      vendor           part_num           "
                "Cu-Len\tconn-type\n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");
    for (port = 1; port <= max_port; port++) {
        if (!bf_qsfp_is_present (port)) {
            continue;
        }
        if (!bf_qsfp_is_cmis (port)) {
            continue;
        }
        if (bf_qsfp_is_optical (port)) {
            continue;
        }

        // grab the cached vendor info
        if (!bf_qsfp_get_vendor_info (port, &vendor)) {
            continue;
        }
        if (!bf_qsfp_get_module_type_str (port,
                                          module_type_str, true)) {
            continue;
        }

        aim_printf (&uc->pvs,
                    " %-2d   %-14s   %-16s %-16s",
                    port,
                    module_type_str,
                    vendor.name,
                    vendor.part_number);
        aim_printf (&uc->pvs,
                    "  %-8s \t%s\n",
                    bf_cmis_get_media_type_string (port),
                    bf_qsfp_get_conn_type_string (port));
    }

    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfpdd_show (ucli_context_t
                                 *uc)
{
    qsfp_vendor_info_t vendor;
    char module_type_str[40];
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    bf_pltfm_qsfpdd_type_t qsfpdd_type =
        BF_PLTFM_QSFPDD_UNKNOWN;

    // Display for copper
    bf_pltfm_ucli_ucli__qsfpdd_show_for_copper (uc);

    // Display for Optics
    aim_printf (&uc->pvs,
                "port  module-type      vendor           part_num           "
                "Optics\tconn-type \n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");
    for (port = 1; port <= max_port; port++) {
        if (!bf_qsfp_is_present (port)) {
            continue;
        }
        if (!bf_qsfp_is_cmis (port)) {
            continue;
        }
        if (!bf_qsfp_is_optical (port)) {
            continue;
        }

        // grab the cached vendor info
        if (!bf_qsfp_get_vendor_info (port, &vendor)) {
            continue;
        }
        if (!bf_qsfp_get_module_type_str (port,
                                          module_type_str, true)) {
            continue;
        }
        if (bf_cmis_type_get (port, &qsfpdd_type) != 0) {
            qsfpdd_type = BF_PLTFM_QSFPDD_UNKNOWN;
        }

        aim_printf (&uc->pvs,
                    " %-2d   %-14s   %-16s %-16s",
                    port,
                    module_type_str,
                    vendor.name,
                    vendor.part_number);
        aim_printf (&uc->pvs,
                    "  %-8s \t%s\n",
                    bf_cmis_get_media_type_string (port),
                    bf_qsfp_get_conn_type_string (port));
    }

    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");

    return 0;
}

void printChannelMonitor_for_dd (ucli_context_t
                                 *uc,
                                 unsigned int index,
                                 const uint8_t *buf,
                                 unsigned int rxMSB,
                                 unsigned int rxLSB,
                                 unsigned int txMSB,
                                 unsigned int txLSB,
                                 unsigned int txpwrMSB,
                                 unsigned int txpwrLSB)
{
    /* Notes about DDM register values:
        - MSB in lower byte
        - For Tx/Rx power, each unit is 1uW
        - Converting to dBm is 10*log( mW)
     */
    uint16_t rxValue = (buf[rxMSB] << 8) | buf[rxLSB];
    uint16_t txValue = (buf[txMSB] << 8) | buf[txLSB];
    uint16_t txpwrValue = (buf[txpwrMSB] << 8) |
                          buf[txpwrLSB];

    /* RX power ranges from 0mW to 6.5535mW*/
    double rxPower = 10 * log10 (0.0001 * rxValue);

    /* TX power ranges from 0mW to 6.5535mW */
    double txPower = 10 * log10 (0.0001 * txpwrValue);

    uint8_t txbias_mul = 1;
    uint8_t tx_bias_mval = (buf[160] >> 3) & 0x3;
    if (tx_bias_mval) {
        if (tx_bias_mval == 1) {
            txbias_mul = 2;
        } else {
            txbias_mul = 4;
        }
    }

    /* TX bias ranges from 0mA to 131mA */
    double txBias = ((double) (txbias_mul *
                               txValue)) / 65535;
    txBias = (131.0 * txValue) / 65535;

    aim_printf (&uc->pvs,
                "    Channel %d:   %9.2fdBm  %9.2fdBm  %10.2fmA\n",
                index,
                rxPower,
                txPower,
                txBias);
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_show (ucli_context_t
                               *uc)
{
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    uint8_t vendor[17];
    uint8_t vendor_pn[17];
    char optical_lstr[16];
    bf_pltfm_qsfp_type_t qsfp_type =
        BF_PLTFM_QSFP_UNKNOWN;
    char display_qsfp_type[13];
    uint32_t flags = bf_pltfm_mgr_ctx()->flags;
    bool temper_monitor_en = (0 != (flags & AF_PLAT_MNTR_QSFP_REALTIME_DDM));

    UCLI_COMMAND_INFO (uc,
        "show", 0,
        "show qsfp or qsfp-dd summary information");

    aim_printf (&uc->pvs,
                "Max ports supported  ==> %2d\n", max_port);
    aim_printf (&uc->pvs,
                "Temperature monitor enabled ==> %s",
                temper_monitor_en ? "YES" : "NO");
    if (temper_monitor_en) {
        aim_printf (&uc->pvs,
                    " (interval %d seconds)\n",
                    bf_port_qsfp_mgmnt_temper_monitor_period_get());
    }
    aim_printf (&uc->pvs, "\n================\n");
    aim_printf (&uc->pvs, "Show for QSFP  :\n");
    aim_printf (&uc->pvs, "================\n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");
    aim_printf (&uc->pvs,
                "port  id    vendor           part_num       eth  ext-eth Om-len "
                "Cu-Len  Qsfp-type\n");
    aim_printf (&uc->pvs,
                "                                           cmpl   cmpl"
                "               \n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");
    for (port = 1; port <= max_port; port++) {
        if (!bf_qsfp_is_present (port)) {
            continue;
        }

        if (bf_qsfp_get_cached_info (port,
                                     QSFP_PAGE0_LOWER, buf)) {
            continue;
        }
        if (bf_qsfp_get_cached_info (
                port, QSFP_PAGE0_UPPER,
                buf + MAX_QSFP_PAGE_SIZE)) {
            continue;
        }

        if (bf_qsfp_is_cmis (port)) {
            continue;
        }

        sfp_copy_string (vendor, buf, 148, 16);
        sfp_copy_string (vendor_pn, buf, 168, 16);
        /* determine optical domain length to display */
        if (buf[142]) {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "SMF %d km", buf[142]);
        } else if (buf[143]) {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "OM3 %d m", buf[143] * 2);
        } else if (buf[144]) {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "OM2 %d m", buf[144]);
        } else if (buf[145]) {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "OM1 %d m", buf[145]);
        } else {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "--0--");
        }
        /* get qsfp_type */
        if (bf_qsfp_type_get (port, &qsfp_type) != 0) {
            qsfp_type = BF_PLTFM_QSFP_UNKNOWN;
        }
        qsfp_type_to_display_get (
            qsfp_type, display_qsfp_type,
            sizeof (display_qsfp_type));
        /* now display the stuff */
        aim_printf (
            &uc->pvs, " %2d  0x%02x %16s %16s", port, buf[0],
            vendor, vendor_pn);
        aim_printf (&uc->pvs,
                    "0x%02x  0x%02x %10s  %d m",
                    buf[131],
                    buf[192],
                    optical_lstr,
                    buf[146]);
        aim_printf (&uc->pvs, " %s\n", display_qsfp_type);
    }
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");
    bf_pltfm_ucli_ucli__qsfpdd_show (uc);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfpdd_dump_info (
    ucli_context_t *uc)
{
    uint8_t buf17[MAX_QSFP_PAGE_SIZE * 2] = {0};
    int port;
    qsfp_module_state_info_t module_state_info;
    uint8_t spec_major_rev = 0;
    char buf[16];
    uint8_t data = 0;
    uint8_t buf2[2] = {0};
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (uc,
        "dump-info", 1,
        "<port>");

    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    if (!bf_qsfp_is_present (port)) {
        return 0;
    }
    if (!bf_qsfp_is_cmis (port)) {
        return 0;
    }

    // Show information specific to optics

    aim_printf (&uc->pvs,
                "## MODULE STATE MACHINE\n");
    bf_qsfp_get_module_state_info (port,
                                   &module_state_info);
    aim_printf (&uc->pvs,
                "%-30s = %s\n",
                "Module state",
                module_state_info.module_state_str);
    aim_printf (&uc->pvs,
                "%-30s = %d\n",
                "ForceLowPwr bit",
                module_state_info.forceLowPwr);
    bf_cmis_spec_rev_get (port, &spec_major_rev,
                          NULL);
    if (spec_major_rev >= 4) {
        aim_printf (
            &uc->pvs, "%-30s = %d\n", "LowPwr bit",
            module_state_info.lowPwr);
    }
    if (bf_port_qsfp_mgmnt_temper_high_alarm_flag_get (
            port)) {
        aim_printf (&uc->pvs,
                    "** LowPWr due to high temp     = %f C\n",
                    bf_port_qsfp_mgmnt_temper_high_record_get (port));
    }
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs,
                "## DATA PATH STATE MACHINE\n");
    int cur_ch, num_ch = bf_qsfp_get_ch_cnt (port);
    qsfp_datapath_state_info_t dp_st_info[num_ch];

    bf_qsfp_get_dp_state_info (port, num_ch,
                               dp_st_info);
    aim_printf (&uc->pvs, "%-25s", " ");
    for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
        sprintf (buf, "LN%d", cur_ch + 1);
        aim_printf (&uc->pvs, "%-5s", buf);
    }
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs, "%-25s", "Data path state");
    for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
        aim_printf (&uc->pvs, "%-5s",
                    dp_st_info[cur_ch].datapath_state_str_short);
    }
    aim_printf (&uc->pvs, "\n");
    if (spec_major_rev >= 4) {
        aim_printf (&uc->pvs, "%-25s",
                    "DataPathDeinit bits");
    } else {
        aim_printf (&uc->pvs, "%-25s",
                    "DataPathPwrUp bits");
    }
    for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
        aim_printf (&uc->pvs, "%-5d",
                    dp_st_info[cur_ch].data_path_deinit_bit);
    }
    aim_printf (&uc->pvs, "\n\n");

    // TBD - use onebank to get flags
    bf_qsfp_module_read (
        port, QSFP_BANK0, QSFP_PAGE17, 128, 128,
        buf17 + MAX_QSFP_PAGE_SIZE);

    aim_printf (&uc->pvs, "## LATCHED FLAGS\n");
    aim_printf (&uc->pvs,
                "TX Latched Flags (Stop periodics and read twice):\n");
    aim_printf (&uc->pvs, "  TX-Fault: 0x%0x \n",
                buf17[135]);
    aim_printf (&uc->pvs, "  TX-LOS: 0x%0x \n",
                buf17[136]);
    aim_printf (&uc->pvs, "  TX-CDR-LOL: 0x%0x \n",
                buf17[137]);
    aim_printf (&uc->pvs,
                "  TX-Adaptive Input Eq Fault: 0x%0x \n",
                buf17[138]);

    aim_printf (&uc->pvs,
                "  TX-output power High Alarm: 0x%0x \n",
                buf17[139]);
    aim_printf (&uc->pvs,
                "  TX-output power Low Alarm: 0x%0x \n",
                buf17[140]);
    aim_printf (&uc->pvs,
                "  TX-output power High warning: 0x%0x \n",
                buf17[141]);
    aim_printf (&uc->pvs,
                "  TX-output power Low warning: 0x%0x \n",
                buf17[142]);

    // if (!bf_qsfp_field_read_onebank(port, CHANNEL_TX_BIAS_ALARMS, 0, 0, 4,
    // &buf4[0])) {
    //}
    aim_printf (&uc->pvs,
                "  TX-Bias High Alarm: 0x%0x \n", buf17[143]);
    aim_printf (&uc->pvs,
                "  TX-Bias Low Alarm: 0x%0x \n", buf17[144]);
    aim_printf (&uc->pvs,
                "  TX-Bias High warning: 0x%0x \n", buf17[145]);
    aim_printf (&uc->pvs,
                "  TX-Bias Low warning: 0x%0x \n", buf17[146]);

    aim_printf (&uc->pvs, "RX Flags:\n");
    aim_printf (&uc->pvs, "  RX-LOS: 0x%0x \n",
                buf17[147]);
    aim_printf (&uc->pvs, "  RX-CDR-LOL: 0x%0x \n",
                buf17[148]);

    aim_printf (&uc->pvs,
                "  RX-input power High Alarm: 0x%0x \n",
                buf17[149]);
    aim_printf (&uc->pvs,
                "  RX-input power Low Alarm: 0x%0x \n",
                buf17[150]);
    aim_printf (&uc->pvs,
                "  RX-input power High warning: 0x%0x \n",
                buf17[151]);
    aim_printf (&uc->pvs,
                "  RX-input power Low warning: 0x%0x \n",
                buf17[152]);

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "## POWER/TEMPERATURE\n");

    //  if (!bf_qsfp_field_read_onebank(port, CHANNEL_TX_DISABLE, 0, 0, 1, &data))
    //  {
    //    aim_printf(&uc->pvs, "%-30s = 0x%0x\n", "TX DISABLE", data);
    //  }

    if (!bf_qsfp_field_read_onebank (port, PWR_CLASS,
                                     0, 0, 1, &data)) {
        aim_printf (&uc->pvs, "  Power Class: %s \n",
                    qsfpdd_power_class (data));
    }
    if (!bf_qsfp_field_read_onebank (port,
                                     TEMPERATURE, 0, 0, 2, &buf2[0])) {
        double temp = (buf2[0]) + (buf2[1] / 256.0);
        aim_printf (&uc->pvs, "  Temperature: %f C\n",
                    temp);
    }
    if (!bf_qsfp_field_read_onebank (port, VCC, 0, 0,
                                     2, &buf2[0])) {
        double voltage = ((buf2[0] << 8) | buf2[1]) /
                         10000.0;
        aim_printf (&uc->pvs, "  Supply Voltage: %f V\n",
                    voltage);
    }

    aim_printf (&uc->pvs,
                "  Channel Data:  %12s     %9s   %11s\n",
                "RX Power",
                "TX Power",
                "TX Bias");

    uint8_t rxPwrByte = 186, txPwrByte = 154,
            txBiasByte = 170;
    uint8_t i = 0;
    uint8_t max_ch = bf_qsfp_get_media_ch_cnt (
                               port);
    for (i = 0; i < max_ch; i++) {
        printChannelMonitor_for_dd (uc,
                                    i + 1,
                                    buf17,
                                    rxPwrByte,
                                    rxPwrByte + 1,
                                    txBiasByte,
                                    txBiasByte + 1,
                                    txPwrByte,
                                    txPwrByte + 1);
        rxPwrByte = rxPwrByte + 2;
        txPwrByte = txPwrByte + 2;
        txBiasByte = txBiasByte + 2;
    }
    printThreshold (uc, port);

    // Add average power, los, lol etc - TBD
    //
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_dump_info (
    ucli_context_t *uc)
{
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int port;
    qsfp_vendor_info_t vendor;
    char module_type_str[20];
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (uc,
        "dump-info", 1,
        "<port>");

    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }
    if (!bf_qsfp_is_present (port)) {
        return 0;
    }

    aim_printf (&uc->pvs, "%-30s = %d\n", "Port",
                port);
    bf_qsfp_get_module_type_str (port,
                                 module_type_str, true);
    aim_printf (
        &uc->pvs, "%-30s = %s\n",
        "Module type identifier", module_type_str);
    if (bf_qsfp_is_cmis (port)) {
        aim_printf (&uc->pvs,
                    "%-30s = %s\n",
                    "Module",
                    bf_cmis_get_media_type_string (port));
    } else {
        char ext_code_desc[255];
        bf_qsfp_get_sff_eth_extended_code_description (
            port, ext_code_desc);
        aim_printf (&uc->pvs, "%-30s = %s\n", "Module",
                    ext_code_desc);
    }
    aim_printf (&uc->pvs, "\n");

    bf_qsfp_get_vendor_info (port, &vendor);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Vendor name", vendor.name);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Vendor P/N", vendor.part_number);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Vendor rev", vendor.rev);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Vendor S/N", vendor.serial_number);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Date & lot code", vendor.date_code);
    aim_printf (&uc->pvs,
                "%-30s = %02X-%02X-%02X\n",
                "Vendor OUI",
                vendor.oui[0],
                vendor.oui[1],
                vendor.oui[2]);

    aim_printf (&uc->pvs, "\n");
    if (!bf_qsfp_is_optical (port)) {
        return 0;
    }

    if (bf_qsfp_is_cmis (port)) {
        bf_pltfm_ucli_ucli__qsfpdd_dump_info (uc);
        return 0;
    }

    // TBD - use onebank to get flags
    bf_qsfp_module_read (port, QSFP_BANKNA,
                         QSFP_PAGE0_LOWER, 0, 128, buf);
    bf_qsfp_module_read (port, QSFP_BANKNA,
                         QSFP_PAGE0_UPPER, 128, 128, buf + MAX_QSFP_PAGE_SIZE);

    aim_printf (&uc->pvs, "## MODULE STATUS\n");
    aim_printf (&uc->pvs, "Status: 0x%02x 0x%02x\n",
                buf[1], buf[2]);

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "## OTHER MODULE INFO\n");
    aim_printf (&uc->pvs,
                "  Extended ID:  0x%02x\t"
                "Ethernet Compliance:  0x%02x\n"
                "  Extended Ethernet Compliance:  0x%02x\n",
                buf[129],
                buf[131],
                buf[192]);

    aim_printf (&uc->pvs, "  Connector: 0x%02x\n",
                buf[130]);
    aim_printf (&uc->pvs,
                "  Spec compliance: "
                "0x%02x 0x%02x 0x%02x 0x%02x"
                "0x%02x 0x%02x 0x%02x 0x%02x\n",
                buf[131],
                buf[132],
                buf[133],
                buf[134],
                buf[135],
                buf[136],
                buf[137],
                buf[138]);
    aim_printf (&uc->pvs, "  Encoding: 0x%02x\n",
                buf[139]);
    aim_printf (&uc->pvs,
                "  Nominal Bit Rate: %d MBps\n", buf[140] * 100);
    aim_printf (&uc->pvs,
                "  Ext rate select compliance: 0x%#02x\n",
                buf[141]);
    aim_printf (&uc->pvs, "  Length (SMF): %d km\n",
                buf[142]);
    aim_printf (&uc->pvs, "  Length (OM3): %d m\n",
                buf[143] * 2);
    aim_printf (&uc->pvs, "  Length (OM2): %d m\n",
                buf[144]);
    aim_printf (&uc->pvs, "  Length (OM1): %d m\n",
                buf[145]);
    aim_printf (&uc->pvs, "  Length (Copper): %d m\n",
                buf[146]);
    aim_printf (&uc->pvs, "  Device Tech: 0x%02x\n",
                buf[147]);
    aim_printf (&uc->pvs, "  Ext Module: 0x%02x\n",
                buf[164]);
    aim_printf (
        &uc->pvs, "  Wavelength tolerance: 0x%02x 0x%02x\n",
        buf[188], buf[189]);
    aim_printf (&uc->pvs, "  Max case temp: %dC\n",
                buf[190]);
    aim_printf (&uc->pvs, "  CC_BASE: 0x%02x\n",
                buf[191]);
    aim_printf (&uc->pvs,
                "  Options: 0x%02x 0x%02x 0x%02x 0x%02x\n",
                buf[192],
                buf[193],
                buf[194],
                buf[195]);
    aim_printf (&uc->pvs, "  DOM Type: 0x%02x\n",
                buf[220]);
    aim_printf (&uc->pvs,
                "  Enhanced Options: 0x%02x\n", buf[221]);
    aim_printf (&uc->pvs, "  Reserved: 0x%02x\n",
                buf[222]);
    aim_printf (&uc->pvs, "  CC_EXT: 0x%02x\n",
                buf[223]);
    aim_printf (&uc->pvs, "  Vendor Specific:\n");
    aim_printf (&uc->pvs,
                "    %02x %02x %02x %02x %02x %02x %02x %02x"
                "  %02x %02x %02x %02x %02x %02x %02x %02x\n",
                buf[224],
                buf[225],
                buf[226],
                buf[227],
                buf[228],
                buf[229],
                buf[230],
                buf[231],
                buf[232],
                buf[233],
                buf[234],
                buf[235],
                buf[236],
                buf[237],
                buf[238],
                buf[239]);
    aim_printf (&uc->pvs,
                "    %02x %02x %02x %02x %02x %02x %02x %02x"
                "  %02x %02x %02x %02x %02x %02x %02x %02x\n",
                buf[240],
                buf[241],
                buf[242],
                buf[243],
                buf[244],
                buf[245],
                buf[246],
                buf[247],
                buf[248],
                buf[249],
                buf[250],
                buf[251],
                buf[252],
                buf[253],
                buf[254],
                buf[255]);

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "## INTERRUPT FLAGS\n");
    aim_printf (&uc->pvs, "    LOS: 0x%02x\n",
                buf[3]);
    aim_printf (&uc->pvs, "    Fault: 0x%02x\n",
                buf[4]);
    aim_printf (&uc->pvs, "    Temp: 0x%02x\n",
                buf[6]);
    aim_printf (&uc->pvs, "    Vcc: 0x%02x\n",
                buf[7]);
    aim_printf (&uc->pvs,
                "    Rx Power: 0x%02x 0x%02x\n", buf[9], buf[10]);
    aim_printf (&uc->pvs,
                "    Tx Bias: 0x%02x 0x%02x\n", buf[11], buf[12]);
    aim_printf (&uc->pvs,
                "    Reserved Set 3: 0x%02x 0x%02x\n", buf[13],
                buf[14]);
    aim_printf (&uc->pvs,
                "    Reserved Set 4: 0x%02x 0x%02x\n", buf[15],
                buf[16]);
    aim_printf (&uc->pvs,
                "    Reserved Set 5: 0x%02x 0x%02x\n", buf[17],
                buf[18]);
    aim_printf (&uc->pvs,
                "    Vendor Defined: 0x%02x 0x%02x 0x%02x\n",
                buf[19],
                buf[20],
                buf[21]);

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "## POWER/TEMPERATURE\n");
    aim_printf (&uc->pvs,
                "  Power set/LPMODE:  0x%02x\n", buf[93]);
    aim_printf (&uc->pvs,
                "  TX disable bits: 0x%02x\n", buf[86]);
    aim_printf (&uc->pvs,
                "  TX rate select bits: 0x%02x\n", buf[88]);

    double temp = (buf[22]) + (buf[23] / 256.0);
    aim_printf (&uc->pvs, "  Temperature: %f C\n",
                temp);
    double voltage = (buf[26] << 8) | buf[27];
    aim_printf (&uc->pvs, "  Supply Voltage: %f V\n",
                voltage / 10000.0);

    aim_printf (&uc->pvs,
                "  Channel Data:  %12s     %9s   %11s\n",
                "RX Power",
                "TX Power",
                "TX Bias");
    printChannelMonitor (uc, 1, buf, 34, 35, 42, 43,
                         50, 51);
    printChannelMonitor (uc, 2, buf, 36, 37, 44, 45,
                         52, 53);
    printChannelMonitor (uc, 3, buf, 38, 39, 46, 47,
                         54, 55);
    printChannelMonitor (uc, 4, buf, 40, 41, 48, 49,
                         56, 57);
    aim_printf (&uc->pvs,
                "    Reported RX Power is %s\n",
                (buf[220] & 0x08) ? "average power" : "OMA");

    printThreshold (uc, port);
    return 0;
}

static int bf_pltfm_ucli_ucli__qsfp_mem_show_dp (
    ucli_context_t *uc,
    qsfp_application_info_t *app_info,
    qsfp_datapath_staged_set_info_t *dp_ss_info,
    Control_Sets set_num,
    int num_ch)
{
    // only call this subroutine for CMIS cables
    // set_num == -1 means Active Set
    char buf[26];
    int cur_ch;
    if (set_num == ACTIVE_SET) {
        aim_printf (&uc->pvs, "%-25s", "ACTIVE SET");
    } else {
        sprintf (buf, "%s %d", "STAGED SET", set_num);
        aim_printf (&uc->pvs, "%-25s", buf);
    }
    for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
        sprintf (buf, "LN%d", cur_ch + 1);
        aim_printf (&uc->pvs, "%-5s", buf);
    }
    aim_printf (&uc->pvs, "\n");

    /*   aim_printf(&uc->pvs, "%-25s", "Active data path visual");
      for(cur_ch = 0; cur_ch < num_ch; cur_ch ++) {
        sprintf(buf, "LN%d", cur_ch + 1);
        aim_printf(&uc->pvs, "%-5s", buf);
       }
      aim_printf(&uc->pvs, "\n");*/

    aim_printf (&uc->pvs, "%-25s",
                "ApSel code selected");
    for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
        aim_printf (&uc->pvs, "%-5d",
                    dp_ss_info[cur_ch].selected_ApSel_code);
    }
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs, "%-25s",
                "Data path ID (1stLn-1)");
    for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
        aim_printf (&uc->pvs, "%-5d",
                    dp_ss_info[cur_ch].data_path_id);
    }
    aim_printf (&uc->pvs, "\n");
    return 0;
}

static int bf_pltfm_ucli_ucli__qsfp_module_show (
    ucli_context_t *uc, int port)
{
    aim_printf (&uc->pvs,
                "Module memory dump, port %d\n", port);

    char module_type_str[20];
    int cur_ch, num_ch;
    bool is_cmis, is_sff8636, is_flat_mem,
         is_checksum;
    char memmap_spec_rev[25];
    uint8_t spec_major_rev = 0;
    char buf[16];
    qsfp_vendor_info_t vendor;

    is_cmis = bf_qsfp_is_cmis (port);
    is_sff8636 = bf_qsfp_is_sff8636 (port);

    bf_qsfp_get_module_type_str (port,
                                 module_type_str, true);
    aim_printf (
        &uc->pvs, "%-30s = %s\n",
        "Module type identifier", module_type_str);

    if (is_cmis) {
        aim_printf (&uc->pvs,
                    "%-30s = %s\n",
                    "Module",
                    bf_cmis_get_media_type_string (port));
    }

    num_ch = bf_qsfp_get_ch_cnt (port);
    aim_printf (&uc->pvs, "%-30s = %d\n",
                "Number of lanes", num_ch);
    aim_printf (&uc->pvs, "\n");

    bf_qsfp_get_vendor_info (port, &vendor);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Vendor name", vendor.name);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Vendor P/N", vendor.part_number);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Vendor rev", vendor.rev);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Vendor S/N", vendor.serial_number);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Date & lot code", vendor.date_code);
    aim_printf (&uc->pvs,
                "%-30s = %02X-%02X-%02X\n",
                "Vendor OUI",
                vendor.oui[0],
                vendor.oui[1],
                vendor.oui[2]);
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs, "%-30s = ",
                "Memory map format");
    if (is_cmis) {
        aim_printf (&uc->pvs, "CMIS\n");
        bf_cmis_spec_rev_get (port, &spec_major_rev,
                              NULL);
    } else if (is_sff8636) {
        aim_printf (&uc->pvs, "SFF-8636\n");
    } else {
        aim_printf (&uc->pvs, "Unknown\n");
    }
    bf_qsfp_get_spec_rev_str (port, memmap_spec_rev);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Memory map spec rev", memmap_spec_rev);

    is_flat_mem = bf_qsfp_is_flat_mem (port);
    aim_printf (&uc->pvs, "%-30s = ",
                "Flat/paged memory");
    if (is_flat_mem) {
        aim_printf (&uc->pvs, "Flat\n");
    } else {
        aim_printf (&uc->pvs, "Paged\n");
    }

    is_checksum = bf_qsfp_is_checksum_passed (port);
    aim_printf (&uc->pvs, "%-30s = ", "Checksum");
    if (is_checksum) {
        aim_printf (&uc->pvs, "PASSED\n");
    } else {
        aim_printf (&uc->pvs, "FAILED\n");
    }
    aim_printf (&uc->pvs, "\n");

    qsfp_cable_t qsfp_cable;
    bool is_passive_cu;
    is_passive_cu = bf_qsfp_is_passive_cu (port);
    char media_type_str[46];
    bf_qsfp_get_media_type_str (port, media_type_str);
    aim_printf (&uc->pvs, "%-30s = %s\n",
                "Media type", media_type_str);
    aim_printf (&uc->pvs,
                "%-30s = %s\n",
                "Media side connector",
                bf_qsfp_get_conn_type_string (port));
    bf_qsfp_get_cable_info (port, &qsfp_cable);
    if (qsfp_cable.is_cable_assy) {
        aim_printf (&uc->pvs,
                    "%-30s = %.1f m\n",
                    "Cable assembly length",
                    qsfp_cable.cable_assy);
    } else {
        if (qsfp_cable._isset.xcvr_single_mode) {
            aim_printf (&uc->pvs,
                        "%-30s = %d m\n",
                        "Max supported SMF fiber length",
                        qsfp_cable.xcvr_single_mode);
        }
        if (qsfp_cable._isset.xcvr_om1) {
            aim_printf (&uc->pvs,
                        "%-30s = %d m\n",
                        "Max supported OM1 fiber length",
                        qsfp_cable.xcvr_om1);
        }
        if (qsfp_cable._isset.xcvr_om2) {
            aim_printf (&uc->pvs,
                        "%-30s = %d m\n",
                        "Max supported OM2 fiber length",
                        qsfp_cable.xcvr_om2);
        }
        if (qsfp_cable._isset.xcvr_om3) {
            aim_printf (&uc->pvs,
                        "%-30s = %d m\n",
                        "Max supported OM3 fiber length",
                        qsfp_cable.xcvr_om3);
        }
        if (qsfp_cable._isset.xcvr_om4) {
            aim_printf (&uc->pvs,
                        "%-30s = %d m\n",
                        "Max supported OM4 fiber length",
                        qsfp_cable.xcvr_om4);
        }
        if (qsfp_cable._isset.xcvr_om5) {
            aim_printf (&uc->pvs,
                        "%-30s = %d m\n",
                        "Max supported OM5 fiber length",
                        qsfp_cable.xcvr_om5);
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* returns the nominal wavelength and the wavelength tolerance */
    double nominal, tolerance;
    if ((!is_passive_cu) &&
        (bf_qsfp_get_wavelength_info (port,
                                      &nominal,
                                      &tolerance))) {
        aim_printf (&uc->pvs,
                    "%-30s = %.2f nm\n",
                    "Nominal wavelength",
                    nominal);
        aim_printf (&uc->pvs,
                    "%-30s = ±%.3f nm\n",
                    "Wavelength tolerance",
                    tolerance);
    }
    bool ctrl_flag, tune_flag;
    /* returns the wavelength controllable and tramsmitter tunable flags */
    if ((is_cmis) && (!is_passive_cu) &&
        bf_qsfp_get_wavelength_flags (port,
                                      &ctrl_flag,
                                      &tune_flag)) {
        aim_printf (&uc->pvs,
                    "%-30s = %s\n",
                    "Wavelength control",
                    (ctrl_flag ? "Active control" : "Not supported"));
        aim_printf (&uc->pvs,
                    "%-30s = %s\n",
                    "Wavelength tunable",
                    (tune_flag ? "True" : "False"));
        aim_printf (&uc->pvs, "\n");
    }

    /* returns the active and inactive firmware versions */
    uint8_t active_ver_major, active_ver_minor;
    uint8_t inactive_ver_major, inactive_ver_minor;
    if ((!is_passive_cu) &&
        (bf_qsfp_get_firmware_ver (port,
                                   &active_ver_major,
                                   &active_ver_minor,
                                   &inactive_ver_major,
                                   &inactive_ver_minor))) {
        aim_printf (&uc->pvs,
                    "%-30s = %d.%d\n",
                    "Active firmware version",
                    active_ver_major,
                    active_ver_minor);
        if (spec_major_rev >= 4) {
            aim_printf (&uc->pvs,
                        "%-30s = %d.%d\n",
                        "Inactive firmware version",
                        inactive_ver_major,
                        inactive_ver_minor);
        }

        uint8_t hw_ver_major, hw_ver_minor;
        bf_qsfp_get_hardware_ver (port, &hw_ver_major,
                                  &hw_ver_minor);
        aim_printf (&uc->pvs,
                    "%-30s = %d.%d\n",
                    "Hardware version",
                    hw_ver_major,
                    hw_ver_minor);
        aim_printf (&uc->pvs, "\n");
    }

    aim_printf (&uc->pvs, "## SUPPORTED ");
    if (is_cmis) {
        aim_printf (&uc->pvs, "APPLICATIONS\n");
        aim_printf (&uc->pvs, "%-7s", " ");
    } else {
        aim_printf (&uc->pvs, "SPECIFICATIONS\n");
    }

    if ((is_cmis) && (!is_passive_cu)) {
        aim_printf (&uc->pvs,
                    "%s  %s\n",
                    "------------------- Host interface -------------------",
                    "------------------- Media interface ------------------");
    } else {
        aim_printf (
            &uc->pvs, "%s\n",
            "---------------- Host interface -----------------");
    }

    if (is_cmis) {
        aim_printf (&uc->pvs, "%-7s", "ApSel");
    }
    aim_printf (&uc->pvs, "%-46s%-4s",
                "Standard supported", "Lns");
    if ((is_cmis) && (!is_passive_cu)) {
        aim_printf (&uc->pvs,
                    "%-4s  %-46s%-4s%s",
                    "Mask",
                    "Standard supported",
                    "Lns",
                    "Mask");
    }
    aim_printf (&uc->pvs, "\n");

    int app_count = bf_qsfp_get_application_count (
                        port);
    qsfp_application_info_t app_info[app_count];
    if (app_count == 0) {
        aim_printf (&uc->pvs, "NONE ADVERTISED\n");
    } else {
        for (int cur_app = 0; cur_app < app_count;
             cur_app++) {
            bf_qsfp_get_application0 (port, cur_app + 1,
                                     &app_info[cur_app]);
            if ((app_info[cur_app].host_if_id == 0) &&
                (app_info[cur_app].host_lane_cnt == 0)) {
                // some old modules just used all zeros to signify the end of the list
                app_count = cur_app;
                break;
            }
            if (is_cmis) {
                aim_printf (&uc->pvs, "%-7d", cur_app + 1);
            }
            aim_printf (&uc->pvs,
                        "%-46s%-4d",
                        app_info[cur_app].host_if_id_str,
                        app_info[cur_app].host_lane_cnt);
            if ((is_cmis) && (!is_passive_cu)) {
                aim_printf (&uc->pvs, "0x%02X",
                            app_info[cur_app].host_lane_assign_mask);
                aim_printf (&uc->pvs,
                            "  %-46s%-4d",
                            app_info[cur_app].media_if_id_str,
                            app_info[cur_app].media_lane_cnt);
                aim_printf (
                    &uc->pvs, "0x%02X",
                    app_info[cur_app].media_lane_assign_mask);
            }
            aim_printf (&uc->pvs, "\n");
        }
        aim_printf (&uc->pvs, "\n");
    }

    if ((is_cmis) && (!is_passive_cu)) {
        qsfp_module_state_info_t module_state_info;
        aim_printf (&uc->pvs,
                    "## MODULE STATE MACHINE\n");
        bf_qsfp_get_module_state_info (port,
                                       &module_state_info);
        aim_printf (&uc->pvs,
                    "%-30s = %s\n",
                    "Module state",
                    module_state_info.module_state_str);
        aim_printf (&uc->pvs,
                    "%-30s = %d\n",
                    "ForceLowPwr bit",
                    module_state_info.forceLowPwr);
        if (spec_major_rev >= 4) {
            aim_printf (
                &uc->pvs, "%-30s = %d\n", "LowPwr bit",
                module_state_info.lowPwr);
        }
        if (bf_port_qsfp_mgmnt_temper_high_alarm_flag_get (
                port)) {
            aim_printf (&uc->pvs,
                        "** LowPWr due to high temp     = %f C\n",
                        bf_port_qsfp_mgmnt_temper_high_record_get (port));
        }
        aim_printf (&uc->pvs, "\n");

        qsfp_datapath_state_info_t dp_st_info[num_ch];
        qsfp_datapath_staged_set_info_t
        cfg_active_set[num_ch];
        qsfp_datapath_staged_set_info_t
        cfg_staged_set0[num_ch];
        // qsfp_datapath_staged_set_info_t staged_set1[num_ch];

        aim_printf (&uc->pvs,
                    "## DATA PATH STATE MACHINE\n");
        bf_qsfp_get_dp_state_info (port, num_ch,
                                   dp_st_info);
        aim_printf (&uc->pvs, "%-25s", " ");
        for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
            sprintf (buf, "LN%d", cur_ch + 1);
            aim_printf (&uc->pvs, "%-5s", buf);
        }
        aim_printf (&uc->pvs, "\n");
        aim_printf (&uc->pvs, "%-25s", "Data path state");
        for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
            aim_printf (&uc->pvs, "%-5s",
                        dp_st_info[cur_ch].datapath_state_str_short);
        }
        aim_printf (&uc->pvs, "\n");
        if (spec_major_rev >= 4) {
            aim_printf (&uc->pvs, "%-25s",
                        "DataPathDeinit bits");
        } else {
            aim_printf (&uc->pvs, "%-25s",
                        "DataPathPwrUp bits");
        }
        for (cur_ch = 0; cur_ch < num_ch; cur_ch++) {
            aim_printf (&uc->pvs, "%-5d",
                        dp_st_info[cur_ch].data_path_deinit_bit);
        }
        aim_printf (&uc->pvs, "\n\n");

        // Active set Data Path info
        bf_qsfp_get_dp_state_config (port, num_ch,
                                     cfg_active_set, ACTIVE_SET);
        bf_pltfm_ucli_ucli__qsfp_mem_show_dp (
            uc, app_info, cfg_active_set, ACTIVE_SET, num_ch);
        aim_printf (&uc->pvs, "\n");

        // Staged set 0 Data Path info
        bf_qsfp_get_dp_state_config (port, num_ch,
                                     cfg_staged_set0, STAGED_SET0);
        bf_pltfm_ucli_ucli__qsfp_mem_show_dp (
            uc, app_info, cfg_staged_set0, STAGED_SET0,
            num_ch);
        aim_printf (&uc->pvs, "\n");
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_mem_show (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "module-show", 1,
        "<port OR -1 for all ports>");

    int max_port = bf_qsfp_get_max_qsfp_ports();
    int startport, curport;

    curport = atoi (uc->pargs->args[0]);
    if (curport > max_port) {
        aim_printf (&uc->pvs,
                    "port must be 1-%d or -1 for all\n", max_port);
        return -1;
    }

    if (curport > 0) {
        startport = max_port = curport;
    } else {
        startport = 1;
        max_port = bf_qsfp_get_max_qsfp_ports();
    }
    for (curport = startport; curport <= max_port;
         curport++) {
        if (bf_qsfp_is_present (curport)) {
            bf_pltfm_ucli_ucli__qsfp_module_show (uc,
                                                  curport);
            if (curport != max_port) {
                aim_printf (&uc->pvs, "\n");
            }
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_detect_xver (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "detect-xver", -1,
        "[port]");

    bool is_present;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        //if (uc->pargs->count > 1) {
        //    last_port = atoi (uc->pargs->args[1]);
        //}
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    for (port = first_port; port <= last_port;
         port ++) {
        if (bf_qsfp_detect_transceiver (port,
                                        &is_present) == 0) {
            aim_printf (
                &uc->pvs, "port %2d %s\n", port,
                (is_present ? "detected" : "removed"));
        } else {
            aim_printf (&uc->pvs, "error detecting port %2d\n",
                        port);
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_type_show (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-type", -1,
        "[sport] [dport]");

    bf_pltfm_qsfp_type_t qsfp_type = BF_PLTFM_QSFP_UNKNOWN;
    char display_qsfp_type[13];
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    for (port = first_port; port <= last_port;
         port++) {
        if (bf_qsfp_type_get (port, &qsfp_type) != 0) {
            qsfp_type = BF_PLTFM_QSFP_UNKNOWN;
        }
        qsfp_type_to_display_get (
            qsfp_type, display_qsfp_type,
            sizeof (display_qsfp_type));
        aim_printf (&uc->pvs, "port %d %s\n", port,
                    display_qsfp_type);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_read_reg (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "read-reg", 3,
        "<port> <page> <offset>");

    int port, page, offset;
    uint8_t val;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int err;

    port = atoi (uc->pargs->args[0]);
    page = atoi (uc->pargs->args[1]);
    offset = strtol (uc->pargs->args[2], NULL, 0);

    aim_printf (
        &uc->pvs,
        "read-reg <port=%d> <page=%d> <offset=0x%x>\n",
        port,
        page,
        offset);

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    if (!bf_qsfp_is_present (port)) {
        aim_printf (&uc->pvs, "Module not present %d\n",
                    port);
        return 0;
    }

    err = bf_pltfm_qsfp_read_reg (port, page, offset,
                                  &val);
    if (err) {
        aim_printf (&uc->pvs,
                    "error(%d) reading register offset=0x%02x\n", err,
                    offset);
    } else {
        aim_printf (&uc->pvs,
                    "page %d offset 0x%x read 0x%02x\n", page, offset,
                    val);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_write_reg (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "write-reg", 4,
        "<port> <page> <offset> <val>");

    int port, page, offset;
    uint8_t val;
    int err;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    port = atoi (uc->pargs->args[0]);
    page = atoi (uc->pargs->args[1]);
    offset = strtol (uc->pargs->args[2], NULL, 0);
    val = strtol (uc->pargs->args[3], NULL, 0);

    aim_printf (
        &uc->pvs,
        "write-reg <port=%d> <page=%d> <offset=0x%x> "
        "<val=0x%x>\n",
        port,
        page,
        offset,
        val);

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    if (!bf_qsfp_is_present (port)) {
        aim_printf (&uc->pvs, "Module not present %d\n",
                    port);
        return 0;
    }

    err = bf_pltfm_qsfp_write_reg (port, page, offset,
                                   val);
    if (err) {
        aim_printf (&uc->pvs,
                    "error(%d) writing register offset=0x%02x\n", err,
                    offset);
    } else {
        aim_printf (
            &uc->pvs, "page %d offset 0x%x written 0x%0x\n",
            page, offset, val);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_reset (ucli_context_t
                                *uc)
{
    UCLI_COMMAND_INFO (
        uc, "qsfp-reset", 2,
        "<port OR -1 for all ports> "
        "<1: reset, 0: unreset>");

    int reset_val;
    bool reset;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    reset_val = atoi (uc->pargs->args[1]);
    reset = (reset_val == 0) ? false : true;

    for (port = first_port; port <= last_port;
         port++) {
        bf_qsfp_reset (port, reset);
    }
    return 0;
}

#if 0
static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_lpmode_hw (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "qsfp-lpmode-hw", 2,
        "qsfp-lpmode-hw <port> <1: lpmode 0 : no lpmode>");
    int port;
    int port_begin, max_port;
    int lpmode_val;
    bool lpmode;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        port_begin = port;
        max_port = port;
    } else {
        port_begin = 1;
        max_port = bf_qsfp_get_max_qsfp_ports();
    }
    lpmode_val = atoi (uc->pargs->args[1]);
    lpmode = (lpmode_val == 0) ? false : true;

    for (port = port_begin; port <= max_port;
         port++) {
        bf_qsfp_set_transceiver_lpmode (port, lpmode);
    }
    return 0;
}
#endif
static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_lpmode_sw (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-lpmode-sw", 2,
        "<port OR -1 for all ports> "
        "<1: lpmode, 0: no lpmode>");

    int lpmode_val;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    lpmode_val = atoi (uc->pargs->args[1]);

    for (port = first_port; port <= last_port;
         port++) {
        qsfp_lpmode_sw_set (0, port, lpmode_val);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_lpbk (ucli_context_t
                               *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-lpbk", 2,
        "<port> [near/elec far/opt]");

    int port;
    bool lpbk_near = true;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    port = atoi (uc->pargs->args[0]);
    lpbk_near = (uc->pargs->args[1][0] == 'n')
                ? true
                : (uc->pargs->args[1][0] == 'e') ? true : false;
    if (port > max_port) {
        return 0;
    }
    qsfp_luxtera_lpbk (port, lpbk_near);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_capture (ucli_context_t
                                  *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-capture", 1,
        "<port>");

    int port, ofs;
    uint8_t arr_0x3k[0x3000];
    char printable_strs[17] = {0};

    port = atoi (uc->pargs->args[0]);
    bf_pm_qsfp_luxtera_state_capture (port, arr_0x3k);

    for (ofs = 0; ofs < 12288; ofs++) {
        if ((ofs % 16) == 0) {
            printable_strs[16] = 0;
            aim_printf (&uc->pvs, " %s\n%6d : ",
                        printable_strs, ofs);
        }
        aim_printf (&uc->pvs, "%02x ", arr_0x3k[ofs]);
        printable_strs[ (ofs % 16)] = isprint (
                                          arr_0x3k[ofs]) ? arr_0x3k[ofs] : '.';
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_tmr_stop (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-tmr-stop", 0,
        "stop qsfp scan timer");

    bf_pltfm_pm_qsfp_scan_poll_stop();
    bf_sys_sleep (
        1); /* wait for on going qsfp scan to be over */

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_tmr_start (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-tmr-start", 0,
        "start qsfp scan timer");

    bf_pltfm_pm_qsfp_scan_poll_start();
    return 0;
}

static int bf_qsfp_ucli_qsfp_type_get (
    const char *str1,
    bf_pltfm_qsfp_type_t *qsfp_type)
{
    int i = 0;
    char str[25];

    if ((!qsfp_type) || (!str1)) {
        return -1;
    }

    memset (str, '\0', sizeof (str));

    while ((str1[i]) && i < 25) {
        str[i] = tolower (str1[i]);
        i++;
    }

    if (strcmp (str, "copper-0.5m") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_0_5_M;
    } else if (strcmp (str, "copper-1m") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_1_M;
    } else if (strcmp (str, "copper-2m") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_2_M;
    } else if (strcmp (str, "copper-3m") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_3_M;
    } else if (strcmp (str, "copper-loopback") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
    } else if (strcmp (str, "optical") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_OPT;
    } else {
        return -1;
    }

    return 0;
}
static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_ctrlmask_set (
        ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "ctrlmask-set", -1,
        "[port OR -1 for all ports] "
        "[ctrlmask, hex value]. Dump all ctrlmask if no input arguments.");

    uint32_t ctrlmask = 0, ctrlmasks[65] = {0};
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        if (port > 0) {
            first_port = last_port = port;
        } else {
            // 1 - max_port;
        }
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            ctrlmask = strtol (uc->pargs->args[1], NULL, 16);;
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    for (port = first_port; port <= last_port;
         port++) {
        bf_qsfp_ctrlmask_get (port, &ctrlmasks[port]);
    }

    if (uc->pargs->count <= 1) goto dump;

    for (port = first_port; port <= last_port;
         port++) {
        ctrlmasks[port] = ctrlmask;
        bf_qsfp_ctrlmask_set (port, ctrlmasks[port]);
    }

dump:
    for (port = first_port; port <= last_port;
         port++) {
        bf_qsfp_ctrlmask_get (port, &ctrlmasks[port]);
        aim_printf (&uc->pvs, "\n\nPort : %2d, 0x%-8X\n\n",
            port, ctrlmasks[port]);
        for (int bit = 0; bit < 32; bit ++) {
            if (bf_qsfp_ctrlmask_str[bit][0] != ' ') {
                aim_printf (&uc->pvs, "%35s (bit=%2d) -> %s\n",
                    bf_qsfp_ctrlmask_str[bit],
                    bit,
                    ((ctrlmasks[port] >> bit) & 1) ? "enabled" : "disabled");
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_special_case_set (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-type-set", 2,
        "<port OR -1 for all ports> "
        "<copper-0.5, copper-1m, copper-2m, copper-3m, "
        "copper-loopback, optical>");

    bf_pltfm_qsfp_type_t qsfp_type;
    int ret = 0;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    ret = bf_qsfp_ucli_qsfp_type_get (
              uc->pargs->args[1], &qsfp_type);
    if (ret != 0) {
        return 0;
    };

    for (port = first_port; port <= last_port;
         port++) {
        // expand if more info needs to set
        bf_qsfp_special_case_set (port, qsfp_type, true);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_special_case_clear (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-type-clear", 1,
        "<port OR -1 for all ports>");

    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    for (port = first_port; port <= last_port;
         port++) {
        bf_qsfp_special_case_set (port,
                                  BF_PLTFM_QSFP_UNKNOWN, false);
    }
    return 0;
}


static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_soft_remove (
    ucli_context_t *uc)
{

    UCLI_COMMAND_INFO (uc,
        "soft-remove", 2,
        "<port OR -1 for all ports> "
        "<set/clear>");

    bool remove_flag = 0;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    if (strcmp (uc->pargs->args[1], "set") == 0) {
        remove_flag = true;
    } else if (strcmp (uc->pargs->args[1],
                       "clear") == 0) {
        remove_flag = false;
    } else {
        aim_printf (&uc->pvs,
                    "Usage: front-panel port-num <1 to %d or -1 for all ports> <set "
                    "or clear>\n",
                    max_port);
        return 0;
    }

    for (port = first_port; port <= last_port;
         port++) {
        if (bf_port_qsfp_mgmnt_temper_high_alarm_flag_get (
                port) &&
            bf_qsfp_is_present (port)) {
            bf_qsfp_set_pwr_ctrl (port, 0);
        }
        bf_qsfp_soft_removal_set (port, remove_flag);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_soft_remove_show (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "soft-remove-show", 1,
        "<port OR -1 for all ports>");

    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    for (port = first_port; port <= last_port;
         port++) {
        aim_printf (&uc->pvs,
                    "port %d soft-removed %s\n",
                    port,
                    bf_qsfp_soft_removal_get (port) ? "Yes" : "No");
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_write_mul (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-write", -1,
        "<port> <page> <num-bytes> <start-offset> <data1 data2 ...>");

    int port, offset, nBytes;
    uint8_t page, *val;
    int i;
    int arg_err = 0;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    if (uc->pargs->count < 5) {
        aim_printf (&uc->pvs,
                    "Minimum 4 args required to write the data \n");
        return 0;
    }

    port = atoi (uc->pargs->args[0]);
    page = atoi (uc->pargs->args[1]);
    nBytes = atoi (uc->pargs->args[2]);
    offset = strtol (uc->pargs->args[3], NULL, 0);

    if (nBytes > uc->pargs->count) {
        aim_printf (&uc->pvs,
                    "Seems args are wrong !! number of bytes exceed num of args\n");
    }
    if (!nBytes) {
        aim_printf (&uc->pvs,
                    "number of bytes cannot be zero\n");
        arg_err = 1;
    }

    int n = uc->pargs->count - 4;
    if (n != nBytes) {
        aim_printf (&uc->pvs,
                    "Looks like number of arguments are wrong\n");
        arg_err = 1;
    }

    if (arg_err) {
        return 0;
    }

    val = (uint8_t *)bf_sys_calloc (nBytes,
                                    sizeof (uint8_t));

    if (!val) {
        return 0;
    }

    for (i = 0; i < nBytes; i++) {
        val[i] = strtol (uc->pargs->args[4 + i], NULL, 0);
        aim_printf (&uc->pvs, "val[%d]:%d\n", i, val[i]);
    }

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        goto q_free;
    }

    if (!bf_qsfp_is_present (port)) {
        aim_printf (&uc->pvs, "Module not present %d\n",
                    port);
        return 0;
    }

    if (bf_qsfp_module_write (port, QSFP_BANK0, page,
                              offset, nBytes, val)) {
        aim_printf (&uc->pvs,
                    "error writing the module %d\n", port);
    } else {
        aim_printf (&uc->pvs,
                    "page %d offset 0x%x written nbytes:0x%0x\n",
                    page,
                    offset,
                    nBytes);
    }

q_free:
    if (val) {
        bf_sys_free (val);
    }
    return 0;
}

static ucli_status_t qsfp_read (ucli_context_t
                                *uc, bool is_cached_read)
{
    int port, offset, nBytes, page;
    uint8_t *val;
    int i;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    if (uc->pargs->count < 4) {
        aim_printf (&uc->pvs,
                    "Minimum 4 args required to read the data\n");
        return 0;
    }

    port = atoi (uc->pargs->args[0]);
    page = atoi (uc->pargs->args[1]);
    nBytes = atoi (uc->pargs->args[2]);
    offset = strtol (uc->pargs->args[3], NULL, 0);

    val = (uint8_t *)bf_sys_calloc (nBytes,
                                    sizeof (uint8_t));

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        goto q_free;
    }

    if (nBytes > MAX_QSFP_PAGE_SIZE_255) {
        aim_printf (&uc->pvs,
                    "Number of bytes must be less than %d\n",
                    MAX_QSFP_PAGE_SIZE_255);
        goto q_free;
    }

    if (!bf_qsfp_is_present (port)) {
        aim_printf (&uc->pvs, "Module not present %d\n",
                    port);
        return 0;
    }

    bf_pltfm_status_t sts;
    if (is_cached_read) {
        sts =
            bf_qsfp_module_cached_read (port, QSFP_BANK0,
                                        page, offset, nBytes, val);
    } else {
        sts = bf_qsfp_module_read (port, QSFP_BANK0, page,
                                   offset, nBytes, val);
    }
    if (sts != BF_SUCCESS) {
        aim_printf (&uc->pvs,
                    "error reading the module %d\n", port);
        goto q_free;
    }

    aim_printf (&uc->pvs,
                "QSFP:%d page %d offset 0x%x read nbytes:%d\n",
                port,
                page,
                offset,
                nBytes);

    uint8_t as_chars[17] = {0};
    for (int j = 0; j < 17; j++) {
        as_chars[j] = '.';
    }
    for (i = 0; i < nBytes; i++) {
        if ((offset + i) % 16 == 0) {
            aim_printf (&uc->pvs,
                        "\nPage %3d : offset %3d : ", page, offset + i);
        } else if (i == 0) {
            aim_printf (&uc->pvs,
                        "\nPage %3d : offset %3d : ", page, offset + i);
            for (int k = 0; k < (offset % 16); k++) {
                aim_printf (&uc->pvs, "   ");
                as_chars[k] = '.';
            }
        }
        aim_printf (&uc->pvs, "%02x ", val[i]);
        if (isprint (val[i])) {
            as_chars[i % 16] = val[i];
        } else {
            as_chars[i % 16] = '.';
        }
        if ((offset + i) % 16 == 15) {
            as_chars[16] = 0;  // terminate string
            aim_printf (&uc->pvs, " : %s", as_chars);
            for (int j = 0; j < 17; j++) {
                as_chars[j] = 0;
            }
        }
    }
    if ((offset + i) % 16 != 0) {
        for (int k = 0; k < (16 - ((offset + i) % 16));
             k++) {
            aim_printf (&uc->pvs, "   ");
            if (k < 17) {
                as_chars[k] = '.';
            }
        }
    }
    as_chars[16] = 0;  // terminate string
    aim_printf (&uc->pvs, " : %s", as_chars);

    aim_printf (&uc->pvs, "\n");

q_free:
    if (val) {
        bf_sys_free (val);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_read_mul (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-read", -1,
        "<port> <page> <num-bytes> <start-offset>");

    return qsfp_read (uc, false);
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_combined_read (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-combined-read", -1,
        "<port> <page> <num-bytes> <start-offset>");

    return qsfp_read (uc, true);
}

static ucli_status_t
bf_pltfm_ucli_ucli__mark_qsfp_port_as_internal (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "mark-port-internal", 2,
        "<port OR -1 for all ports> <set/clear>");


    bool set = 0;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    if (strcmp (uc->pargs->args[1], "set") == 0) {
        set = true;
    } else if (strcmp (uc->pargs->args[1],
                       "clear") == 0) {
        set = false;
    } else {
        aim_printf (&uc->pvs,
                    "Usage: front-panel port-num <1 to %d or -1 for all ports> <set "
                    "or clear>\n",
                    max_port);
        return 0;
    }

    for (port = first_port; port <= last_port;
         port++) {
        bf_qsfp_internal_port_set (port, set);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_db (ucli_context_t
                             *uc)
{
    UCLI_COMMAND_INFO (uc,
        "db", 0,
        "display suported QSFP database.");

    sff_info_t *sff;
    sff_eeprom_t *se;
    sff_db_entry_t *entry;
    int num = 0;
    int i;

    sff_db_get (&entry, &num);
    for (i = 0; i < num; i ++) {
        se = &entry[i].se;
        sff = &se->info;
        if (sff->sfp_type == SFF_SFP_TYPE_QSFP ||
            sff->sfp_type == SFF_SFP_TYPE_QSFP_PLUS ||
            sff->sfp_type == SFF_SFP_TYPE_QSFP28) {
            sff_info_show (sff, uc);
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_map (ucli_context_t
                              *uc)
{
    UCLI_COMMAND_INFO (uc,
        "map", 0,
        "display QSFP map.");

    int module, err;
    uint32_t conn_id, chnl_id = 0;
    char alias[16] = {0}, connc[16] = {0}, prefix = ' ';

    /* Dump the map of Module <-> Alias <-> QSFP/CH <-> Present. */
    aim_printf (&uc->pvs, "%12s%12s%20s%12s\n",
                "MODULE", "ALIAS", "PORT", "TX_DISABLED");

    if (platform_type_equal(AFN_X732QT)) {
        prefix = 'Q';
    };

    /* QSFP */
    for (int i = 0; i < bf_qsfp_get_max_qsfp_ports();
         i ++) {
        module = (i + 1);
        err = bf_pltfm_qsfp_lookup_by_module (module,
                                              &conn_id);
        if (!err) {
            sprintf (alias, "%cC%d", prefix,
                     module % (BF_PLAT_MAX_QSFP * 4));
            sprintf (connc, "%2d/%d",
                     (conn_id % BF_PLAT_MAX_QSFP),
                     (chnl_id % MAX_CHAN_PER_CONNECTOR));
            aim_printf (&uc->pvs, "%12d%12s%20s%12s\n",
                        module, alias, connc,
                        /* Only check lane 0. */
                        bf_qsfp_is_present (module) ? bf_qsfp_tx_is_disabled(module, 0) ? "true" : "false" : "----");
        }
    }

    /* vQSFP, always true. */
    aim_printf (&uc->pvs, "%12s\n",
                "===vQSFP===");
    for (int i = 0;
         i < bf_pltfm_get_max_vqsfp_ports(); i ++) {
        err  = bf_pltfm_vqsfp_lookup_by_index (i, alias,
                                               &conn_id);
        if (!err) {
            sprintf (connc, "%2d/%d",
                     (conn_id % BF_PLAT_MAX_QSFP),
                     (chnl_id % MAX_CHAN_PER_CONNECTOR));
            aim_printf (&uc->pvs, "%12d%12s%20s%12s\n",
                        bf_qsfp_get_max_qsfp_ports() + 1 + i, alias,
                        connc, "false");
        }
    }

    return 0;
}

extern char *bf_pm_intf_fsm_st_get (int conn_id,
                               int ch);
static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_fsm (ucli_context_t *uc)
{

    UCLI_COMMAND_INFO (uc,
        "fsm", -1,
        "[sport] [dport]");

    int i;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    aim_printf (&uc->pvs, "%s | ", "Port");
    aim_printf (&uc->pvs, "%-20s | ", "Module FSM");
    aim_printf (&uc->pvs, "%-19s | ", "CH0");
    aim_printf (&uc->pvs, "%-19s | ", "CH1");
    aim_printf (&uc->pvs, "%-19s | ", "CH2");
    aim_printf (&uc->pvs, "%-19s | ", "CH3");
    aim_printf (&uc->pvs, "%-19s | ", "CH4");
    aim_printf (&uc->pvs, "%-19s | ", "CH5");
    aim_printf (&uc->pvs, "%-19s | ", "CH6");
    aim_printf (&uc->pvs, "%-19s | \n", "CH7");

    for (port = first_port; port <= last_port;
         port++) {
        // FSM for both host and media side with tof1 implementation.
        // FSM for media side only with tof2 implementation.
        aim_printf (&uc->pvs, "%-4d | %s | ", port,
                    qsfp_module_fsm_st_get (port));
        for (i = 0; i < MAX_CHAN_PER_CONNECTOR; i++) {
            if (i >= bf_qsfp_get_ch_cnt (port)) {
                aim_printf (&uc->pvs, "%-19s | ", "NA");
            } else {
                aim_printf (&uc->pvs, "%-19s | ",
                            qsfp_channel_fsm_st_get (port, i));
            }
        }
        aim_printf (&uc->pvs, "\n");
        // FSM for host side only with tof2 implementation.
        if (platform_type_equal (AFN_X732QT)) {
            aim_printf (&uc->pvs, "%-4s | %s | ", " ",
                        "                   ");
            for (i = 0; i < MAX_CHAN_PER_CONNECTOR; i++) {
                 if (i >= bf_qsfp_get_ch_cnt (port)) {
                     aim_printf (&uc->pvs, "%-19s | ", "NA");
                 } else {
                     aim_printf (&uc->pvs, "%-19s | ",
                         bf_pm_intf_fsm_st_get (port, i));
                 }
             }
            aim_printf (&uc->pvs, "\n");
        }
    }
    return 0;
}
#if 0
static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_fsm_set (ucli_context_t *uc)
{

    UCLI_COMMAND_INFO (uc,
        "fsm-set", 2,
        "<port OR -1 for all ports> <state>");

    uint8_t st = 0;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    st = atoi (uc->pargs->args[1]);

    for (port = first_port; port <= max_port;
         port++) {
        qsfp_module_fsm_st_set (port, st);
    }
    aim_printf (&uc->pvs, "\n");
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_fsm_ch_set (ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "fsm-ch-set", 4,
        "<port OR -1 for all ports> "
        "<ch OR -1 for all channels> "
        "<state> "
        "<stop>");

    uint8_t st = 0, stop = 0;
    int max_ch = 4;
    int ch, first_ch = 0, last_ch = (max_ch - 1);
    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    if (platform_type_equal(AFN_X732QT)) {
        last_ch = max_ch = 8;
    }

    ch = atoi (uc->pargs->args[1]);
    if (ch >= 0) {
        first_ch = last_ch = ch;
    }
    BF_UCLI_CH_VALID(ch, first_ch, last_ch, max_ch, "ch");

    /* qsfp_fsm_ch_en_state_t */
    st = atoi (uc->pargs->args[2]);
    stop = atoi (uc->pargs->args[3]);

    for (port = first_port; port <= max_port;
         port ++) {
        for (int c = first_ch; c <= last_ch; c ++) {
            qsfp_channel_fsm_st_set (port, ch, st, stop);
        }
    }
    aim_printf (&uc->pvs, "\n");
    return 0;
}
#endif
static ucli_status_t bf_pltfm_ucli_ucli__qsfp_rxlos_debounce_set(
    ucli_context_t *uc) {

    UCLI_COMMAND_INFO(uc,
        "qsfp-rxlos-debounce-set", 2,
        "<port> <count>");

    int port, count, max_port;

    max_port = bf_qsfp_get_max_qsfp_ports();
    port = atoi(uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf(&uc->pvs, "port must be 1-%d\n", max_port);
        return 0;
    }

    count = atoi(uc->pargs->args[1]);
    if (count < 0) {
        aim_printf(&uc->pvs, "count must be 0 or more\n");
        return 0;
    }
    aim_printf(&uc->pvs, "RxLOS debounce count %d -> %d\n",
        bf_qsfp_rxlos_debounce_get(port),
        count);
    bf_qsfp_rxlos_debounce_set(port, count);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_mgmnt_temper_monit_period_set (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-temper-period-set", 1,
        "<poll period in sec. min:1> Default 5-sec");

    int64_t period_secs;

    if (uc->pargs->count == 1) {
        period_secs = strtol (uc->pargs->args[0], NULL,
                              10);
        if ((period_secs != LONG_MIN) &&
            (period_secs != LONG_MAX) &&
            (period_secs > 0)) {
            bf_port_qsfp_mgmnt_temper_monitor_period_set (
                period_secs);
            return 0;
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_wavelength_get (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-wavelength-get", 1, "<port>");

    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    port = atoi (uc->pargs->args[0]);

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    if (!bf_qsfp_is_present (port)) {
        aim_printf (&uc->pvs, "Module not present on port %d\n",
                    port);
        return 0;
    }

    bool is_cmis = bf_qsfp_is_cmis (port);
    bool is_passive_cu = bf_qsfp_is_passive_cu (port);
    aim_printf (&uc->pvs,
                "====================\n");
    aim_printf (&uc->pvs,
                "Info for Wavelength:\n");
    aim_printf (&uc->pvs,
                "====================\n");

    double nominal, tolerance;
    if ((!is_passive_cu) &&
        (bf_qsfp_get_wavelength_info (port,
                                      &nominal,
                                      &tolerance))) {
        aim_printf (&uc->pvs,
                    "%-21s = %.2f nm\n",
                    "Nominal wavelength",
                    nominal);
        aim_printf (&uc->pvs,
                    "%-21s = ±%.3f nm\n",
                    "Wavelength tolerance",
                    tolerance);
        aim_printf (&uc->pvs, "\n");
    }

    bool ctrl_flag, tune_flag;
    if ((is_cmis) && (!is_passive_cu) &&
        bf_qsfp_get_wavelength_flags (port,
                                      &ctrl_flag,
                                      &tune_flag)) {
        aim_printf (&uc->pvs,
                    "%-21s = %s\n",
                    "Wavelength control",
                    (ctrl_flag ? "Active control" : "Not supported"));
        aim_printf (&uc->pvs,
                    "%-21s = %s\n",
                    "Wavelength tunable",
                    (tune_flag ? "True" : "False"));
    }

    if (tune_flag) {
        qsfp_laser_info_t laser_info;
        bf_cmis_get_laser_info (port, &laser_info);

        aim_printf (&uc->pvs,
                    "%-21s = %s\n",
                    "Wavelength tuning",
                    (laser_info.tuning_set ? "Enabled" : "Disabled"));

        aim_printf (&uc->pvs, "\n");
        aim_printf (&uc->pvs,
                    "Wavelength DDM:\n");
        aim_printf (&uc->pvs,
                    "%10s %17s\n",
                    "-------",
                    "---------------");
        aim_printf (&uc->pvs,
                    "%10s %17s\n",
                    "Port/Ch",
                    "Wavelength (nm)");
        aim_printf (&uc->pvs,
                    "%10s %17s\n",
                    "-------",
                    "---------------");

        uint8_t max_ch = bf_qsfp_get_media_ch_cnt (port);
        double wavelength_nm;
        for (int ch = 0; ch < max_ch; ch++) {
            bf_cmis_module_wavelength_get (port,
                                           ch,
                                           &wavelength_nm);
            aim_printf (&uc->pvs,
                        "      %d/%d %17.2f\n",
                        port, ch, wavelength_nm);
        }
        aim_printf (&uc->pvs, "\n");
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_wavelength_set (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-wavelength-set", 2,
        "<port> <wavelength(nm) in range 1528.77-1567.13>");

    int port;
    bool ctrl_flag, tune_flag;
    double wavelength;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    port = atoi (uc->pargs->args[0]);
    wavelength = atof (uc->pargs->args[1]);

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    if (!bf_qsfp_is_present (port)) {
        aim_printf (&uc->pvs, "Module not present on port %d\n",
                    port);
        return 0;
    }

    if (!bf_qsfp_is_cmis (port)) {
        aim_printf (&uc->pvs, "Wavelength setting unsupported for module on port %d\n",
                    port);
        return 0;
    }

    bf_qsfp_get_wavelength_flags (port, &ctrl_flag, &tune_flag);
    if (!tune_flag) {
        aim_printf (&uc->pvs, "Wavelength setting unsupported for module on port %d\n",
                    port);
        return 0;
    }

    if (bf_cmis_module_wavelength_set(port, wavelength)) {
        aim_printf (&uc->pvs, "Wavelength setting failed for module on port %d\n",
                    port);
    } else {
        aim_printf (&uc->pvs,
                    "Wavelength set to %.2f nm (%.3f GHz) for module on port %d, "
                    "re-enable the port to take effect\n",
                    wavelength, LIGHT_SPEED / wavelength, port);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_wavelength_clear (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-wavelength-clear", 1, "<port>");

    int port = atoi (uc->pargs->args[0]);
    int max_port = bf_qsfp_get_max_qsfp_ports();

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    bf_cmis_module_wavelength_clear (port);
    aim_printf (&uc->pvs,
                "Wavelength setting cleared for port %d, "
                "re-enable the port to take effect\n",
                port);

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_mgmnt_temper_monit_log_enable (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-realtime-ddm-log", 1,
        "<1: Enable, 0: Disable>");

    bool enable;

    enable = atoi (uc->pargs->args[0]);
    if (uc->pargs->count == 1) {
        if (enable) {
            bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_QSFP_REALTIME_DDM_LOG;
        } else {
            bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_MNTR_QSFP_REALTIME_DDM_LOG;
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_vdm (ucli_context_t
                                  *uc)
{
    UCLI_COMMAND_INFO (uc,
        "get-vdm", -1,
        "[sport] [dport]");

    int max_port = bf_qsfp_get_max_qsfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    /*
    Assuming module on port 1, 2 supports VDM whilst the one on port 3 does not.
    Sample output:

    bf-sde> get-vdm 1 3
    CMIS VDM Info for module on port 1
       -------  --------------  ----------------  ---------------  ---------------  --------------  ---------------  --------------  --------------  -------------
       Port/Ch           Value  High Alarm Thres  Low Alarm Thres  High Warn Thres  Low Warn Thres  High Alarm Trig  Low Alarm Trig  High Warn Trig  Low Warn Trig
       -------  --------------  ----------------  ---------------  ---------------  --------------  ---------------  --------------  --------------  -------------
       eSNR Media Input [dB]
           1/1            7.57           255.996          255.996          255.996         255.996            false           false           false          false
           1/2            7.14           255.996          255.996          255.996         255.996            false           false           false          false
           1/3            7.51           255.996          255.996          255.996         255.996            false           false           false          false
           1/4            6.56           255.996          255.996          255.996         255.996            false           false           false          false
       -------  --------------  ----------------  ---------------  ---------------  --------------  ---------------  --------------  --------------  -------------
       eSNR Host Input [dB]
           1/1               0           255.996          255.996          255.996         255.996            false           false           false          false
           1/2               0           255.996          255.996          255.996         255.996            false           false           false          false
           1/3               0           255.996          255.996          255.996         255.996            false           false           false          false
           1/4               0           255.996          255.996          255.996         255.996            false           false           false          false
           1/5               0           255.996          255.996          255.996         255.996            false           false           false          false
           1/6               0           255.996          255.996          255.996         255.996            false           false           false          false
           1/7               0           255.996          255.996          255.996         255.996            false           false           false          false
           1/8               0           255.996          255.996          255.996         255.996            false           false           false          false
       ...

    CMIS VDM Info for module on port 2
       -------  --------------  ----------------  ---------------  ---------------  --------------  ---------------  --------------  --------------  -------------
       Port/Ch           Value  High Alarm Thres  Low Alarm Thres  High Warn Thres  Low Warn Thres  High Alarm Trig  Low Alarm Trig  High Warn Trig  Low Warn Trig
       -------  --------------  ----------------  ---------------  ---------------  --------------  ---------------  --------------  --------------  -------------
       ...

    */
    for (port = first_port; port <= last_port; port++) {
        if (!bf_qsfp_is_present (port) ||
            (!bf_qsfp_is_optical (port))) {
            continue;
        }

        if (!bf_qsfp_is_cmis (port)) {
            continue;
        }

        printVdm_for_cmis(uc, port);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_mgmnt_temper_monitor_enable (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "qsfp-realtime-ddm-monit-enable", 1,
        "<1: Enable, 0: Disable>");

    bool enable;

    enable = atoi (uc->pargs->args[0]);
    if (uc->pargs->count == 1) {
        if (enable) {
            bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_QSFP_REALTIME_DDM;
        } else {
            bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_MNTR_QSFP_REALTIME_DDM;
        }
    }

    return 0;
}

/* <auto.ucli.handlers.start> */
static ucli_command_handler_f
bf_pltfm_qsfp_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__qsfp_mem_show,
    bf_pltfm_ucli_ucli__qsfp_detect_xver,
    // bf_pltfm_ucli_ucli__qsfp_get_xver_info,
    bf_pltfm_ucli_ucli__qsfp_dump_info,
    bf_pltfm_ucli_ucli__qsfp_show,
    bf_pltfm_ucli_ucli__qsfp_init,
    bf_pltfm_ucli_ucli__qsfp_get_int,
    bf_pltfm_ucli_ucli__qsfp_get_lpmode,
    bf_pltfm_ucli_ucli__qsfp_set_lpmode,
    bf_pltfm_ucli_ucli__qsfp_get_pres,
    bf_pltfm_ucli_ucli__qsfp_get_ddm,
    bf_pltfm_ucli_ucli__qsfp_type_show,
    bf_pltfm_ucli_ucli__qsfp_tx_disable_set,
    bf_pltfm_ucli_ucli__qsfp_cdr_disable_set,
    bf_pltfm_ucli_ucli__qsfp_read_reg,
    bf_pltfm_ucli_ucli__qsfp_write_reg,
    bf_pltfm_ucli_ucli__qsfp_read_mul,
    bf_pltfm_ucli_ucli__qsfp_combined_read,
    bf_pltfm_ucli_ucli__qsfp_write_mul,
    bf_pltfm_ucli_ucli__qsfp_pg0,
    bf_pltfm_ucli_ucli__qsfp_pg3,
    bf_pltfm_ucli_ucli__qsfp_oper,
    bf_pltfm_ucli_ucli__qsfp_summary,
    bf_pltfm_ucli_ucli__qsfp_reset,
    //bf_pltfm_ucli_ucli__qsfp_lpmode_hw,
    bf_pltfm_ucli_ucli__qsfp_lpmode_sw,
    bf_pltfm_ucli_ucli__qsfp_sw_get_lpmode,
    bf_pltfm_ucli_ucli__qsfp_lpbk,
    bf_pltfm_ucli_ucli__qsfp_capture,
    bf_pltfm_ucli_ucli__qsfp_tmr_stop,
    bf_pltfm_ucli_ucli__qsfp_tmr_start,
    bf_pltfm_ucli_ucli__qsfp_special_case_set,
    bf_pltfm_ucli_ucli__qsfp_special_case_clear,
    bf_pltfm_ucli_ucli__qsfp_ctrlmask_set,
    bf_pltfm_ucli_ucli__qsfp_soft_remove,
    bf_pltfm_ucli_ucli__qsfp_soft_remove_show,
    bf_pltfm_ucli_ucli__mark_qsfp_port_as_internal,
    bf_pltfm_ucli_ucli__qsfp_fsm,
    //bf_pltfm_ucli_ucli__qsfp_fsm_set,
    //bf_pltfm_ucli_ucli__qsfp_fsm_ch_set,
    bf_pltfm_ucli_ucli__qsfp_dump_idprom_cache,
    bf_pltfm_ucli_ucli__qsfp_mgmnt_temper_monit_period_set,
    bf_pltfm_ucli_ucli__qsfp_mgmnt_temper_monit_log_enable,
    bf_pltfm_ucli_ucli__qsfp_mgmnt_temper_monitor_enable,
    bf_pltfm_ucli_ucli__qsfp_db,
    bf_pltfm_ucli_ucli__qsfp_map,
    bf_pltfm_ucli_ucli__qsfp_rxlos_debounce_set,
    bf_pltfm_ucli_ucli__qsfp_wavelength_get,
    bf_pltfm_ucli_ucli__qsfp_wavelength_set,
    bf_pltfm_ucli_ucli__qsfp_wavelength_clear,
    bf_pltfm_ucli_ucli__qsfp_get_vdm,
    NULL,
};

/* <auto.ucli.handlers.end> */
static ucli_module_t bf_pltfm_qsfp_ucli_module__
= {
    "qsfp_ucli", NULL, bf_pltfm_qsfp_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_qsfp_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_qsfp_ucli_module__);
    n = ucli_node_create ("qsfp", m,
                          &bf_pltfm_qsfp_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("qsfp"));
    return n;
}
