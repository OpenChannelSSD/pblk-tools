#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <liblightnvm_cli.h>

enum line_type {
	PBLK_LINETYPE_FREE = 0,
	PBLK_LINETYPE_LOG = 1,
	PBLK_LINETYPE_DATA = 2,
};

const char *line_type_str(int ltype)
{
	switch (ltype) {
	case PBLK_LINETYPE_FREE:
		return "PBLK_LINETYPE_FREE";
	case PBLK_LINETYPE_LOG:
		return "PBLK_LINETYPE_LOG";
	case PBLK_LINETYPE_DATA:
		return "PBLK_LINETYPE_DATA";
	default:
		return "PBLK_LINETYPE_INVALID";
	}
}

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
	uint64_t lbas[];
};

struct line {
	int id;

	struct line_smeta smeta;
	struct nvm_addr smeta_addr;
	struct nvm_ret smeta_ret;

	struct line_emeta emeta;
	struct nvm_addr emeta_addr;
	struct nvm_ret emeta_ret;
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
	printf("    type: %s\n", line_type_str(header->type));
	printf("    version: %02x\n", header->version);
	printf("    id: %04u\n", header->id);
}

void line_smeta_pr(const struct line_smeta *smeta)
{
	if (!smeta) {
		printf("smeta: ~\n");
		return;
	}

	printf("smeta:\n");
	line_header_pr(&smeta->header);
	printf("  crc: 0x%04x\n", smeta->crc);
	printf("  prev_id: %04u\n", smeta->prev_id);
	printf("  seq_nr: %04lu\n", smeta->seq_nr);
	printf("  window_wr_lun: %08u\n", smeta->window_wr_lun);
}

void line_emeta_pr(const struct line_emeta *emeta)
{
	if (!emeta) {
		printf("emeta: ~\n");
		return;
	}

	printf("emeta:\n");
	line_header_pr(&emeta->header);
	printf("  crc: 0x%04x\n", emeta->crc);
	printf("  prev_id: %04u\n", emeta->prev_id);
	printf("  seq_nr: %04lu\n", emeta->seq_nr);
	printf("  window_wr_lun: %08u\n", emeta->window_wr_lun);
	printf("  next_id: %04u\n", emeta->next_id);
	printf("  nr_lbas: %04lu\n", emeta->nr_lbas);
}

void line_pr(const struct line *line)
{
	int smeta_read = !(line->smeta_ret.status || line->smeta_ret.result);
	int emeta_read = !(line->emeta_ret.status || line->emeta_ret.result);

	printf("line_%04d:\n", line->id);
	printf("  id: %04d:\n", line->id);
	printf("  smeta_"); nvm_addr_pr(line->smeta_addr);
	printf("  emeta_"); nvm_addr_pr(line->emeta_addr);

	if (smeta_read) {
		printf("  smeta_nvm_ret: ~\n");
	} else {
		printf("  smeta_");
		nvm_ret_pr(&line->smeta_ret);
	}

	if (emeta_read) {
		printf("  emeta_nvm_ret: ~\n");
	} else {
		printf("  emeta_");
		nvm_ret_pr(&line->emeta_ret);
	}

	if (smeta_read) {
		printf("line%04i_", line->id);
		line_smeta_pr(&line->smeta);
	}
	if (emeta_read) {
		printf("line%04i_", line->id);
		line_emeta_pr(&line->emeta);
	}
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

int line_emeta_from_buf(char *buf, struct line_emeta *emeta)
{
	if ((!buf) || (!emeta)) {
		errno = EINVAL;
		return -1;
	}

	memcpy(emeta, buf, sizeof(*emeta));

	return 0;
}

/**
 * Compute and update the smeta-address for the given line given on the given
 * device
 */
int line_smeta_addr_calc(struct line *line, struct nvm_dev *dev,
			 const struct nvm_bbt **bbts)
{
	const struct nvm_geo *geo = nvm_dev_get_geo(dev);
	const int tluns = geo->nchannels * geo->nluns;

	for (int tlun = 0; tlun < tluns; ++tlun) {
		const struct nvm_bbt *bbt = bbts[tlun];
		const size_t blk_off = line->id * geo->nplanes;
		const size_t blk_lim = blk_off + geo->nplanes;

		int broken = 0;

		for (size_t blk = blk_off; blk < blk_lim; ++blk)
			broken |= bbt->blks[blk];

		if (broken)
			continue;

		line->smeta_addr.ppa = 0;
		line->smeta_addr.g.blk = line->id;
		line->smeta_addr.g.ch = bbt->addr.g.ch;
		line->smeta_addr.g.lun = bbt->addr.g.lun;
		return 0;
	}

	return -1;
}

/**
 * Compute and update the emeta-address for the given given on the given device
 */
int line_emeta_addr_calc(struct line *line, struct nvm_dev *dev,
			 const struct nvm_bbt **bbts)
{
	const struct nvm_geo *geo = nvm_dev_get_geo(dev);
	const int tluns = geo->nchannels * geo->nluns;

	for (int tlun = tluns - 1; tlun >= 0; --tlun) {
		const struct nvm_bbt *bbt = bbts[tlun];
		const size_t blk_off = line->id * geo->nplanes;
		const size_t blk_lim = blk_off + geo->nplanes;

		int broken = 0;

		for (size_t blk = blk_off; blk < blk_lim; ++blk)
			broken |= bbt->blks[blk];

		if (broken)
			continue;

		line->emeta_addr.ppa = 0;
		line->emeta_addr.g.blk = line->id;
		line->emeta_addr.g.ch = bbt->addr.g.ch;
		line->emeta_addr.g.lun = bbt->addr.g.lun;
		line->emeta_addr.g.pg = geo->npages - 1;
		return 0;
	}

	return -1;
}

/**
 * Perform a shallow comparison of the given line smeta and emeta
 * structures
 *
 * @returns 0 when the smeta and emeta and equivalent, some other value when
 * they are not.
 */
int line_check_shallow(struct line *line)
{
	const int hdr_sz = sizeof(struct line_header);

	// Compare headers
	if (memcmp(&line->smeta.header, &line->emeta.header, hdr_sz))
		return -1;

	// TODO: CRC checks and other checks?

	// Check return-code of read commands
	if (line->smeta_ret.status || line->smeta_ret.result ||
	    line->emeta_ret.status || line->emeta_ret.result)
		return -1;

	// Compare remainder of smeta bodies
	return (line->smeta.prev_id != line->emeta.prev_id) ||
		(line->smeta.seq_nr != line->emeta.seq_nr);
}

void status_pr(const char *task, size_t cur, size_t total)
{
	printf("status: {task: '%s', cur: %lu, total: %lu}\n", task, cur, total);
}

int cmd_mdck(struct nvm_cli *cli)
{
	const struct nvm_geo *geo = cli->args.geo;
	struct nvm_dev *dev = cli->args.dev;
	size_t smeta_buf_len = geo->sector_nbytes;
	const size_t tluns = geo->nchannels * geo->nluns;
	const struct nvm_bbt **bbts;
	char *smeta_buf;
	struct line *lines;

	// Allocate smeta read buffer
	smeta_buf = nvm_buf_alloc(geo, smeta_buf_len);
	if (!smeta_buf) {
		errno = ENOMEM;
		return -1;
	}

	// Retrieve all bad-block-tables
	bbts = malloc(tluns * sizeof(const struct nvm_bbt*));
	if (!bbts) {
		errno = ENOMEM;
		free(smeta_buf);
		return -1;
	}
	for (size_t tlun = 0; tlun < tluns; ++tlun) {
		struct nvm_ret ret = { 0 };
		struct nvm_addr lun_addr = { 0 };

		lun_addr.g.ch = tlun % geo->nchannels;
		lun_addr.g.lun = (tlun / geo->nchannels) % geo->nluns;

		if (!cli->opts.brief)
			status_pr("bbt_get", tlun, tluns);

		bbts[tlun] = nvm_bbt_get(dev, lun_addr, &ret);
		if (!bbts[tlun]) {
			free(smeta_buf);
			free(bbts);
			return -1;	// Propagate errno from nvm_bbt_get
		}
	}

	// Construct 'lines' containing id, smeta address and emeta addresses
	lines = malloc(geo->nblocks * sizeof(*lines));
	if (!lines) {
		errno = ENOMEM;
		free(smeta_buf);
		free(bbts);
		return -1;
	}
	memset(lines, 0, geo->nblocks * sizeof(*lines));
	for (size_t i = 0; i < geo->nblocks; ++i) {
		struct line *line = &lines[i];

		if (!cli->opts.brief)
			status_pr("line_setup", i, geo->nblocks);

		line->id = i;
		line_smeta_addr_calc(line, dev, bbts);
		line_emeta_addr_calc(line, dev, bbts);
	}

	// Read smeta for all lines from media into data structures
	for (size_t i = 0; i < geo->nblocks; ++i) {
		struct line *line = &lines[i];

		if (!cli->opts.brief)
			status_pr("smeta_read", i, geo->nblocks);

		memset(smeta_buf, 0 , smeta_buf_len);
		if (!nvm_addr_read(dev, &line->smeta_addr, 1, smeta_buf, NULL,
				   0x0, &line->smeta_ret))
			line_smeta_from_buf(smeta_buf, &line->smeta);
	}

	// Read emeta for all lines from media into data structures
	for (size_t i = 0; i < geo->nblocks; ++i) {
		struct line *line = &lines[i];

		if (!cli->opts.brief)
			status_pr("emeta_read", i, geo->nblocks);

		memset(smeta_buf, 0 , smeta_buf_len);
		if (!nvm_addr_read(dev, &line->emeta_addr, 1, smeta_buf, NULL,
				   0x0, &line->emeta_ret))
			line_emeta_from_buf(smeta_buf, &line->emeta);
	}

	// Present the results
	for (size_t i = 0; i < geo->nblocks; ++i) {
		struct line *line = &lines[i];

		if ((!cli->opts.brief) || line_check_shallow(line)) {
			printf("\n");
			line_pr(line);
		}
	}

	free(smeta_buf);
	free(bbts);
	free(lines);

	return 0;
}

//
// Remaining code is CLI boiler-plate
//

/* Define commands */
static struct nvm_cli_cmd cmds[] = {
	{"mdck", cmd_mdck, NVM_CLI_ARG_DEV_PATH, NVM_CLI_OPT_DEFAULT},
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

