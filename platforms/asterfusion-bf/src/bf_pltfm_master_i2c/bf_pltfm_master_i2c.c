/*!
 * @file bf_pltfm_master_i2c.c
 * @date 2020/03/18
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>	/* for NAME_MAX */
#include <sys/ioctl.h>
#include <string.h>
#include <strings.h>	/* for strcasecmp() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include <bf_pltfm_types/bf_pltfm_types.h>
#include "bf_pltfm_cgos_i2c.h"
#include <bf_types/bf_types.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_master_i2c.h>
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>
#include <bf_pltfm_chss_mgmt_intf.h>

#include <pltfm_types.h>

bf_sys_rmutex_t master_i2c_lock;

#define MISSING_FUNC_FMT	"Error: Adapter does not have %s capability\n"

enum adt { adt_dummy, adt_isa, adt_i2c, adt_smbus, adt_unknown };

struct adap_type {
    const char *funcs;
    const char *algo;
};

static struct adap_type adap_types[5] = {
    {
        .funcs	= "dummy",
        .algo		= "Dummy bus",
    },
    {
        .funcs	= "isa",
        .algo		= "ISA bus",
    },
    {
        .funcs	= "i2c",
        .algo		= "I2C adapter",
    },
    {
        .funcs	= "smbus",
        .algo		= "SMBus adapter",
    },
    {
        .funcs	= "unknown",
        .algo		= "N/A",
    },
};

struct i2c_adap {
    int nr;
    char *name;
    const char *funcs;
    const char *algo;
};

struct i2c_ctx_t {
    int fd;
    int fd_suio;
    int fd_cp2112;
} i2c_ctx = {
    .fd = -1,
    .fd_suio = -1,
    .fd_cp2112 = -1,
};

#define Enter fprintf(stdout, "%s:%d\n", __func__, __LINE__);

int open_i2c_dev (int i2cbus, char *filename,
                  size_t size, int quiet)
{
    int file;

    snprintf (filename, size, "/dev/i2c/%d", i2cbus);
    filename[size - 1] = '\0';
    file = open (filename, O_RDWR);

    if (file < 0 && (errno == ENOENT ||
                     errno == ENOTDIR)) {
        sprintf (filename, "/dev/i2c-%d", i2cbus);
        file = open (filename, O_RDWR);
    }

    if (file < 0 && !quiet) {
        if (errno == ENOENT) {
            LOG_ERROR ("Error: Could not open file "
                       "`/dev/i2c-%d' or `/dev/i2c/%d': %s",
                       i2cbus, i2cbus, strerror (ENOENT));
        } else {
            LOG_ERROR ("Error: Could not open file "
                       "`%s': %s", filename, strerror (errno));
            if (errno == EACCES) {
                LOG_ERROR ("Run as root?");
            }
        }
    }

    return file;
}

static enum adt i2c_get_funcs (int i2cbus)
{
    unsigned long funcs;
    int file;
    char filename[20];
    enum adt ret;

    file = open_i2c_dev (i2cbus, filename,
                         sizeof (filename), 1);
    if (file < 0) {
        return adt_unknown;
    }

    if (ioctl (file, I2C_FUNCS, &funcs) < 0) {
        ret = adt_unknown;
    } else if (funcs & I2C_FUNC_I2C) {
        ret = adt_i2c;
    } else if (funcs & (I2C_FUNC_SMBUS_BYTE |
                        I2C_FUNC_SMBUS_BYTE_DATA |
                        I2C_FUNC_SMBUS_WORD_DATA)) {
        ret = adt_smbus;
    } else {
        ret = adt_dummy;
    }

    close (file);
    return ret;
}

/* Remove trailing spaces from a string
   Return the new string length including the trailing NUL */
static int rtrim (char *s)
{
    int i;

    for (i = strlen (s) - 1; i >= 0 && (s[i] == ' ' ||
                                        s[i] == '\n'); i--) {
        s[i] = '\0';
    }
    return i + 2;
}

void free_adapters (struct i2c_adap *adapters)
{
    int i;

    for (i = 0; adapters[i].name; i++) {
        free (adapters[i].name);
    }
    free (adapters);
}

/* We allocate space for the adapters in bunches. The last item is a
   terminator, so here we start with room for 7 adapters, which should
   be enough in most cases. If not, we allocate more later as needed. */
#define BUNCH	8

/* n must match the size of adapters at calling time */
static struct i2c_adap *more_adapters (
    struct i2c_adap *adapters, int n)
{
    struct i2c_adap *new_adapters;

    new_adapters = realloc (adapters,
                            (n + BUNCH) * sizeof (struct i2c_adap));
    if (!new_adapters) {
        free_adapters (adapters);
        return NULL;
    }
    memset (new_adapters + n, 0,
            BUNCH * sizeof (struct i2c_adap));

    return new_adapters;
}

struct i2c_adap *gather_i2c_busses (void)
{
    char s[120];
    struct dirent *de, *dde;
    DIR *dir, *ddir;
    FILE *f;
    char fstype[NAME_MAX], sysfs[NAME_MAX],
         n[NAME_MAX + NAME_MAX + NAME_MAX + NAME_MAX];/* Fixed compiled errors with gcc-8 in debian 10 */
    int foundsysfs = 0;
    int count = 0;
    struct i2c_adap *adapters;

    adapters = calloc (BUNCH,
                       sizeof (struct i2c_adap));
    if (!adapters) {
        return NULL;
    }

    /* look in /proc/bus/i2c */
    if ((f = fopen ("/proc/bus/i2c", "r"))) {
        while (fgets (s, 120, f)) {
            char *algo, *name, *type, *all;
            int len_algo, len_name, len_type;
            int i2cbus;

            algo = strrchr (s, '\t');
            * (algo++) = '\0';
            len_algo = rtrim (algo);

            name = strrchr (s, '\t');
            * (name++) = '\0';
            len_name = rtrim (name);

            type = strrchr (s, '\t');
            * (type++) = '\0';
            len_type = rtrim (type);

            sscanf (s, "i2c-%d", &i2cbus);

            if ((count + 1) % BUNCH == 0) {
                /* We need more space */
                adapters = more_adapters (adapters, count + 1);
                if (!adapters) {
                    return NULL;
                }
            }

            all = malloc (len_name + len_type + len_algo);
            if (all == NULL) {
                free_adapters (adapters);
                return NULL;
            }
            adapters[count].nr = i2cbus;
            adapters[count].name = strcpy (all, name);
            adapters[count].funcs = strcpy (all + len_name,
                                            type);
            adapters[count].algo = strcpy (all + len_name +
                                           len_type,
                                           algo);
            count++;
        }
        fclose (f);
        goto done;
    }

    /* look in sysfs */
    /* First figure out where sysfs was mounted */
    if ((f = fopen ("/proc/mounts", "r")) == NULL) {
        goto done;
    }

    while (fgets (n, NAME_MAX, f)) {
        sscanf (n, "%*[^ ] %[^ ] %[^ ] %*s\n", sysfs,
                fstype);
        if (strcasecmp (fstype, "sysfs") == 0) {
            foundsysfs++;
            break;
        }
    }
    fclose (f);
    if (! foundsysfs) {
        goto done;
    }

    /* Bus numbers in i2c-adapter don't necessarily match those in
       i2c-dev and what we really care about are the i2c-dev numbers.
       Unfortunately the names are harder to get in i2c-dev */
    strcat (sysfs, "/class/i2c-dev");
    if (! (dir = opendir (sysfs))) {
        goto done;
    }
    /* go through the busses */
    while ((de = readdir (dir)) != NULL) {
        if (!strcmp (de->d_name, ".")) {
            continue;
        }
        if (!strcmp (de->d_name, "..")) {
            continue;
        }

        /* this should work for kernels 2.6.5 or higher and */
        /* is preferred because is unambiguous */
        sprintf (n, "%s/%s/name", sysfs, de->d_name);
        f = fopen (n, "r");
        /* this seems to work for ISA */
        if (f == NULL) {
            sprintf (n, "%s/%s/device/name", sysfs,
                     de->d_name);
            f = fopen (n, "r");
        }
        /* non-ISA is much harder */
        /* and this won't find the correct bus name if a driver
           has more than one bus */
        if (f == NULL) {
            sprintf (n, "%s/%s/device", sysfs, de->d_name);
            if (! (ddir = opendir (n))) {
                continue;
            }
            while ((dde = readdir (ddir)) != NULL) {
                if (!strcmp (dde->d_name, ".")) {
                    continue;
                }
                if (!strcmp (dde->d_name, "..")) {
                    continue;
                }
                if ((!strncmp (dde->d_name, "i2c-", 4))) {
                    sprintf (n, "%s/%s/device/%s/name",
                             sysfs, de->d_name, dde->d_name);
                    if ((f = fopen (n, "r"))) {
                        goto found;
                    }
                }
            }
        }

found:
        if (f != NULL) {
            int i2cbus;
            enum adt type;
            char *px;

            px = fgets (s, 120, f);
            fclose (f);
            if (!px) {
                continue;
            }
            if ((px = strchr (s, '\n')) != NULL) {
                *px = 0;
            }
            if (!sscanf (de->d_name, "i2c-%d", &i2cbus)) {
                continue;
            }
            if (!strncmp (s, "ISA ", 4)) {
                type = adt_isa;
            } else {
                /* Attempt to probe for adapter capabilities */
                type = i2c_get_funcs (i2cbus);
            }

            if ((count + 1) % BUNCH == 0) {
                /* We need more space */
                adapters = more_adapters (adapters, count + 1);
                if (!adapters) {
                    return NULL;
                }
            }

            adapters[count].nr = i2cbus;
            adapters[count].name = strdup (s);
            if (adapters[count].name == NULL) {
                free_adapters (adapters);
                return NULL;
            }
            adapters[count].funcs = adap_types[type].funcs;
            adapters[count].algo = adap_types[type].algo;
            count++;
        }
    }
    closedir (dir);

done:
    return adapters;
}

static int lookup_i2c_bus_by_name (
    const char *bus_name)
{
    struct i2c_adap *adapters;
    int i, i2cbus = -1;

    adapters = gather_i2c_busses();
    if (adapters == NULL) {
        LOG_ERROR ("Error: Out of memory!");
        return -3;
    }

    /* Walk the list of i2c busses, looking for the one with the
       right name */
    for (i = 0; adapters[i].name; i++) {
        if (strcmp (adapters[i].name, bus_name) == 0) {
            if (i2cbus >= 0) {
                LOG_ERROR (
                    "Error: I2C bus name is not unique!");
                i2cbus = -4;
                goto done;
            }
            i2cbus = adapters[i].nr;
        }
    }

    if (i2cbus == -1)
        LOG_WARNING ("Warning: I2C bus name doesn't match any "
                   "bus present!");

done:
    free_adapters (adapters);
    return i2cbus;
}

/*
 * Parse an I2CBUS command line argument and return the corresponding
 * bus number, or a negative value if the bus is invalid.
 */
int lookup_i2c_bus (const char *i2cbus_arg)
{
    unsigned long i2cbus;
    char *end;

    i2cbus = strtoul (i2cbus_arg, &end, 0);
    if (*end || !*i2cbus_arg) {
        /* Not a number, maybe a name? */
        return lookup_i2c_bus_by_name (i2cbus_arg);
    }
    if (i2cbus > 0xFFFFF) {
        LOG_ERROR ("Error: I2C bus out of range!");
        return -2;
    }

    return i2cbus;
}

/*
 * Parse a CHIP-ADDRESS command line argument and return the corresponding
 * chip address, or a negative value if the address is invalid.
 */
int parse_i2c_address (const char *address_arg)
{
    long address;
    char *end;

    address = strtol (address_arg, &end, 0);
    if (*end || !*address_arg) {
        LOG_ERROR ("Error: Chip address is not a number!");
        return -1;
    }
    if (address < 0x03 || address > 0x77) {
        LOG_ERROR ("Error: Chip address out of range "
                   "(0x03-0x77)!");
        return -2;
    }

    return address;
}

int check_funcs (int file, int size, int pec)
{
    unsigned long funcs;

    /* check adapter functionality */
    if (ioctl (file, I2C_FUNCS, &funcs) < 0) {
        LOG_ERROR ("Error: Could not get the adapter "
                   "functionality matrix: %s", strerror (errno));
        return -1;
    }

    switch (size) {
        case I2C_SMBUS_BYTE:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_BYTE)) {
                LOG_ERROR (MISSING_FUNC_FMT, "SMBus send byte");
                return -1;
            }
            break;

        case I2C_SMBUS_BYTE_DATA:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
                LOG_ERROR (MISSING_FUNC_FMT, "SMBus write byte");
                return -1;
            }
            break;

        case I2C_SMBUS_WORD_DATA:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_WORD_DATA)) {
                LOG_ERROR (MISSING_FUNC_FMT, "SMBus write word");
                return -1;
            }
            break;

        case I2C_SMBUS_BLOCK_DATA:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_BLOCK_DATA)) {
                LOG_ERROR (MISSING_FUNC_FMT, "SMBus block write");
                return -1;
            }
            break;
        case I2C_SMBUS_I2C_BLOCK_DATA:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)) {
                LOG_ERROR (MISSING_FUNC_FMT, "I2C block write");
                return -1;
            }
            break;
    }

    if (pec
        && ! (funcs & (I2C_FUNC_SMBUS_PEC |
                       I2C_FUNC_I2C))) {
        LOG_ERROR ("Warning: Adapter does "
                   "not seem to support PEC");
    }

    return 0;
}

static int bf_pltfm_master_i2c_select(uint8_t read_write_slave)
{
    if (platform_type_equal (X308P)) {
        i2c_ctx.fd = i2c_ctx.fd_suio;
    } else if (platform_type_equal (X312P)) {
        if (platform_subtype_equal(v1dot2)) {
            i2c_ctx.fd = i2c_ctx.fd_cp2112;
        } else if (platform_subtype_equal(v1dot3)) {
            switch (read_write_slave)
            {
                case 0x60:          //X312P_CPLD1_ADDR
                case 0x62:          //X312P_CPLD3_ADDR
                case 0x64:          //X312P_CPLD4_ADDR
                case 0x66:          //X312P_CPLD5_ADDR
                case 0x3E:          //X312P_BMC_ADDR
                    i2c_ctx.fd = i2c_ctx.fd_suio;
                    break;
                default:
                    i2c_ctx.fd = i2c_ctx.fd_cp2112;
                    break;
            }
        }
    }

    return 0;
}

int bf_pltfm_master_i2c_read_byte (
    uint8_t slave,
    uint8_t offset,
    uint8_t *value)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX()) {
        return bf_cgos_i2c_read_byte (slave, offset,
                                      value);
    }
    int force = 0;
    int err;
    int32_t val = 0;
    struct i2c_ctx_t *i2c = &i2c_ctx;

    bf_pltfm_master_i2c_select(slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (i2c->fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "Error: Could not set address to 0x%02x: %s",
            slave, strerror (errno));
        return -1;
    }

    val = i2c_smbus_read_byte_data (i2c->fd, offset);
    if (val < 0) {
        LOG_ERROR (
            "Error: Could not read offset 0x%02x: %s",
            offset, strerror (errno));
        return -3;
    }
    *value = val;

    return 0;
}

int bf_pltfm_master_i2c_read_block (
    uint8_t slave,
    uint8_t offset,
    uint8_t *rdbuf,
    uint8_t  rdlen)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX()) {
        return bf_cgos_i2c_read_block (slave, offset,
                                       rdbuf, rdlen);
    }
    int force = 0;
    int err;
    struct i2c_ctx_t *i2c = &i2c_ctx;

    bf_pltfm_master_i2c_select(slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (i2c->fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "Error: Could not set address to 0x%02x: %s",
            slave, strerror (errno));
        return -1;
    }

    int i = 0;
    int off = offset;

#if 1
    int32_t val = 0;
    for (off = offset; off < (int) (offset + rdlen);
         off ++) {
        val = i2c_smbus_read_byte_data (i2c->fd, off);
        if (val < 0) {
            LOG_ERROR (
                "Error: Could not read offset 0x%02x: %s",
                off, strerror (errno));
            return -3;
        }
        rdbuf[i ++] = val;
        usleep (1000);
    }
#else
    /* Doesn't support I2C block W/R method */
    val = i2c_smbus_read_block_data (i2c->fd, off,
                                     &rdbuf[0]);
    if (val < 0) {
        MASTER_I2C_UNLOCK;
        fprintf (stdout,
                 "Error: Could not read offset 0x%02x: %s\n",
                 off, strerror (errno));
        return -3;
    }
#endif

    return 0;
}


int bf_pltfm_master_i2c_write_byte (
    uint8_t slave,
    uint8_t offset,
    uint8_t value)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX()) {
        return bf_cgos_i2c_write_byte (slave, offset,
                                       value);
    }
    int force = 0;
    int err;
    struct i2c_ctx_t *i2c = &i2c_ctx;

    bf_pltfm_master_i2c_select(slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (i2c->fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "Error: Could not set address to 0x%02x: %s",
            slave, strerror (errno));
        return -1;
    }

    err = i2c_smbus_write_byte_data (i2c->fd, offset,
                                     value);
    if (err < 0) {
        LOG_ERROR (
            "Error: Could not select offset to 0x%02x : 0x%02x: %s",
            slave, offset, strerror (errno));
        return -2;
    }

    return 0;
}

int bf_pltfm_master_i2c_write_block (
    uint8_t slave,
    uint8_t offset,
    uint8_t *wrbuf,
    uint8_t  wrlen)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX()) {
        return bf_cgos_i2c_write_block (slave, offset,
                                        wrbuf, wrlen);
    }
    int force = 0;
    int err;
    struct i2c_ctx_t *i2c = &i2c_ctx;

    bf_pltfm_master_i2c_select(slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (i2c->fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "Error: Could not set address to 0x%02x: %s",
            slave, strerror (errno));
        return -1;
    }

    int i = 0;
    int off = offset;

    for (off = offset; off < (int) (offset + wrlen);
         off ++) {
        err = i2c_smbus_write_byte_data (i2c->fd, off,
                                         wrbuf[i ++]);
        if (err < 0) {
            LOG_ERROR (
                "Error: Could not select offset to 0x%02x : 0x%02x: %s",
                slave, offset, strerror (errno));
            return -2;
        }
        usleep (1000);
    }

    return 0;
}

// this func returns read_len if read success, -1 when fail
int bf_pltfm_bmc_write_read (
    uint8_t read_write_slave,
    uint8_t write_offset,
    uint8_t *write_buf,
    uint8_t write_len,
    uint8_t read_offset,
    uint8_t *read_buf,
    int usec)
{

    /* sp for COM-e: CG15XX */
    if (is_CG15XX()) {
        return bf_cgos_i2c_bmc_read (read_write_slave,
                                     write_offset, write_buf, write_len, read_buf,
                                     usec);
    }

    int err;
    struct i2c_ctx_t *i2c = &i2c_ctx;

    MASTER_I2C_LOCK;

    bf_pltfm_master_i2c_select(read_write_slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (i2c->fd, I2C_SLAVE,
                 read_write_slave);
    if (err < 0) {
        MASTER_I2C_UNLOCK;
        LOG_ERROR (
            "Error: Could not set address to 0x%02x: %s",
            read_write_slave, strerror (errno));
        return -1;
    }

    err = i2c_smbus_write_block_data (i2c->fd,
                                      write_offset, write_len, write_buf);
    if (err < 0) {
        MASTER_I2C_UNLOCK;
        LOG_ERROR (
            "Error: Could not write address to 0x%02x: %s",
            read_write_slave, strerror (errno));
        return -1;
    }

    usleep (usec);

    err = i2c_smbus_read_block_data (i2c->fd,
                                     read_offset, read_buf);
    if (err < 0) {
        MASTER_I2C_UNLOCK;
        LOG_ERROR (
            "Error: Could not read address to 0x%02x: %s",
            read_write_slave, strerror (errno));
        return -1;
    }

    MASTER_I2C_UNLOCK;
    return err;
}

int bf_pltfm_bmc_write (
    uint8_t read_write_slave,
    uint8_t write_offset,
    uint8_t *write_buf,
    uint8_t write_len)
{

    /* sp for COM-e: CG15XX */
    if (is_CG15XX()) {
        return bf_cgos_i2c_bmc_write (read_write_slave,
                                      write_offset, write_buf, write_len);
    }

    int err;
    struct i2c_ctx_t *i2c = &i2c_ctx;

    MASTER_I2C_LOCK;

    bf_pltfm_master_i2c_select(read_write_slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (i2c->fd, I2C_SLAVE,
                 read_write_slave);
    if (err < 0) {
        MASTER_I2C_UNLOCK;
        LOG_ERROR (
            "Error: Could not set address to 0x%02x: %s",
            read_write_slave, strerror (errno));
        return -1;
    }

    err = i2c_smbus_write_block_data (i2c->fd,
                                      write_offset, write_len, write_buf);
    if (err < 0) {
        MASTER_I2C_UNLOCK;
        LOG_ERROR (
            "Error: Could not write address to 0x%02x: %s",
            read_write_slave, strerror (errno));
        return -1;
    }

    MASTER_I2C_UNLOCK;
    return err;
}

int bf_pltfm_master_i2c_init()
{
    char filename[20] = {0};
    struct i2c_ctx_t *i2c = &i2c_ctx;
    int i2cbus;
    /* Error of "I2C bus name doesn't match any bus present!" occured
     * when find CP2112 during second launching of switchd.
     * The reason why this happen is that libusb would try to detach to kernel
     * at every launch and we couldn't see by 'i2cdetect -l' after detach successfully.
     * At this moment I don't see any negative effects with such an error/warnning.
     * by tsihang, 2022-04-20. */
    const char *i2c_bus_name[] = {
        "CP2112 SMBus Bridge on hidraw0",
        "CP2112 SMBus Bridge on hiddev0",
        "sio_smbus"
    };

    if (bf_sys_rmutex_init (&master_i2c_lock) != 0) {
        LOG_ERROR ("pltfm_mgr: i2c lock init failed\n");
        return -1;
    }

    if (global_come_type == COME_UNKNOWN) {
        fprintf (stdout, "X86 Come type unspecified.\n");
        exit (1);
    } else {
        if (is_CG15XX()) {
            if (bf_cgos_init()) {
                fprintf (stdout, "Error in cgos init \n");
                exit (1);
            }
        } else {
#if 0
            /* At the very beginnig we don't know which i2c-channel is used
             * to read eeprom to get hw version. So MAKE SURE that a correct
             * i2c channel is given by /etc/platform.conf under cme3000/cme7000.
             * by tsihang, 2022-04-19. */
            i2c->fd = open_i2c_dev (bmc_i2c_bus, filename,
                                    sizeof (filename), 0);
            if (i2c->fd < 0) {
                exit (1);
            }
#else
            for (int i = 0; i < (int)ARRAY_LENGTH (i2c_bus_name); i ++) {
                i2cbus = lookup_i2c_bus(i2c_bus_name[i]);
                if (i2cbus >= 0) {
                    if (strstr(i2c_bus_name[i], "CP2112")) {
                        i2c->fd_cp2112 = open_i2c_dev (i2cbus, filename,
                                    sizeof (filename), 0);
                        if (i2c->fd_cp2112 < 0) {
                            exit (1);
                        }
                        if (i2cbus == bmc_i2c_bus) {
                            /* At the very beginnig we don't know which i2c-channel is selected
                             * to read eeprom to get correct hw version. So MAKE SURE that a correct
                             * i2c channel is given by /etc/platform.conf under cme3000/cme7000.
                             * by tsihang, 2022-04-19. */
                            i2c->fd = i2c->fd_cp2112;
                        }
                    } else if (strstr(i2c_bus_name[i], "sio")) {
                        i2c->fd_suio = open_i2c_dev (i2cbus, filename,
                                    sizeof (filename), 0);
                        if (i2c->fd_suio < 0) {
                            exit (1);
                        }
                        if (i2cbus == bmc_i2c_bus) {
                            /* At the very beginnig we don't know which i2c-channel is selected
                             * to read eeprom to get correct hw version. So MAKE SURE that a correct
                             * i2c channel is given by /etc/platform.conf under cme3000/cme7000.
                             * by tsihang, 2022-04-19. */
                            i2c->fd = i2c->fd_suio;
                        }
                    }
                }
            }
            //i2c->fd = i2c->fd_cp2112;
#endif
#if 0
            check_funcs (i2c->fd, I2C_SMBUS_QUICK, 1);
            check_funcs (i2c->fd, I2C_SMBUS_BYTE, 1);
            check_funcs (i2c->fd, I2C_SMBUS_BYTE_DATA, 1);
            check_funcs (i2c->fd, I2C_SMBUS_WORD_DATA, 1);
            check_funcs (i2c->fd, I2C_SMBUS_BLOCK_DATA, 1);
            check_funcs (i2c->fd, I2C_SMBUS_I2C_BLOCK_DATA,
                         1);
#endif
            fprintf (stdout, "Master i2c-%d init done !\n",
                     bmc_i2c_bus);
            LOG_DEBUG ("Master i2c-%d init done !",
                       bmc_i2c_bus);
        }
    }

    return 0;
}

int bf_pltfm_master_i2c_de_init()
{
    struct i2c_ctx_t *i2c = &i2c_ctx;

    fprintf(stdout, "================== Deinit .... %48s ================== \n",
        __func__);

    if (global_come_type == COME_UNKNOWN) {
        return 0;
    } else {
        if (is_CG15XX()) {
            if (bf_cgos_de_init ()) {
                fprintf (stdout, "Deinit Master i2c\n");
            }
        } else {
            if (i2c->fd_suio > 0) {
                close (i2c->fd_suio);
            }
            if (i2c->fd_cp2112 > 0) {
                close (i2c->fd_cp2112);
            }
        }
    }

    bf_sys_rmutex_del (&master_i2c_lock);
    fprintf(stdout, "================== Deinit done %48s ================== \n",
        __func__);

    return 0;
}

