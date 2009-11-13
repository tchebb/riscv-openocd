/***************************************************************************
 *   Copyright (C) 2007 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Partially based on drivers/mtd/nand_ids.c from Linux.                 *
 *   Copyright (C) 2002 Thomas Gleixner <tglx@linutronix.de>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "nand.h"
#include "time_support.h"
#include "fileio.h"

static int nand_read_page(struct nand_device_s *nand, uint32_t page, uint8_t *data, uint32_t data_size, uint8_t *oob, uint32_t oob_size);
//static int nand_read_plain(struct nand_device_s *nand, uint32_t address, uint8_t *data, uint32_t data_size);

static int nand_write_page(struct nand_device_s *nand, uint32_t page, uint8_t *data, uint32_t data_size, uint8_t *oob, uint32_t oob_size);

/* NAND flash controller
 */
extern struct nand_flash_controller davinci_nand_controller;
extern struct nand_flash_controller lpc3180_nand_controller;
extern struct nand_flash_controller orion_nand_controller;
extern struct nand_flash_controller s3c2410_nand_controller;
extern struct nand_flash_controller s3c2412_nand_controller;
extern struct nand_flash_controller s3c2440_nand_controller;
extern struct nand_flash_controller s3c2443_nand_controller;
extern struct nand_flash_controller imx31_nand_flash_controller;

/* extern struct nand_flash_controller boundary_scan_nand_controller; */

static struct nand_flash_controller *nand_flash_controllers[] =
{
	&davinci_nand_controller,
	&lpc3180_nand_controller,
	&orion_nand_controller,
	&s3c2410_nand_controller,
	&s3c2412_nand_controller,
	&s3c2440_nand_controller,
	&s3c2443_nand_controller,
 	&imx31_nand_flash_controller,
/*	&boundary_scan_nand_controller, */
	NULL
};

/* configured NAND devices and NAND Flash command handler */
static nand_device_t *nand_devices = NULL;
static command_t *nand_cmd;

/*	Chip ID list
 *
 *	Name, ID code, pagesize, chipsize in MegaByte, eraseblock size,
 *	options
 *
 *	Pagesize; 0, 256, 512
 *	0	get this information from the extended chip ID
 *	256	256 Byte page size
 *	512	512 Byte page size
 */
static nand_info_t nand_flash_ids[] =
{
	/* start "museum" IDs */
	{"NAND 1MiB 5V 8-bit",		0x6e, 256, 1, 0x1000, 0},
	{"NAND 2MiB 5V 8-bit",		0x64, 256, 2, 0x1000, 0},
	{"NAND 4MiB 5V 8-bit",		0x6b, 512, 4, 0x2000, 0},
	{"NAND 1MiB 3,3V 8-bit",	0xe8, 256, 1, 0x1000, 0},
	{"NAND 1MiB 3,3V 8-bit",	0xec, 256, 1, 0x1000, 0},
	{"NAND 2MiB 3,3V 8-bit",	0xea, 256, 2, 0x1000, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xd5, 512, 4, 0x2000, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xe3, 512, 4, 0x2000, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xe5, 512, 4, 0x2000, 0},
	{"NAND 8MiB 3,3V 8-bit",	0xd6, 512, 8, 0x2000, 0},

	{"NAND 8MiB 1,8V 8-bit",	0x39, 512, 8, 0x2000, 0},
	{"NAND 8MiB 3,3V 8-bit",	0xe6, 512, 8, 0x2000, 0},
	{"NAND 8MiB 1,8V 16-bit",	0x49, 512, 8, 0x2000, NAND_BUSWIDTH_16},
	{"NAND 8MiB 3,3V 16-bit",	0x59, 512, 8, 0x2000, NAND_BUSWIDTH_16},
	/* end "museum" IDs */

	{"NAND 16MiB 1,8V 8-bit",	0x33, 512, 16, 0x4000, 0},
	{"NAND 16MiB 3,3V 8-bit",	0x73, 512, 16, 0x4000, 0},
	{"NAND 16MiB 1,8V 16-bit",	0x43, 512, 16, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 16MiB 3,3V 16-bit",	0x53, 512, 16, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 32MiB 1,8V 8-bit",	0x35, 512, 32, 0x4000, 0},
	{"NAND 32MiB 3,3V 8-bit",	0x75, 512, 32, 0x4000, 0},
	{"NAND 32MiB 1,8V 16-bit",	0x45, 512, 32, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 32MiB 3,3V 16-bit",	0x55, 512, 32, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 64MiB 1,8V 8-bit",	0x36, 512, 64, 0x4000, 0},
	{"NAND 64MiB 3,3V 8-bit",	0x76, 512, 64, 0x4000, 0},
	{"NAND 64MiB 1,8V 16-bit",	0x46, 512, 64, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 64MiB 3,3V 16-bit",	0x56, 512, 64, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 128MiB 1,8V 8-bit",	0x78, 512, 128, 0x4000, 0},
	{"NAND 128MiB 1,8V 8-bit",	0x39, 512, 128, 0x4000, 0},
	{"NAND 128MiB 3,3V 8-bit",	0x79, 512, 128, 0x4000, 0},
	{"NAND 128MiB 1,8V 16-bit",	0x72, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 1,8V 16-bit",	0x49, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 3,3V 16-bit",	0x74, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 3,3V 16-bit",	0x59, 512, 128, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 256MiB 3,3V 8-bit",	0x71, 512, 256, 0x4000, 0},

	{"NAND 64MiB 1,8V 8-bit",	0xA2, 0,  64, 0, LP_OPTIONS},
	{"NAND 64MiB 3,3V 8-bit",	0xF2, 0,  64, 0, LP_OPTIONS},
	{"NAND 64MiB 1,8V 16-bit",	0xB2, 0,  64, 0, LP_OPTIONS16},
	{"NAND 64MiB 3,3V 16-bit",	0xC2, 0,  64, 0, LP_OPTIONS16},

	{"NAND 128MiB 1,8V 8-bit",	0xA1, 0, 128, 0, LP_OPTIONS},
	{"NAND 128MiB 3,3V 8-bit",	0xF1, 0, 128, 0, LP_OPTIONS},
	{"NAND 128MiB 1,8V 16-bit",	0xB1, 0, 128, 0, LP_OPTIONS16},
	{"NAND 128MiB 3,3V 16-bit",	0xC1, 0, 128, 0, LP_OPTIONS16},

	{"NAND 256MiB 1,8V 8-bit",	0xAA, 0, 256, 0, LP_OPTIONS},
	{"NAND 256MiB 3,3V 8-bit",	0xDA, 0, 256, 0, LP_OPTIONS},
	{"NAND 256MiB 1,8V 16-bit",	0xBA, 0, 256, 0, LP_OPTIONS16},
	{"NAND 256MiB 3,3V 16-bit",	0xCA, 0, 256, 0, LP_OPTIONS16},

	{"NAND 512MiB 1,8V 8-bit",	0xAC, 0, 512, 0, LP_OPTIONS},
	{"NAND 512MiB 3,3V 8-bit",	0xDC, 0, 512, 0, LP_OPTIONS},
	{"NAND 512MiB 1,8V 16-bit",	0xBC, 0, 512, 0, LP_OPTIONS16},
	{"NAND 512MiB 3,3V 16-bit",	0xCC, 0, 512, 0, LP_OPTIONS16},

	{"NAND 1GiB 1,8V 8-bit",	0xA3, 0, 1024, 0, LP_OPTIONS},
	{"NAND 1GiB 3,3V 8-bit",	0xD3, 0, 1024, 0, LP_OPTIONS},
	{"NAND 1GiB 1,8V 16-bit",	0xB3, 0, 1024, 0, LP_OPTIONS16},
	{"NAND 1GiB 3,3V 16-bit",	0xC3, 0, 1024, 0, LP_OPTIONS16},

	{"NAND 2GiB 1,8V 8-bit",	0xA5, 0, 2048, 0, LP_OPTIONS},
	{"NAND 2GiB 3,3V 8-bit",	0xD5, 0, 2048, 0, LP_OPTIONS},
	{"NAND 2GiB 1,8V 16-bit",	0xB5, 0, 2048, 0, LP_OPTIONS16},
	{"NAND 2GiB 3,3V 16-bit",	0xC5, 0, 2048, 0, LP_OPTIONS16},

	{NULL, 0, 0, 0, 0, 0 }
};

/* Manufacturer ID list
 */
static nand_manufacturer_t nand_manuf_ids[] =
{
	{0x0, "unknown"},
	{NAND_MFR_TOSHIBA, "Toshiba"},
	{NAND_MFR_SAMSUNG, "Samsung"},
	{NAND_MFR_FUJITSU, "Fujitsu"},
	{NAND_MFR_NATIONAL, "National"},
	{NAND_MFR_RENESAS, "Renesas"},
	{NAND_MFR_STMICRO, "ST Micro"},
	{NAND_MFR_HYNIX, "Hynix"},
	{NAND_MFR_MICRON, "Micron"},
	{0x0, NULL},
};

/*
 * Define default oob placement schemes for large and small page devices
 */

#if 0
static nand_ecclayout_t nand_oob_8 = {
	.eccbytes = 3,
	.eccpos = {0, 1, 2},
	.oobfree = {
		{.offset = 3,
		 .length = 2},
		{.offset = 6,
		 .length = 2}}
};
#endif

static nand_ecclayout_t nand_oob_16 = {
	.eccbytes = 6,
	.eccpos = {0, 1, 2, 3, 6, 7},
	.oobfree = {
		{.offset = 8,
		 . length = 8}}
};

static nand_ecclayout_t nand_oob_64 = {
	.eccbytes = 24,
	.eccpos = {
		   40, 41, 42, 43, 44, 45, 46, 47,
		   48, 49, 50, 51, 52, 53, 54, 55,
		   56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {
		{.offset = 2,
		 .length = 38}}
};

/* nand device <nand_controller> [controller options]
 */
COMMAND_HANDLER(handle_nand_device_command)
{
	int i;
	int retval;

	if (argc < 1)
	{
		LOG_WARNING("incomplete flash device nand configuration");
		return ERROR_FLASH_BANK_INVALID;
	}

	for (i = 0; nand_flash_controllers[i]; i++)
	{
		nand_device_t *p, *c;

		if (strcmp(args[0], nand_flash_controllers[i]->name) == 0)
		{
			/* register flash specific commands */
			if ((retval = nand_flash_controllers[i]->register_commands(cmd_ctx)) != ERROR_OK)
			{
				LOG_ERROR("couldn't register '%s' commands", args[0]);
				return retval;
			}

			c = malloc(sizeof(nand_device_t));

			c->controller = nand_flash_controllers[i];
			c->controller_priv = NULL;
			c->manufacturer = NULL;
			c->device = NULL;
			c->bus_width = 0;
			c->address_cycles = 0;
			c->page_size = 0;
			c->use_raw = 0;
			c->next = NULL;

			retval = CALL_COMMAND_HANDLER(nand_flash_controllers[i]->nand_device_command, c);
			if (ERROR_OK != retval)
			{
				LOG_ERROR("'%s' driver rejected nand flash", c->controller->name);
				free(c);
				return ERROR_OK;
			}

			/* put NAND device in linked list */
			if (nand_devices)
			{
				/* find last flash device */
				for (p = nand_devices; p && p->next; p = p->next);
				if (p)
					p->next = c;
			}
			else
			{
				nand_devices = c;
			}

			return ERROR_OK;
		}
	}

	/* no valid NAND controller was found (i.e. the configuration option,
	 * didn't match one of the compiled-in controllers)
	 */
	LOG_ERROR("No valid NAND flash controller found (%s)", args[0]);
	LOG_ERROR("compiled-in NAND flash controllers:");
	for (i = 0; nand_flash_controllers[i]; i++)
	{
		LOG_ERROR("%i: %s", i, nand_flash_controllers[i]->name);
	}

	return ERROR_OK;
}

int nand_register_commands(struct command_context_s *cmd_ctx)
{
	nand_cmd = register_command(cmd_ctx, NULL, "nand", NULL, COMMAND_ANY, "NAND specific commands");

	register_command(cmd_ctx, nand_cmd, "device", handle_nand_device_command, COMMAND_CONFIG, NULL);

	return ERROR_OK;
}

nand_device_t *get_nand_device_by_num(int num)
{
	nand_device_t *p;
	int i = 0;

	for (p = nand_devices; p; p = p->next)
	{
		if (i++ == num)
		{
			return p;
		}
	}

	return NULL;
}

int nand_command_get_device_by_num(struct command_context_s *cmd_ctx,
		const char *str, nand_device_t **nand)
{
	unsigned num;
	COMMAND_PARSE_NUMBER(uint, str, num);
	*nand = get_nand_device_by_num(num);
	if (!*nand) {
		command_print(cmd_ctx, "NAND flash device '#%s' is out of bounds", str);
		return ERROR_INVALID_ARGUMENTS;
	}
	return ERROR_OK;
}

static int nand_build_bbt(struct nand_device_s *nand, int first, int last)
{
	uint32_t page = 0x0;
	int i;
	uint8_t oob[6];

	if ((first < 0) || (first >= nand->num_blocks))
		first = 0;

	if ((last >= nand->num_blocks) || (last == -1))
		last = nand->num_blocks - 1;

	for (i = first; i < last; i++)
	{
		nand_read_page(nand, page, NULL, 0, oob, 6);

		if (((nand->device->options & NAND_BUSWIDTH_16) && ((oob[0] & oob[1]) != 0xff))
			|| (((nand->page_size == 512) && (oob[5] != 0xff)) ||
				((nand->page_size == 2048) && (oob[0] != 0xff))))
		{
			LOG_WARNING("bad block: %i", i);
			nand->blocks[i].is_bad = 1;
		}
		else
		{
			nand->blocks[i].is_bad = 0;
		}

		page += (nand->erase_size / nand->page_size);
	}

	return ERROR_OK;
}

int nand_read_status(struct nand_device_s *nand, uint8_t *status)
{
	if (!nand->device)
		return ERROR_NAND_DEVICE_NOT_PROBED;

	/* Send read status command */
	nand->controller->command(nand, NAND_CMD_STATUS);

	alive_sleep(1);

	/* read status */
	if (nand->device->options & NAND_BUSWIDTH_16)
	{
		uint16_t data;
		nand->controller->read_data(nand, &data);
		*status = data & 0xff;
	}
	else
	{
		nand->controller->read_data(nand, status);
	}

	return ERROR_OK;
}

static int nand_poll_ready(struct nand_device_s *nand, int timeout)
{
	uint8_t status;

	nand->controller->command(nand, NAND_CMD_STATUS);
	do {
		if (nand->device->options & NAND_BUSWIDTH_16) {
			uint16_t data;
			nand->controller->read_data(nand, &data);
			status = data & 0xff;
		} else {
			nand->controller->read_data(nand, &status);
		}
		if (status & NAND_STATUS_READY)
			break;
		alive_sleep(1);
	} while (timeout--);

	return (status & NAND_STATUS_READY) != 0;
}

int nand_probe(struct nand_device_s *nand)
{
	uint8_t manufacturer_id, device_id;
	uint8_t id_buff[6];
	int retval;
	int i;

	/* clear device data */
	nand->device = NULL;
	nand->manufacturer = NULL;

	/* clear device parameters */
	nand->bus_width = 0;
	nand->address_cycles = 0;
	nand->page_size = 0;
	nand->erase_size = 0;

	/* initialize controller (device parameters are zero, use controller default) */
	if ((retval = nand->controller->init(nand) != ERROR_OK))
	{
		switch (retval)
		{
			case ERROR_NAND_OPERATION_FAILED:
				LOG_DEBUG("controller initialization failed");
				return ERROR_NAND_OPERATION_FAILED;
			case ERROR_NAND_OPERATION_NOT_SUPPORTED:
				LOG_ERROR("BUG: controller reported that it doesn't support default parameters");
				return ERROR_NAND_OPERATION_FAILED;
			default:
				LOG_ERROR("BUG: unknown controller initialization failure");
				return ERROR_NAND_OPERATION_FAILED;
		}
	}

	nand->controller->command(nand, NAND_CMD_RESET);
	nand->controller->reset(nand);

	nand->controller->command(nand, NAND_CMD_READID);
	nand->controller->address(nand, 0x0);

	if (nand->bus_width == 8)
	{
		nand->controller->read_data(nand, &manufacturer_id);
		nand->controller->read_data(nand, &device_id);
	}
	else
	{
		uint16_t data_buf;
		nand->controller->read_data(nand, &data_buf);
		manufacturer_id = data_buf & 0xff;
		nand->controller->read_data(nand, &data_buf);
		device_id = data_buf & 0xff;
	}

	for (i = 0; nand_flash_ids[i].name; i++)
	{
		if (nand_flash_ids[i].id == device_id)
		{
			nand->device = &nand_flash_ids[i];
			break;
		}
	}

	for (i = 0; nand_manuf_ids[i].name; i++)
	{
		if (nand_manuf_ids[i].id == manufacturer_id)
		{
			nand->manufacturer = &nand_manuf_ids[i];
			break;
		}
	}

	if (!nand->manufacturer)
	{
		nand->manufacturer = &nand_manuf_ids[0];
		nand->manufacturer->id = manufacturer_id;
	}

	if (!nand->device)
	{
		LOG_ERROR("unknown NAND flash device found, manufacturer id: 0x%2.2x device id: 0x%2.2x",
			manufacturer_id, device_id);
		return ERROR_NAND_OPERATION_FAILED;
	}

	LOG_DEBUG("found %s (%s)", nand->device->name, nand->manufacturer->name);

	/* initialize device parameters */

	/* bus width */
	if (nand->device->options & NAND_BUSWIDTH_16)
		nand->bus_width = 16;
	else
		nand->bus_width = 8;

	/* Do we need extended device probe information? */
	if (nand->device->page_size == 0 ||
	    nand->device->erase_size == 0)
	{
		if (nand->bus_width == 8)
		{
			nand->controller->read_data(nand, id_buff + 3);
			nand->controller->read_data(nand, id_buff + 4);
			nand->controller->read_data(nand, id_buff + 5);
		}
		else
		{
			uint16_t data_buf;

			nand->controller->read_data(nand, &data_buf);
			id_buff[3] = data_buf;

			nand->controller->read_data(nand, &data_buf);
			id_buff[4] = data_buf;

			nand->controller->read_data(nand, &data_buf);
			id_buff[5] = data_buf >> 8;
		}
	}

	/* page size */
	if (nand->device->page_size == 0)
	{
		nand->page_size = 1 << (10 + (id_buff[4] & 3));
	}
	else if (nand->device->page_size == 256)
	{
		LOG_ERROR("NAND flashes with 256 byte pagesize are not supported");
		return ERROR_NAND_OPERATION_FAILED;
	}
	else
	{
		nand->page_size = nand->device->page_size;
	}

	/* number of address cycles */
	if (nand->page_size <= 512)
	{
		/* small page devices */
		if (nand->device->chip_size <= 32)
			nand->address_cycles = 3;
		else if (nand->device->chip_size <= 8*1024)
			nand->address_cycles = 4;
		else
		{
			LOG_ERROR("BUG: small page NAND device with more than 8 GiB encountered");
			nand->address_cycles = 5;
		}
	}
	else
	{
		/* large page devices */
		if (nand->device->chip_size <= 128)
			nand->address_cycles = 4;
		else if (nand->device->chip_size <= 32*1024)
			nand->address_cycles = 5;
		else
		{
			LOG_ERROR("BUG: large page NAND device with more than 32 GiB encountered");
			nand->address_cycles = 6;
		}
	}

	/* erase size */
	if (nand->device->erase_size == 0)
	{
		switch ((id_buff[4] >> 4) & 3) {
		case 0:
			nand->erase_size = 64 << 10;
			break;
		case 1:
			nand->erase_size = 128 << 10;
			break;
		case 2:
			nand->erase_size = 256 << 10;
			break;
		case 3:
			nand->erase_size =512 << 10;
			break;
		}
	}
	else
	{
		nand->erase_size = nand->device->erase_size;
	}

	/* initialize controller, but leave parameters at the controllers default */
	if ((retval = nand->controller->init(nand) != ERROR_OK))
	{
		switch (retval)
		{
			case ERROR_NAND_OPERATION_FAILED:
				LOG_DEBUG("controller initialization failed");
				return ERROR_NAND_OPERATION_FAILED;
			case ERROR_NAND_OPERATION_NOT_SUPPORTED:
				LOG_ERROR("controller doesn't support requested parameters (buswidth: %i, address cycles: %i, page size: %i)",
					nand->bus_width, nand->address_cycles, nand->page_size);
				return ERROR_NAND_OPERATION_FAILED;
			default:
				LOG_ERROR("BUG: unknown controller initialization failure");
				return ERROR_NAND_OPERATION_FAILED;
		}
	}

	nand->num_blocks = (nand->device->chip_size * 1024) / (nand->erase_size / 1024);
	nand->blocks = malloc(sizeof(nand_block_t) * nand->num_blocks);

	for (i = 0; i < nand->num_blocks; i++)
	{
		nand->blocks[i].size = nand->erase_size;
		nand->blocks[i].offset = i * nand->erase_size;
		nand->blocks[i].is_erased = -1;
		nand->blocks[i].is_bad = -1;
	}

	return ERROR_OK;
}

static int nand_erase(struct nand_device_s *nand, int first_block, int last_block)
{
	int i;
	uint32_t page;
	uint8_t status;
	int retval;

	if (!nand->device)
		return ERROR_NAND_DEVICE_NOT_PROBED;

	if ((first_block < 0) || (last_block > nand->num_blocks))
		return ERROR_INVALID_ARGUMENTS;

	/* make sure we know if a block is bad before erasing it */
	for (i = first_block; i <= last_block; i++)
	{
		if (nand->blocks[i].is_bad == -1)
		{
			nand_build_bbt(nand, i, last_block);
			break;
		}
	}

	for (i = first_block; i <= last_block; i++)
	{
		/* Send erase setup command */
		nand->controller->command(nand, NAND_CMD_ERASE1);

		page = i * (nand->erase_size / nand->page_size);

		/* Send page address */
		if (nand->page_size <= 512)
		{
			/* row */
			nand->controller->address(nand, page & 0xff);
			nand->controller->address(nand, (page >> 8) & 0xff);

			/* 3rd cycle only on devices with more than 32 MiB */
			if (nand->address_cycles >= 4)
				nand->controller->address(nand, (page >> 16) & 0xff);

			/* 4th cycle only on devices with more than 8 GiB */
			if (nand->address_cycles >= 5)
				nand->controller->address(nand, (page >> 24) & 0xff);
		}
		else
		{
			/* row */
			nand->controller->address(nand, page & 0xff);
			nand->controller->address(nand, (page >> 8) & 0xff);

			/* 3rd cycle only on devices with more than 128 MiB */
			if (nand->address_cycles >= 5)
				nand->controller->address(nand, (page >> 16) & 0xff);
		}

		/* Send erase confirm command */
		nand->controller->command(nand, NAND_CMD_ERASE2);

		retval = nand->controller->nand_ready ?
				nand->controller->nand_ready(nand, 1000) :
				nand_poll_ready(nand, 1000);
		if (!retval) {
			LOG_ERROR("timeout waiting for NAND flash block erase to complete");
			return ERROR_NAND_OPERATION_TIMEOUT;
		}

		if ((retval = nand_read_status(nand, &status)) != ERROR_OK)
		{
			LOG_ERROR("couldn't read status");
			return ERROR_NAND_OPERATION_FAILED;
		}

		if (status & 0x1)
		{
			LOG_ERROR("didn't erase %sblock %d; status: 0x%2.2x",
					(nand->blocks[i].is_bad == 1)
						? "bad " : "",
					i, status);
			/* continue; other blocks might still be erasable */
		}

		nand->blocks[i].is_erased = 1;
	}

	return ERROR_OK;
}

#if 0
static int nand_read_plain(struct nand_device_s *nand, uint32_t address, uint8_t *data, uint32_t data_size)
{
	uint8_t *page;

	if (!nand->device)
		return ERROR_NAND_DEVICE_NOT_PROBED;

	if (address % nand->page_size)
	{
		LOG_ERROR("reads need to be page aligned");
		return ERROR_NAND_OPERATION_FAILED;
	}

	page = malloc(nand->page_size);

	while (data_size > 0)
	{
		uint32_t thisrun_size = (data_size > nand->page_size) ? nand->page_size : data_size;
		uint32_t page_address;


		page_address = address / nand->page_size;

		nand_read_page(nand, page_address, page, nand->page_size, NULL, 0);

		memcpy(data, page, thisrun_size);

		address += thisrun_size;
		data += thisrun_size;
		data_size -= thisrun_size;
	}

	free(page);

	return ERROR_OK;
}

static int nand_write_plain(struct nand_device_s *nand, uint32_t address, uint8_t *data, uint32_t data_size)
{
	uint8_t *page;

	if (!nand->device)
		return ERROR_NAND_DEVICE_NOT_PROBED;

	if (address % nand->page_size)
	{
		LOG_ERROR("writes need to be page aligned");
		return ERROR_NAND_OPERATION_FAILED;
	}

	page = malloc(nand->page_size);

	while (data_size > 0)
	{
		uint32_t thisrun_size = (data_size > nand->page_size) ? nand->page_size : data_size;
		uint32_t page_address;

		memset(page, 0xff, nand->page_size);
		memcpy(page, data, thisrun_size);

		page_address = address / nand->page_size;

		nand_write_page(nand, page_address, page, nand->page_size, NULL, 0);

		address += thisrun_size;
		data += thisrun_size;
		data_size -= thisrun_size;
	}

	free(page);

	return ERROR_OK;
}
#endif

int nand_write_page(struct nand_device_s *nand, uint32_t page, uint8_t *data, uint32_t data_size, uint8_t *oob, uint32_t oob_size)
{
	uint32_t block;

	if (!nand->device)
		return ERROR_NAND_DEVICE_NOT_PROBED;

	block = page / (nand->erase_size / nand->page_size);
	if (nand->blocks[block].is_erased == 1)
		nand->blocks[block].is_erased = 0;

	if (nand->use_raw || nand->controller->write_page == NULL)
		return nand_write_page_raw(nand, page, data, data_size, oob, oob_size);
	else
		return nand->controller->write_page(nand, page, data, data_size, oob, oob_size);
}

static int nand_read_page(struct nand_device_s *nand, uint32_t page, uint8_t *data, uint32_t data_size, uint8_t *oob, uint32_t oob_size)
{
	if (!nand->device)
		return ERROR_NAND_DEVICE_NOT_PROBED;

	if (nand->use_raw || nand->controller->read_page == NULL)
		return nand_read_page_raw(nand, page, data, data_size, oob, oob_size);
	else
		return nand->controller->read_page(nand, page, data, data_size, oob, oob_size);
}

int nand_read_page_raw(struct nand_device_s *nand, uint32_t page, uint8_t *data, uint32_t data_size, uint8_t *oob, uint32_t oob_size)
{
	uint32_t i;

	if (!nand->device)
		return ERROR_NAND_DEVICE_NOT_PROBED;

	if (nand->page_size <= 512)
	{
		/* small page device */
		if (data)
			nand->controller->command(nand, NAND_CMD_READ0);
		else
			nand->controller->command(nand, NAND_CMD_READOOB);

		/* column (always 0, we start at the beginning of a page/OOB area) */
		nand->controller->address(nand, 0x0);

		/* row */
		nand->controller->address(nand, page & 0xff);
		nand->controller->address(nand, (page >> 8) & 0xff);

		/* 4th cycle only on devices with more than 32 MiB */
		if (nand->address_cycles >= 4)
			nand->controller->address(nand, (page >> 16) & 0xff);

		/* 5th cycle only on devices with more than 8 GiB */
		if (nand->address_cycles >= 5)
			nand->controller->address(nand, (page >> 24) & 0xff);
	}
	else
	{
		/* large page device */
		nand->controller->command(nand, NAND_CMD_READ0);

		/* column (0 when we start at the beginning of a page,
		 * or 2048 for the beginning of OOB area)
		 */
		nand->controller->address(nand, 0x0);
		if (data)
			nand->controller->address(nand, 0x0);
		else
			nand->controller->address(nand, 0x8);

		/* row */
		nand->controller->address(nand, page & 0xff);
		nand->controller->address(nand, (page >> 8) & 0xff);

		/* 5th cycle only on devices with more than 128 MiB */
		if (nand->address_cycles >= 5)
			nand->controller->address(nand, (page >> 16) & 0xff);

		/* large page devices need a start command */
		nand->controller->command(nand, NAND_CMD_READSTART);
	}

	if (nand->controller->nand_ready) {
		if (!nand->controller->nand_ready(nand, 100))
			return ERROR_NAND_OPERATION_TIMEOUT;
	} else {
		alive_sleep(1);
	}

	if (data)
	{
		if (nand->controller->read_block_data != NULL)
			(nand->controller->read_block_data)(nand, data, data_size);
		else
		{
			for (i = 0; i < data_size;)
			{
				if (nand->device->options & NAND_BUSWIDTH_16)
				{
					nand->controller->read_data(nand, data);
					data += 2;
					i += 2;
				}
				else
				{
					nand->controller->read_data(nand, data);
					data += 1;
					i += 1;
				}
			}
		}
	}

	if (oob)
	{
		if (nand->controller->read_block_data != NULL)
			(nand->controller->read_block_data)(nand, oob, oob_size);
		else
		{
			for (i = 0; i < oob_size;)
			{
				if (nand->device->options & NAND_BUSWIDTH_16)
				{
					nand->controller->read_data(nand, oob);
					oob += 2;
					i += 2;
				}
				else
				{
					nand->controller->read_data(nand, oob);
					oob += 1;
					i += 1;
				}
			}
		}
	}

	return ERROR_OK;
}

int nand_write_page_raw(struct nand_device_s *nand, uint32_t page, uint8_t *data, uint32_t data_size, uint8_t *oob, uint32_t oob_size)
{
	uint32_t i;
	int retval;
	uint8_t status;

	if (!nand->device)
		return ERROR_NAND_DEVICE_NOT_PROBED;

	nand->controller->command(nand, NAND_CMD_SEQIN);

	if (nand->page_size <= 512)
	{
		/* column (always 0, we start at the beginning of a page/OOB area) */
		nand->controller->address(nand, 0x0);

		/* row */
		nand->controller->address(nand, page & 0xff);
		nand->controller->address(nand, (page >> 8) & 0xff);

		/* 4th cycle only on devices with more than 32 MiB */
		if (nand->address_cycles >= 4)
			nand->controller->address(nand, (page >> 16) & 0xff);

		/* 5th cycle only on devices with more than 8 GiB */
		if (nand->address_cycles >= 5)
			nand->controller->address(nand, (page >> 24) & 0xff);
	}
	else
	{
		/* column (0 when we start at the beginning of a page,
		 * or 2048 for the beginning of OOB area)
		 */
		nand->controller->address(nand, 0x0);
		if (data)
			nand->controller->address(nand, 0x0);
		else
			nand->controller->address(nand, 0x8);

		/* row */
		nand->controller->address(nand, page & 0xff);
		nand->controller->address(nand, (page >> 8) & 0xff);

		/* 5th cycle only on devices with more than 128 MiB */
		if (nand->address_cycles >= 5)
			nand->controller->address(nand, (page >> 16) & 0xff);
	}

	if (data)
	{
		if (nand->controller->write_block_data != NULL)
			(nand->controller->write_block_data)(nand, data, data_size);
		else
		{
			for (i = 0; i < data_size;)
			{
				if (nand->device->options & NAND_BUSWIDTH_16)
				{
					uint16_t data_buf = le_to_h_u16(data);
					nand->controller->write_data(nand, data_buf);
					data += 2;
					i += 2;
				}
				else
				{
					nand->controller->write_data(nand, *data);
					data += 1;
					i += 1;
				}
			}
		}
	}

	if (oob)
	{
		if (nand->controller->write_block_data != NULL)
			(nand->controller->write_block_data)(nand, oob, oob_size);
		else
		{
			for (i = 0; i < oob_size;)
			{
				if (nand->device->options & NAND_BUSWIDTH_16)
				{
					uint16_t oob_buf = le_to_h_u16(data);
					nand->controller->write_data(nand, oob_buf);
					oob += 2;
					i += 2;
				}
				else
				{
					nand->controller->write_data(nand, *oob);
					oob += 1;
					i += 1;
				}
			}
		}
	}

	nand->controller->command(nand, NAND_CMD_PAGEPROG);

	retval = nand->controller->nand_ready ?
			nand->controller->nand_ready(nand, 100) :
			nand_poll_ready(nand, 100);
	if (!retval)
		return ERROR_NAND_OPERATION_TIMEOUT;

	if ((retval = nand_read_status(nand, &status)) != ERROR_OK)
	{
		LOG_ERROR("couldn't read status");
		return ERROR_NAND_OPERATION_FAILED;
	}

	if (status & NAND_STATUS_FAIL)
	{
		LOG_ERROR("write operation didn't pass, status: 0x%2.2x", status);
		return ERROR_NAND_OPERATION_FAILED;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_nand_list_command)
{
	nand_device_t *p;
	int i;

	if (!nand_devices)
	{
		command_print(cmd_ctx, "no NAND flash devices configured");
		return ERROR_OK;
	}

	for (p = nand_devices, i = 0; p; p = p->next, i++)
	{
		if (p->device)
			command_print(cmd_ctx, "#%i: %s (%s) "
				"pagesize: %i, buswidth: %i,\n\t"
				"blocksize: %i, blocks: %i",
				i, p->device->name, p->manufacturer->name,
				p->page_size, p->bus_width,
				p->erase_size, p->num_blocks);
		else
			command_print(cmd_ctx, "#%i: not probed", i);
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_nand_info_command)
{
	int i = 0;
	int j = 0;
	int first = -1;
	int last = -1;

	nand_device_t *p;
	int retval = nand_command_get_device_by_num(cmd_ctx, args[0], &p);
	if (ERROR_OK != retval)
		return retval;

	switch (argc) {
	default:
		return ERROR_COMMAND_SYNTAX_ERROR;
	case 1:
		first = 0;
		last = INT32_MAX;
		break;
	case 2:
		COMMAND_PARSE_NUMBER(int, args[1], i);
		first = last = i;
		i = 0;
		break;
	case 3:
		COMMAND_PARSE_NUMBER(int, args[1], first);
		COMMAND_PARSE_NUMBER(int, args[2], last);
		break;
	}

	if (NULL == p->device)
	{
		command_print(cmd_ctx, "#%s: not probed", args[0]);
		return ERROR_OK;
	}

	if (first >= p->num_blocks)
		first = p->num_blocks - 1;

	if (last >= p->num_blocks)
		last = p->num_blocks - 1;

	command_print(cmd_ctx, "#%i: %s (%s) pagesize: %i, buswidth: %i, erasesize: %i",
		i++, p->device->name, p->manufacturer->name, p->page_size, p->bus_width, p->erase_size);

	for (j = first; j <= last; j++)
	{
		char *erase_state, *bad_state;

		if (p->blocks[j].is_erased == 0)
			erase_state = "not erased";
		else if (p->blocks[j].is_erased == 1)
			erase_state = "erased";
		else
			erase_state = "erase state unknown";

		if (p->blocks[j].is_bad == 0)
			bad_state = "";
		else if (p->blocks[j].is_bad == 1)
			bad_state = " (marked bad)";
		else
			bad_state = " (block condition unknown)";

		command_print(cmd_ctx,
			      "\t#%i: 0x%8.8" PRIx32 " (%" PRId32 "kB) %s%s",
			      j,
			      p->blocks[j].offset,
			      p->blocks[j].size / 1024,
			      erase_state,
			      bad_state);
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_nand_probe_command)
{
	if (argc != 1)
	{
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	nand_device_t *p;
	int retval = nand_command_get_device_by_num(cmd_ctx, args[0], &p);
	if (ERROR_OK != retval)
		return retval;

	if ((retval = nand_probe(p)) == ERROR_OK)
	{
		command_print(cmd_ctx, "NAND flash device '%s' found", p->device->name);
	}
	else if (retval == ERROR_NAND_OPERATION_FAILED)
	{
		command_print(cmd_ctx, "probing failed for NAND flash device");
	}
	else
	{
		command_print(cmd_ctx, "unknown error when probing NAND flash device");
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_nand_erase_command)
{
	if (argc != 1 && argc != 3)
	{
		return ERROR_COMMAND_SYNTAX_ERROR;

	}

	nand_device_t *p;
	int retval = nand_command_get_device_by_num(cmd_ctx, args[0], &p);
	if (ERROR_OK != retval)
		return retval;

	unsigned long offset;
	unsigned long length;

	/* erase specified part of the chip; or else everything */
	if (argc == 3) {
		unsigned long size = p->erase_size * p->num_blocks;

		COMMAND_PARSE_NUMBER(ulong, args[1], offset);
		if ((offset % p->erase_size) != 0 || offset >= size)
			return ERROR_INVALID_ARGUMENTS;

		COMMAND_PARSE_NUMBER(ulong, args[2], length);
		if ((length == 0) || (length % p->erase_size) != 0
				|| (length + offset) > size)
			return ERROR_INVALID_ARGUMENTS;

		offset /= p->erase_size;
		length /= p->erase_size;
	} else {
		offset = 0;
		length = p->num_blocks;
	}

	retval = nand_erase(p, offset, offset + length - 1);
	if (retval == ERROR_OK)
	{
		command_print(cmd_ctx, "erased blocks %lu to %lu "
				"on NAND flash device #%s '%s'",
				offset, offset + length,
				args[0], p->device->name);
	}
	else if (retval == ERROR_NAND_OPERATION_FAILED)
	{
		command_print(cmd_ctx, "erase failed");
	}
	else
	{
		command_print(cmd_ctx, "unknown error when erasing NAND flash device");
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_nand_check_bad_blocks_command)
{
	int first = -1;
	int last = -1;

	if ((argc < 1) || (argc > 3) || (argc == 2))
	{
		return ERROR_COMMAND_SYNTAX_ERROR;

	}

	nand_device_t *p;
	int retval = nand_command_get_device_by_num(cmd_ctx, args[0], &p);
	if (ERROR_OK != retval)
		return retval;

	if (argc == 3)
	{
		unsigned long offset;
		unsigned long length;

		COMMAND_PARSE_NUMBER(ulong, args[1], offset);
		if (offset % p->erase_size)
			return ERROR_INVALID_ARGUMENTS;
		offset /= p->erase_size;

		COMMAND_PARSE_NUMBER(ulong, args[2], length);
		if (length % p->erase_size)
			return ERROR_INVALID_ARGUMENTS;

		length -= 1;
		length /= p->erase_size;

		first = offset;
		last = offset + length;
	}

	retval = nand_build_bbt(p, first, last);
	if (retval == ERROR_OK)
	{
		command_print(cmd_ctx, "checked NAND flash device for bad blocks, "
				"use \"nand info\" command to list blocks");
	}
	else if (retval == ERROR_NAND_OPERATION_FAILED)
	{
		command_print(cmd_ctx, "error when checking for bad blocks on "
				"NAND flash device");
	}
	else
	{
		command_print(cmd_ctx, "unknown error when checking for bad "
				"blocks on NAND flash device");
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_nand_write_command)
{
	uint32_t offset;
	uint32_t binary_size;
	uint32_t buf_cnt;
	enum oob_formats oob_format = NAND_OOB_NONE;

	struct fileio fileio;


	if (argc < 3)
	{
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	nand_device_t *p;
	int retval = nand_command_get_device_by_num(cmd_ctx, args[0], &p);
	if (ERROR_OK != retval)
		return retval;

	uint8_t *page = NULL;
	uint32_t page_size = 0;
	uint8_t *oob = NULL;
	uint32_t oob_size = 0;
	const int *eccpos = NULL;

	COMMAND_PARSE_NUMBER(u32, args[2], offset);

	if (argc > 3)
	{
		for (unsigned i = 3; i < argc; i++)
		{
			if (!strcmp(args[i], "oob_raw"))
				oob_format |= NAND_OOB_RAW;
			else if (!strcmp(args[i], "oob_only"))
				oob_format |= NAND_OOB_RAW | NAND_OOB_ONLY;
			else if (!strcmp(args[i], "oob_softecc"))
				oob_format |= NAND_OOB_SW_ECC;
			else if (!strcmp(args[i], "oob_softecc_kw"))
				oob_format |= NAND_OOB_SW_ECC_KW;
			else
			{
				command_print(cmd_ctx, "unknown option: %s", args[i]);
				return ERROR_COMMAND_SYNTAX_ERROR;
			}
		}
	}

	struct duration bench;
	duration_start(&bench);

	if (fileio_open(&fileio, args[1], FILEIO_READ, FILEIO_BINARY) != ERROR_OK)
	{
		return ERROR_OK;
	}

	buf_cnt = binary_size = fileio.size;

	if (!(oob_format & NAND_OOB_ONLY))
	{
		page_size = p->page_size;
		page = malloc(p->page_size);
	}

	if (oob_format & (NAND_OOB_RAW | NAND_OOB_SW_ECC | NAND_OOB_SW_ECC_KW))
	{
		if (p->page_size == 512) {
			oob_size = 16;
			eccpos = nand_oob_16.eccpos;
		} else if (p->page_size == 2048) {
			oob_size = 64;
			eccpos = nand_oob_64.eccpos;
		}
		oob = malloc(oob_size);
	}

	if (offset % p->page_size)
	{
		command_print(cmd_ctx, "only page size aligned offsets and sizes are supported");
		fileio_close(&fileio);
		free(oob);
		free(page);
		return ERROR_OK;
	}

	while (buf_cnt > 0)
	{
		uint32_t size_read;

		if (NULL != page)
		{
			fileio_read(&fileio, page_size, page, &size_read);
			buf_cnt -= size_read;
			if (size_read < page_size)
			{
				memset(page + size_read, 0xff, page_size - size_read);
			}
		}

		if (oob_format & NAND_OOB_SW_ECC)
		{
			uint32_t i, j;
			uint8_t ecc[3];
			memset(oob, 0xff, oob_size);
			for (i = 0, j = 0; i < page_size; i += 256) {
				nand_calculate_ecc(p, page + i, ecc);
				oob[eccpos[j++]] = ecc[0];
				oob[eccpos[j++]] = ecc[1];
				oob[eccpos[j++]] = ecc[2];
			}
		} else if (oob_format & NAND_OOB_SW_ECC_KW)
		{
			/*
			 * In this case eccpos is not used as
			 * the ECC data is always stored contigously
			 * at the end of the OOB area.  It consists
			 * of 10 bytes per 512-byte data block.
			 */
			uint32_t i;
			uint8_t *ecc = oob + oob_size - page_size/512 * 10;
			memset(oob, 0xff, oob_size);
			for (i = 0; i < page_size; i += 512) {
				nand_calculate_ecc_kw(p, page + i, ecc);
				ecc += 10;
			}
		}
		else if (NULL != oob)
		{
			fileio_read(&fileio, oob_size, oob, &size_read);
			buf_cnt -= size_read;
			if (size_read < oob_size)
			{
				memset(oob + size_read, 0xff, oob_size - size_read);
			}
		}

		if (nand_write_page(p, offset / p->page_size, page, page_size, oob, oob_size) != ERROR_OK)
		{
			command_print(cmd_ctx, "failed writing file %s to NAND flash %s at offset 0x%8.8" PRIx32 "",
				args[1], args[0], offset);

			fileio_close(&fileio);
			free(oob);
			free(page);

			return ERROR_OK;
		}
		offset += page_size;
	}

	fileio_close(&fileio);
	free(oob);
	free(page);
	oob = NULL;
	page = NULL;
	if (duration_measure(&bench) == ERROR_OK)
	{
		command_print(cmd_ctx, "wrote file %s to NAND flash %s "
			"up to offset 0x%8.8" PRIx32 " in %fs (%0.3f kb/s)",
			args[1], args[0], offset, duration_elapsed(&bench),
			duration_kbps(&bench, fileio.size));
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_nand_dump_command)
{
	if (argc < 4)
	{
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	nand_device_t *p;
	int retval = nand_command_get_device_by_num(cmd_ctx, args[0], &p);
	if (ERROR_OK != retval)
		return retval;

	if (NULL == p->device)
	{
		command_print(cmd_ctx, "#%s: not probed", args[0]);
		return ERROR_OK;
	}

	struct fileio fileio;

	uint8_t *page = NULL;
	uint32_t page_size = 0;
	uint8_t *oob = NULL;
	uint32_t oob_size = 0;
	uint32_t address;
	COMMAND_PARSE_NUMBER(u32, args[2], address);
	uint32_t size;
	COMMAND_PARSE_NUMBER(u32, args[3], size);
	uint32_t bytes_done = 0;
	enum oob_formats oob_format = NAND_OOB_NONE;

	if (argc > 4)
	{
		for (unsigned i = 4; i < argc; i++)
		{
			if (!strcmp(args[i], "oob_raw"))
				oob_format |= NAND_OOB_RAW;
			else if (!strcmp(args[i], "oob_only"))
				oob_format |= NAND_OOB_RAW | NAND_OOB_ONLY;
			else
				command_print(cmd_ctx, "unknown option: '%s'", args[i]);
		}
	}

	if ((address % p->page_size) || (size % p->page_size))
	{
		command_print(cmd_ctx, "only page size aligned addresses and sizes are supported");
		return ERROR_OK;
	}

	if (!(oob_format & NAND_OOB_ONLY))
	{
		page_size = p->page_size;
		page = malloc(p->page_size);
	}

	if (oob_format & NAND_OOB_RAW)
	{
		if (p->page_size == 512)
			oob_size = 16;
		else if (p->page_size == 2048)
			oob_size = 64;
		oob = malloc(oob_size);
	}

	if (fileio_open(&fileio, args[1], FILEIO_WRITE, FILEIO_BINARY) != ERROR_OK)
	{
		return ERROR_OK;
	}

	struct duration bench;
	duration_start(&bench);

	while (size > 0)
	{
		uint32_t size_written;
		if ((retval = nand_read_page(p, address / p->page_size, page, page_size, oob, oob_size)) != ERROR_OK)
		{
			command_print(cmd_ctx, "reading NAND flash page failed");
			free(page);
			free(oob);
			fileio_close(&fileio);
			return ERROR_OK;
		}

		if (NULL != page)
		{
			fileio_write(&fileio, page_size, page, &size_written);
			bytes_done += page_size;
		}

		if (NULL != oob)
		{
			fileio_write(&fileio, oob_size, oob, &size_written);
			bytes_done += oob_size;
		}

		size -= p->page_size;
		address += p->page_size;
	}

	free(page);
	page = NULL;
	free(oob);
	oob = NULL;
	fileio_close(&fileio);

	if (duration_measure(&bench) == ERROR_OK)
	{
		command_print(cmd_ctx, "dumped %lld byte in %fs (%0.3f kb/s)",
			fileio.size, duration_elapsed(&bench),
			duration_kbps(&bench, fileio.size));
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_nand_raw_access_command)
{
	if ((argc < 1) || (argc > 2))
	{
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	nand_device_t *p;
	int retval = nand_command_get_device_by_num(cmd_ctx, args[0], &p);
	if (ERROR_OK != retval)
		return retval;

	if (NULL == p->device)
	{
		command_print(cmd_ctx, "#%s: not probed", args[0]);
		return ERROR_OK;
	}

	if (argc == 2)
	{
		if (strcmp("enable", args[1]) == 0)
			p->use_raw = 1;
		else if (strcmp("disable", args[1]) == 0)
			p->use_raw = 0;
		else
			return ERROR_COMMAND_SYNTAX_ERROR;
	}

	const char *msg = p->use_raw ? "enabled" : "disabled";
	command_print(cmd_ctx, "raw access is %s", msg);

	return ERROR_OK;
}

int nand_init(struct command_context_s *cmd_ctx)
{
	if (!nand_devices)
		return ERROR_OK;

	register_command(cmd_ctx, nand_cmd, "list",
			handle_nand_list_command, COMMAND_EXEC,
			"list configured NAND flash devices");
	register_command(cmd_ctx, nand_cmd, "info",
			handle_nand_info_command, COMMAND_EXEC,
			"print info about NAND flash device <num>");
	register_command(cmd_ctx, nand_cmd, "probe",
			handle_nand_probe_command, COMMAND_EXEC,
			"identify NAND flash device <num>");

	register_command(cmd_ctx, nand_cmd, "check_bad_blocks",
			handle_nand_check_bad_blocks_command, COMMAND_EXEC,
			"check NAND flash device <num> for bad blocks [<offset> <length>]");
	register_command(cmd_ctx, nand_cmd, "erase",
			handle_nand_erase_command, COMMAND_EXEC,
			"erase blocks on NAND flash device <num> [<offset> <length>]");
	register_command(cmd_ctx, nand_cmd, "dump",
			handle_nand_dump_command, COMMAND_EXEC,
			"dump from NAND flash device <num> <filename> "
			 "<offset> <length> [oob_raw | oob_only]");
	register_command(cmd_ctx, nand_cmd, "write",
			handle_nand_write_command, COMMAND_EXEC,
			"write to NAND flash device <num> <filename> <offset> "
			"[oob_raw | oob_only | oob_softecc | oob_softecc_kw]");

	register_command(cmd_ctx, nand_cmd, "raw_access",
			handle_nand_raw_access_command, COMMAND_EXEC,
			"raw access to NAND flash device <num> ['enable'|'disable']");

	return ERROR_OK;
}
