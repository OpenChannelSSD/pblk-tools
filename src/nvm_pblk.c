#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <liblightnvm_cli.h>

struct line_header {
	uint32_t crc;
	uint32_t identifier;
	uint32_t uuid[4];
	uint16_t type;
	uint16_t version;
	uint32_t id;
};

struct line_smeta {
	struct line_header header;
	uint32_t crc;
	uint32_t prev_id;
	uint64_t seq_nr;
	uint32_t window_wr_lun;
	uint32_t rsvd[2];
};

struct line_emeta {
	struct line_header header;
	uint32_t crc;
	uint32_t prev_id;
	uint64_t seq_nr;
	uint32_t window_wr_lun;
	uint32_t next_id;
	uint64_t nr_lbas;
};

void line_header_pr(const struct line_header *header)
{
	if (!header) {
		printf("  header: ~\n");
		return;
	}

	printf("  header:\n");
	printf("    crc: 0x%04x\n", header->crc);
	printf("    identifier: 0x%04x\n", header->identifier);
	printf("    uuid: [0x%04x, 0x%04x, 0x%04x, 0x%04x]\n",
	       header->uuid[0], header->uuid[1],
	       header->uuid[2], header->uuid[3]);
	printf("    type: %02x\n", header->type);
	printf("    version: %02x\n", header->version);
	printf("    id: %04u\n", header->id);
}

void line_smeta_pr(const struct line_smeta *smeta)
{
	if (!smeta) {
		printf("line_smeta: ~\n");
		return;
	}

	printf("line_smeta:\n");
	line_header_pr(&smeta->header);
	printf("  crc: 0x%04x\n", smeta->crc);
	printf("  prev_id: %04u\n", smeta->prev_id);
	printf("  seq_nr: %04lu\n", smeta->seq_nr);
	printf("  window_wr_lun: %08u\n", smeta->window_wr_lun);
}

void line_emeta_pr(const struct line_emeta *emeta)
{
	if (!emeta) {
		printf("line_emeta: ~\n");
		return;
	}

	printf("line_emeta:\n");
	line_header_pr(&emeta->header);
	printf("  crc: 0x%04x\n", emeta->crc);
	printf("  prev_id: %04u\n", emeta->prev_id);
	printf("  seq_nr: %04lu\n", emeta->seq_nr);
	printf("  window_wr_lun: %08u\n", emeta->window_wr_lun);
	printf("  next_id: %04u\n", emeta->next_id);
	printf("  nr_lbas: %04lu\n", emeta->nr_lbas);
}

int line_smeta_from_buf(char *buf, struct line_smeta *smeta)
{
	if ((!buf) || (!smeta)) {
		errno = EINVAL;
		return -1;
	}

	memcpy(smeta, buf, sizeof(*smeta));

	return 0;
}

int line_emeta_from_buf(char *buf, struct line_smeta *emeta)
{
	if ((!buf) || (!emeta)) {
		errno = EINVAL;
		return -1;
	}

	memcpy(emeta, buf, sizeof(*emeta));

	return 0;
}

int mdck(struct nvm_cli *cli)
{
	const struct nvm_geo *geo = cli->args.geo;
	struct nvm_dev *dev = cli->args.dev;
	size_t pbuf_len = geo->sector_nbytes * geo->nsectors;
	char pbuf_fpath[1024] = { 0 };
	char *pbuf;

	pbuf = nvm_buf_alloc(geo, pbuf_len);
	if (!pbuf) {
		errno = ENOMEM;
		return -1;
	}

	for (size_t line = 0; line < geo->nblocks; ++line) {
		struct line_smeta smeta= { 0 };
		struct line_emeta emeta = { 0 };
		struct nvm_addr addr = { 0 };
		struct nvm_ret ret = { 0 };

		addr.g.blk = line;

		memset(pbuf, 0 , pbuf_len);
		if (nvm_addr_read(dev, &addr, 1, pbuf, NULL, 0x0, &ret)) {
			perror("# nvm_addr_read");
			sprintf(pbuf, "!WRITTEN");
		}

		sprintf(pbuf_fpath, "/tmp/line%04lu.smeta", line);

		nvm_buf_to_file(pbuf, pbuf_len, pbuf_fpath);

		printf("# \n");
		line_smeta_from_buf(pbuf, &smeta);
		line_smeta_pr(&smeta);
	}

	return 0;
}

//
// Remaining code is CLI boiler-plate
//

/* Define commands */
static struct nvm_cli_cmd cmds[] = {
	{"mdck",	mdck,	NVM_CLI_ARG_DEV_PATH, NVM_CLI_OPT_DEFAULT},
};

/* Define the CLI */
static struct nvm_cli cli = {
	.title = "NVM pblk ",
	.descr_short = "Perform verification of pblk meta data",
	.descr_long = "See http://lightnvm.io/pblk-tools for additional info",
	.cmds = cmds,
	.ncmds = sizeof(cmds) / sizeof(cmds[0]),
};

/* Initialize and run */
int main(int argc, char **argv)
{
	int res = 0;

	if (nvm_cli_init(&cli, argc, argv) < 0) {
		nvm_cli_perror("FAILED");
		return 1;
	}

	res = nvm_cli_run(&cli);
	
	nvm_cli_destroy(&cli);

	return res;
}
