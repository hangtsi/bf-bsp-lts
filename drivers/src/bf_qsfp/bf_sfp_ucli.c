/* sfp cli source file
 * author : luyi
 * date: 2020/07/02
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_bd_cfg/bf_bd_cfg.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_sfp.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>

extern int bf_pltfm_get_sfp_ctx (struct sfp_ctx_t
                                 **sfp_ctx);

#if 0
static void check_check_code (ucli_context_t *uc,
                           uint8_t *buf, int start, int end)
{
    int i;
    uint32_t sum = 0;
    for (i = start; i <= end; i++) {
        sum += buf[i];
    }
    aim_printf (&uc->pvs,
        "sum of Byte %d-%d is 0x%llx\n", start, end, sum);
    aim_printf (&uc->pvs, "checksum is 0x%x\n",
        buf[end + 1]);
    if ((sum & 0xff) == buf[end + 1]) {
        aim_printf (&uc->pvs, "check_code correct!\n");
    } else {
        aim_printf (&uc->pvs, "check_code incorrect!\n");
    }
    return;
}
#endif

static void hex_dump (ucli_context_t *uc,
                   uint8_t *buf, uint32_t len)
{
    uint8_t byte;

    for (byte = 0; byte < len; byte++) {
        if ((byte % 16) == 0) {
            aim_printf (&uc->pvs, "\n%3d : ", byte);
        }
        aim_printf (&uc->pvs, "%02x ", buf[byte]);
    }
    aim_printf (&uc->pvs, "\n");
}

extern int sff_db_get (sff_db_entry_t **entries,
                       int *count);

static void
sff_info_show (sff_info_t *info,
               ucli_context_t *uc)
{
    aim_printf (&uc->pvs,
                "Vendor: %s Model: %s SN: %s Type: %s Module: %s Media: %s Length: %d\n",
                info->vendor, info->model, info->serial,
                info->sfp_type_name,
                info->module_type_name, info->media_type_name,
                info->length);
}

/* see SFF-8472. */
static char *bf_sfp_power_class (uint8_t b64)
{
    /* Maximum power is declared in A2h, byte 66. */
    if ((b64 & 0xC0) == 0x80) {
      return " 3 (2.0 W max.)";
    }
    if ((b64 & 0xC0) == 0x40) {
      return " 2 (1.5 W max.)";
    }
    if ((b64 & 0xC0) == 0x00) {
      return " 1 (1.0 W max.)";
    }
    return "...           ";
}

bool bf_sfp_get_spec_rev_str (uint8_t *idprom,
                              char *rev_str)
{
    uint8_t rev = idprom[94];
    if (!SFF8472_DOM_SUPPORTED (idprom)) {
        /* Not SFF-8472 ? */
        return false;
    }

    switch (rev) {
        case 0x0:
            strcpy (rev_str, "Not specified");
            break;
        case 0x1:
            strcpy (rev_str, "9.3");
            break;
        case 0x2:
            strcpy (rev_str, "9.5");
            break;
        case 0x3:
            strcpy (rev_str, "10.2");
            break;
        case 0x4:
            strcpy (rev_str, "10.4");
            break;
        case 0x5:
            strcpy (rev_str, "11.0");
            break;
        case 0x6:
            strcpy (rev_str, "11.3");
            break;
        case 0x7:
            strcpy (rev_str, "11.4");
            break;
        case 0x8:
            strcpy (rev_str, "12.3");
            break;
        case 0x9:
            strcpy (rev_str, "12.4");
            break;
        default:
            strcpy (rev_str, "Unknown");
            break;
    }

    return true;
}

static ucli_status_t
bf_sfp_dump_info (ucli_context_t
                                 *uc, int port, uint8_t *idprom)
{
  uint8_t *a0h, *a2h;
  sff_info_t *sff;
  sff_eeprom_t *se, eeprom;
  sff_dom_info_t sdi;
  int rc;

  se = &eeprom;
  sff = &se->info;
  memset (se, 0, sizeof (sff_eeprom_t));
  rc = sff_eeprom_parse (se, idprom);
  if (!se->identified) {
      aim_printf (&uc->pvs,
                  " SFP    %02d: non-standard <rc=%d>\n",
                  port, rc);
      /* sff_eeprom_parse is quite an importand API for most sfp.
       * but it is still not ready for all kind of sfps.
       * so override the failure here and keep tracking.
       * by tsihang, 2022-06-17. */
      //return 0;
  }

  a0h = idprom;
  a2h = idprom + MAX_SFP_PAGE_SIZE;
  sff_dom_info_get (&sdi, sff, a0h, a2h);

  /* keep same with qsfp dump. And the info below is quite enough for almost all scene. */

  aim_printf (&uc->pvs, "%-30s = %02d(Y%d)\n",
              "Port",
              port, port);

  aim_printf (&uc->pvs, "%-30s = %s(0x%02x)\n",
              "Module type identifier", sff->sfp_type_name,
              a0h[0]);
  aim_printf (&uc->pvs, "%-30s = %02x\n",
              "Ext. Identifer", a0h[1]);

  aim_printf (
      &uc->pvs, "%-30s = %s\n",
      "Module", sff->module_type_name);
  aim_printf (
      &uc->pvs, "%-30s = %s\n",
      "Media Type", sff->media_type_name);
  aim_printf (
      &uc->pvs, "%-30s = %s\n",
      "Connector Type",
      sff_connector_type_name (a0h[2]));

  aim_printf (&uc->pvs, "\n");
  aim_printf (&uc->pvs, "%-30s = %s\n",
              "Vendor name", sff->vendor);
  aim_printf (&uc->pvs, "%-30s = %s\n",
              "Vendor P/N", sff->model);
  aim_printf (&uc->pvs, "%-30s = %s\n",
              "Vendor S/N", sff->serial);
  aim_printf (&uc->pvs, "%-30s = %s\n",
              "Vendor Rev", sff->rev);
  aim_printf (&uc->pvs, "%-30s = %02x-%02x-%02x\n",
              "Vendor OUI", sff->oui[0], sff->oui[1],
              sff->oui[2]);

  aim_printf (&uc->pvs, "%-30s = %s\n",
              "Date & lot code",
              sff->date);

  aim_printf (&uc->pvs, "%-30s = ",
              "Memory map format");
  if (sdi.spec == SFF_DOM_SPEC_SFF8472) {
      aim_printf (&uc->pvs, "SFF-8472\n");
  } else {
      aim_printf (&uc->pvs, "Unknown\n");
  }

  char memmap_spec_rev[25];
  bf_sfp_get_spec_rev_str (a0h, memmap_spec_rev);
  aim_printf (&uc->pvs, "%-30s = %s\n",
              "Memory map spec rev", memmap_spec_rev);

  aim_printf (&uc->pvs, "%-30s = %02x(%02x)\n",
              "CC_BASE", se->cc_base, a0h[63]);
  aim_printf (&uc->pvs, "%-30s = %02x(%02x)\n",
              "CC_EXT", se->cc_ext, a0h[95]);

  aim_printf (&uc->pvs, "  Length (SMF): %d km\n",
              _sff8472_length_sm(a0h) / 1000);
  aim_printf (&uc->pvs, "  Length (OM3): %d m\n",
              _sff8472_length_om3(a0h));
  aim_printf (&uc->pvs, "  Length (OM2): %d m\n",
              _sff8472_length_om2(a0h));
  aim_printf (&uc->pvs, "  Length (OM1): %d m\n",
              _sff8472_length_om1(a0h));
  aim_printf (&uc->pvs, "  Length (Copper): %d m\n",
              _sff8472_length_cu(a0h));

  aim_printf (&uc->pvs, "\n");
  if (!bf_sfp_is_optical (port) ||
      sdi.spec != SFF_DOM_SPEC_SFF8472) {
      aim_printf (&uc->pvs,
                  "Not optical or not sff-8472\n");
  } else {
      /* A2h */
      aim_printf (&uc->pvs,
                  "## MODULE EXT PROPERTIES\n");

      aim_printf (&uc->pvs, "%-30s = %5d Mbps\n",
                  "Nominal Bit Rate",
                  a0h[12] * 100);

      aim_printf (&uc->pvs, "%-30s = %02x\n",
                  "Rate Identifier",
                  a0h[13]);

      aim_printf (&uc->pvs, "%-30s = %u nm\n",
                  "Laser Wavelength",
                  (a0h[60] << 8) | a0h[61]);

      aim_printf (&uc->pvs, "%-30s = %.3lf C\n",
                  "Temperature",
                  (double) (a2h[96]) + ((double)a2h[97] / 256));

      double voltage = (a2h[98] << 8) + a2h[99];
      aim_printf (&uc->pvs, "%-30s = %.3lf V\n",
                  "Voltage",
                  voltage / 10000.000);

      double bias_current = (a2h[100] << 8) + a2h[101];
      aim_printf (&uc->pvs, "%-30s = %.3lf mA\n",
                  "TX Bias",
                  bias_current / 1000.000);


      double tx_pwr = (a2h[102] << 8) + a2h[103];
      aim_printf (&uc->pvs, "%-30s = %.5f dBm\n",
                  "TX Power",
                  (tx_pwr == 0) ? (-40) : (10 * log10 (
                          tx_pwr * 0.1 / 1000.000)));

      double rx_pwr = (a2h[104] << 8) + a2h[105];
      aim_printf (&uc->pvs, "%-30s = %.5f dBm\n",
                  "RX Power",
                  (rx_pwr == 0) ? (-40) : (10 * log10 (
                          rx_pwr * 0.1 / 1000.000)));

      aim_printf (&uc->pvs, "Page: %d\n", a2h[127]);
  }

  aim_printf (&uc->pvs, "\nA0h:\n");
  hex_dump (uc, a0h, MAX_SFP_PAGE_SIZE);

  aim_printf (&uc->pvs, "\nA2h:\n");
  hex_dump (uc, a2h, MAX_SFP_PAGE_SIZE);

  return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_read_reg (ucli_context_t
                                  *uc)
{
    int err;
    int offset;
    uint8_t addr;
    uint8_t page;
    uint8_t val;
    int module;
    int max_port = bf_sfp_get_max_sfp_ports();

    UCLI_COMMAND_INFO (uc, "read-reg", 4,
                       "read-reg <module> <addr> <page,0/1/2> <offset>");

    module = strtol (uc->pargs->args[0], NULL, 0);
    addr = strtol (uc->pargs->args[1], NULL, 0);
    page = strtol (uc->pargs->args[2], NULL, 0);
    offset = strtol (uc->pargs->args[3], NULL, 0);

    if (module < 1 || module > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    aim_printf (
        &uc->pvs,
        "read-reg: read-reg <moudle=%d> <addr=0x%x> <page=%d> <offset=0x%x>\n",
        module,
        addr,
        page,
        offset);

    if (addr != 0xa0 && addr != 0xa2) {
        aim_printf (&uc->pvs,
                    "only addr 0xa0 or 0xa2 can be read!\n");
        return -1;
    }

    offset = (addr == 0xa0) ? offset : offset +
             MAX_SFP_PAGE_SIZE;
    err = bf_pltfm_sfp_read_reg (module, page, offset,
                                 &val);
    if (err) {
        aim_printf (&uc->pvs,
                    "error(%d) reading register offset=0x%02x\n", err,
                    offset);
        return -1;
    } else {
        aim_printf (&uc->pvs,
                    "SFP%d : page %d offset 0x%x read 0x%02x\n",
                    module, page,
                    offset, val);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_write_reg (
    ucli_context_t *uc)
{
    int err;
    int offset;
    uint8_t addr;
    uint8_t page;
    uint8_t val;
    int module;
    int max_port = bf_sfp_get_max_sfp_ports();

    UCLI_COMMAND_INFO (uc, "write-reg", 5,
                       "write-reg <module> <addr> <page,0/1/2> <offset> <val>");

    module = strtol (uc->pargs->args[0], NULL, 0);
    addr = strtol (uc->pargs->args[1], NULL, 0);
    page = strtol (uc->pargs->args[2], NULL, 0);
    offset = strtol (uc->pargs->args[3], NULL, 0);
    val = strtol (uc->pargs->args[4], NULL, 0);

    if (module < 1 || module > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    aim_printf (
        &uc->pvs,
        "write-reg: write-reg <module=%d> <addr=0x%x> <page=%d> <offset=0x%x> "
        "<val=0x%x>\n",
        module,
        addr,
        page,
        offset,
        val);

    if (addr != 0xa0 && addr != 0xa2) {
        aim_printf (&uc->pvs,
                    "only addr 0xa0 or 0xa2 can be read!\n");
        return -1;
    }

    offset = (addr == 0xa0) ? offset : offset +
             MAX_SFP_PAGE_SIZE;
    err = bf_pltfm_sfp_write_reg (module, page,
                                  offset, val);
    if (err) {
        aim_printf (&uc->pvs,
                    "error(%d) writing register offset=0x%02x\n", err,
                    offset);
        return -1;
    } else {
        aim_printf (&uc->pvs,
                    "SFP%d : page %d offset 0x%x written 0x%0x\n",
                    module, page,
                    offset, val);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_dump_info (ucli_context_t
                                   *uc)
{
    int module;
    int rc;
    uint8_t idprom[MAX_SFP_PAGE_SIZE * 2 + 1] = {0};

    UCLI_COMMAND_INFO (uc, "dump-info", 1,
                       "dump-info <module>");
    module = strtol (uc->pargs->args[0], NULL, 0);
    if (module < 1 || module > bf_sfp_get_max_sfp_ports()) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    bf_sfp_get_max_sfp_ports());
        return 0;
    }

    if (!bf_sfp_is_present (module)) {
        return 0;
    }

    /* A0h */
    rc = bf_pltfm_sfp_read_module (module, 0,
                                      MAX_SFP_PAGE_SIZE, idprom);
    if (rc) {
        return 0;
    }
    /* A2h, what should we do if there's no A2h ? */
    rc = bf_pltfm_sfp_read_module (module,
                                  MAX_SFP_PAGE_SIZE, MAX_SFP_PAGE_SIZE,
                                  idprom + MAX_SFP_PAGE_SIZE);
    if (rc) {
        return 0;
    }

    return bf_sfp_dump_info (uc, module, idprom);
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_show_module (
    ucli_context_t
    *uc)
{
    int module;
    int rc;
    uint8_t idprom[MAX_SFP_PAGE_SIZE * 2 + 1] = {0};

    UCLI_COMMAND_INFO (uc, "show", 1,
                       "show <module>");
    module = strtol (uc->pargs->args[0], NULL, 0);
    if (module < 1 || module > bf_sfp_get_max_sfp_ports()) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    bf_sfp_get_max_sfp_ports());
        return 0;
    }

    if (!bf_sfp_is_present (module)) {
        return 0;
    }

    rc = bf_sfp_get_cached_info (module, 0, idprom);
    if (rc) {
        aim_printf (&uc->pvs,
                    "Unknown module : %2d\n",
                    module);
        return 0;
    }

    rc = bf_sfp_get_cached_info (module, 1,
                                 idprom + MAX_SFP_PAGE_SIZE);
    if (rc) {
        aim_printf (&uc->pvs,
                    "Unknown module : %2d\n",
                    module);
        return 0;
    }

    return bf_sfp_dump_info (uc, module, idprom);
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_summary (ucli_context_t
                                  *uc)
{
    int port;
    int rc;
    uint8_t *a0h, *a2h;
    sff_info_t *sff;
    sff_eeprom_t *se, eeprom;
    sff_dom_info_t sdi;
    uint8_t idprom[MAX_SFP_PAGE_SIZE * 2 + 1] = {0};

    UCLI_COMMAND_INFO (uc, "info", 0, "info");

    aim_printf (&uc->pvs, "\n================\n");
    aim_printf (&uc->pvs, "Info for SFP  :\n");
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

    for (port = 1; port <= bf_sfp_get_max_sfp_ports(); port++) {
        if (!bf_sfp_is_present (port)) {
            continue;
        }

        /* A0h */
        rc = bf_sfp_get_cached_info (port, 0, idprom);
        if (rc) {
            aim_printf (&uc->pvs,
                        "Unknown module : %2d\n",
                        port);
            return 0;
        }

        /* A2h */
        rc = bf_sfp_get_cached_info (port, 1,
                                     idprom + MAX_SFP_PAGE_SIZE);
        if (rc) {
            aim_printf (&uc->pvs,
                        "Unknown module : %2d\n",
                        port);
            return 0;
        }

        se = &eeprom;
        sff = &se->info;
        memset (se, 0, sizeof (sff_eeprom_t));
        rc = sff_eeprom_parse (se, idprom);
        if (!se->identified) {
            aim_printf (&uc->pvs,
                        " SFP    %02d: non-standard <rc=%d>\n",
                        port, rc);
            /* sff_eeprom_parse is quite an importand API for most sfp.
             * but it is still not ready for all kind of sfps.
             * so override the failure here and keep tracking.
             * by tsihang, 2022-06-17. */
            //return 0;
        }

        a0h = idprom;
        a2h = idprom + MAX_SFP_PAGE_SIZE;
        sff_dom_info_get (&sdi, sff, a0h, a2h);

        /* print lower page data */
        aim_printf (&uc->pvs, " %2d:  ", port);


        aim_printf (&uc->pvs, "%s ", sff->vendor);
        aim_printf (&uc->pvs, "%s ", sff->model);
        aim_printf (&uc->pvs, "%s ", sff->rev);
        aim_printf (&uc->pvs, "%s ", sff->serial);
        aim_printf (&uc->pvs, "%s ", sff->date);
        aim_printf (&uc->pvs, "%5d MBps ",
                    a0h[12] * 100);
        aim_printf (&uc->pvs,
                    "  %02x:%02x:%02x",
                    sff->oui[0],
                    sff->oui[1],
                    sff->oui[2]);
        aim_printf (&uc->pvs, "  %s ",
                    bf_sfp_power_class (a0h[64]));

        if (bf_sfp_is_optical (port)) {
            if (_sff8472_length_sm(a0h)) {
                /* a0h[14] in units km, max 254 km. */
                /* a0h[15] in units 100m, max 25.4 km. */
                aim_printf (&uc->pvs, "  (SMF): %d km ",
                        _sff8472_length_sm(a0h) / 1000);

            }
            if (_sff8472_length_om4 (a0h)) {
                aim_printf (&uc->pvs, "  (OM4): %d m ",
                            _sff8472_length_om4(a0h) * 2);
            }
            if (_sff8472_length_om3(a0h)) {
                aim_printf (&uc->pvs, "  (OM3): %d m ",
                            _sff8472_length_om3(a0h) * 2);
            }
            if (_sff8472_length_om2(a0h)) {
                aim_printf (&uc->pvs, "  (OM2): %d m ",
                            _sff8472_length_om2(a0h));
            }
            if (_sff8472_length_om1(a0h)) {
                aim_printf (&uc->pvs, "  (OM1): %d m ",
                            _sff8472_length_om1(a0h));
            }
        } else {
            if (_sff8472_length_cu(a0h)) {
                aim_printf (&uc->pvs, "  (Copper): %d m ",
                            _sff8472_length_cu(a0h));
            }
        }
        aim_printf (&uc->pvs, "\n");
    }
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_db (ucli_context_t
                            *uc)
{
    sff_info_t *sff;
    sff_eeprom_t *se;
    sff_db_entry_t *entry;
    int num = 0;
    int i;

    UCLI_COMMAND_INFO (uc, "db", 0,
                       "Display SFP database.");

    sff_db_get (&entry, &num);
    for (i = 0; i < num; i ++) {
        se = &entry[i].se;
        sff = &se->info;
        if (sff->sfp_type == SFF_SFP_TYPE_SFP ||
            sff->sfp_type == SFF_SFP_TYPE_SFP28) {
            sff_info_show (sff, uc);
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_map (ucli_context_t
                             *uc)
{
    int module, err;
    uint32_t conn_id, chnl_id = 0;
    char alias[8] = {0}, connc[8] = {0};

    UCLI_COMMAND_INFO (uc, "map", 0,
                       "Display SFP map.");

    /* Dump the map of Module <-> Alias <-> QSFP/CH <-> Present. */
    aim_printf (&uc->pvs, "%12s%12s%20s%12s\n",
                "MODULE", "ALIAS", "PORT", "PRESENT");

    for (int i = 0; i < bf_sfp_get_max_sfp_ports();
         i ++) {
        module = (i + 1);
        err = bf_pltfm_sfp_lookup_by_module (module,
                                             &conn_id, &chnl_id);
        if (!err) {
            sprintf (alias, "Y%d",
                     module % (BF_PLAT_MAX_QSFP * 4));
            sprintf (connc, "%2d/%d",
                     (conn_id % BF_PLAT_MAX_QSFP),
                     (chnl_id % MAX_CHAN_PER_CONNECTOR));
            aim_printf (&uc->pvs, "%12d%12s%20s%12s\n",
                        module, alias, connc,
                        bf_sfp_is_present (module) ? "true" : "false");
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_fsm (ucli_context_t *uc)
{
    int port, first_port, last_port, max_port;

    UCLI_COMMAND_INFO (uc, "fsm", -1,
                        "fsm <port>");

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
    } else {
        first_port = 1;
        last_port = bf_sfp_get_max_sfp_ports();
    }

    max_port = bf_sfp_get_max_sfp_ports();

    if (first_port < 1 || last_port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                 max_port);
        return 0;
    }

    aim_printf (&uc->pvs, "%s | ", "Port");
    aim_printf (&uc->pvs, "%-28s | ", "Module FSM");
    aim_printf (&uc->pvs, "%-29s | \n", "CH ");

    for (port = first_port; port <= last_port;
      port++) {
        aim_printf (&uc->pvs, "%-4d | %s | ", port,
                    sfp_module_fsm_st_get (port));
        aim_printf (&uc->pvs, "%-29s | ",
                    sfp_channel_fsm_st_get (port));

        aim_printf (&uc->pvs, "\n");
    }
    return 0;
}

#if 0
/* Unused CMD if cc_base and cc_ext have been dumped by cmd dump-info.
 * by tsihang, 2021-07-29. */
static ucli_status_t
bf_pltfm_ucli_ucli__check_reg (ucli_context_t
                               *uc)
{
    // define
    int module;
    uint8_t a0h[MAX_SFP_PAGE_SIZE + 1] = {0};
    uint8_t a2h[MAX_SFP_PAGE_SIZE + 1] = {0};
    // convert
    UCLI_COMMAND_INFO (uc, "check-reg", 1,
                       "check-reg <module>");
    module = strtol (uc->pargs->args[0], NULL, 0);

    if (!bf_sfp_is_present (module)) {
        return 0;
    }

    // read eeprom info directly
    if (bf_pltfm_sfp_read_module (module, 0,
                                  MAX_SFP_PAGE_SIZE, a0h)) {
        aim_printf (&uc->pvs,
                    "cannot get SFP %d eeprom info, there's maybe something wrong\n",
                    module);
        return BF_PLTFM_INVALID_ARG;
    }
    if (bf_pltfm_sfp_read_module (module,
                                  MAX_SFP_PAGE_SIZE, MAX_SFP_PAGE_SIZE, a2h)) {
        aim_printf (&uc->pvs,
                    "cannot get SFP %d eeprom info, there's maybe something wrong\n",
                    module);
        return BF_PLTFM_INVALID_ARG;
    }

    // show it all
    aim_printf (&uc->pvs, "SFP Y%02d:\n", module);
    aim_printf (&uc->pvs,
                "=======================\n");
    aim_printf (&uc->pvs, "0xA0:\n");
    hex_dump (uc, a0h, MAX_SFP_PAGE_SIZE);
    check_check_code (uc, a0h, 0, 62);
    check_check_code (uc, a0h, 64, 94);
    aim_printf (&uc->pvs,
                "\n=======================\n");
    aim_printf (&uc->pvs, "0xA2:\n");
    hex_dump (uc, a2h, MAX_SFP_PAGE_SIZE);
    check_check_code (uc, a2h, 0, 94);
    // return
    return 0;
}
#endif

static ucli_command_handler_f
bf_pltfm_sfp_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__sfp_read_reg,
    bf_pltfm_ucli_ucli__sfp_write_reg,
    bf_pltfm_ucli_ucli__sfp_dump_info,
    bf_pltfm_ucli_ucli__sfp_show_module,
    bf_pltfm_ucli_ucli__sfp_summary,
    bf_pltfm_ucli_ucli__sfp_db,
    bf_pltfm_ucli_ucli__sfp_map,
    bf_pltfm_ucli_ucli__sfp_fsm,
    /* Closed by tsihang since dump-info include the CC. */
    //bf_pltfm_ucli_ucli__check_reg,
    NULL,
};

/* <auto.ucli.handlers.end> */
static ucli_module_t bf_pltfm_sfp_ucli_module__
= {
    "sfp_ucli", NULL, bf_pltfm_sfp_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_sfp_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_sfp_ucli_module__);
    n = ucli_node_create ("sfp", m,
                          &bf_pltfm_sfp_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("sfp"));
    return n;
}
