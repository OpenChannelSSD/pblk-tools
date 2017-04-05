#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <zlib.h>
#include <liblightnvm_cli.h>

#define PBLK_META_VER 0x1
#define PBLK_META_IDENT 0x70626c6b

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
		return "PBLK_LINETYPE_UNDEF";
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

enum line_state {
	PBLK_LINE_STATE_UNKNOWN = 0x0,
	PBLK_LINE_STATE_OPEN = 0x1,
	PBLK_LINE_STATE_CLOSED = 0x1 << 2,
	PBLK_LINE_STATE_BAD = 0x1 << 3,
};

const char *line_state_str(int lstate)
{
	switch (lstate) {
	case PBLK_LINE_STATE_OPEN:
		return "PBLK_LINE_STATE_OPEN";
	case PBLK_LINE_STATE_CLOSED:
		return "PBLK_LINE_STATE_CLOSED";
	case PBLK_LINE_STATE_BAD:
		return "PBLK_LINE_STATE_BAD";
	case PBLK_LINE_STATE_UNKNOWN:
		return "PBLK_LINE_STATE_UNKNOWN";
	default:
		return "PBLK_LINE_STATE_UNDEF";
	}
}

struct line {
	int id;
	enum line_state state;

	struct line_smeta smeta;
	struct nvm_addr smeta_addr;
	struct nvm_ret smeta_ret;

	struct line_emeta emeta;
	struct nvm_addr emeta_addr;
	struct nvm_ret emeta_ret;
};

static inline uint32_t line_header_crc(struct line_header *hdr)
{
	return crc32(0, ((unsigned char *)hdr) + sizeof(hdr->crc),
		     sizeof(*hdr) - sizeof(hdr->crc)
	) ^ (~(uint32_t)0);
}

static inline uint32_t line_smeta_crc(struct line_smeta *smeta, size_t len)
{
	return crc32(0, ((unsigned char *)smeta) +
			sizeof(smeta->header) + sizeof(smeta->crc),
			len -
			sizeof(smeta->header) - sizeof(smeta->crc)
	) ^ (~(uint32_t)0);
}

static inline uint32_t line_emeta_crc(struct line_emeta *emeta, size_t len)
{
	return crc32(0, ((unsigned char *)emeta) +
			sizeof(emeta->header) + sizeof(emeta->crc),
			len -
			sizeof(emeta->header) - sizeof(emeta->crc)
	) ^ (~(uint32_t)0);
}


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
	printf("  state: %s\n", line_state_str(line->state));
	if (line->state == PBLK_LINE_STATE_BAD)
		return;

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
 *
 * @returns On success, 0 is returned. On error, -1 is returned, `err` set to
 * indicate the error. Error occurs if all blocks in the line are bad.
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

	//for (int tlun = tluns - 1; tlun >= 0; --tlun) {
	for (int tlun = 0; tlun < tluns; ++tlun) {
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
 * Scan the given device lun range for meta-data
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param lun_bgn First lun in flattened range
 * @param lun_end Last lun in flattened range
 *
 * @returns On success, a pointer to array of 'struct line'. On error, NULL is
 * returned and errno set to indicate the error.
 */
struct line *pblk_meta_scan(struct nvm_cli *cli, int lun_bgn, int lun_end)
{
	int err = 0;
	struct line *lines = NULL;
	struct nvm_dev *dev = cli->args.dev;
	const struct nvm_geo *geo = cli->args.geo;
	const size_t tluns = (lun_end - lun_bgn) + 1;
	const size_t nlines = geo->nblocks;
	const struct nvm_bbt **bbts = NULL;
	char *smeta_buf = NULL;
	char *emeta_buf = NULL;
	size_t smeta_buf_len;
	size_t emeta_buf_len;

	if (lun_bgn > lun_end) {
		errno = EINVAL;
		return NULL;
	}

	smeta_buf_len = geo->sector_nbytes;
	emeta_buf_len = geo->sector_nbytes * geo->nsectors * geo->nplanes * \
			tluns;

	// Allocate smeta read buffer
	smeta_buf = nvm_buf_alloc(geo, smeta_buf_len);
	if (!smeta_buf) {
		errno = ENOMEM;
		err = -1;
		goto scan_exit;
	}

	// Allocate emeta read buffer
	emeta_buf = nvm_buf_alloc(geo, emeta_buf_len);
	if (!emeta_buf) {
		errno = ENOMEM;
		err = -1;
		goto scan_exit;
	}

	// Retrieve all bad-block-tables
	bbts = malloc(tluns * sizeof(const struct nvm_bbt*));
	if (!bbts) {
		errno = ENOMEM;
		err = -1;
		goto scan_exit;
	}
	for (size_t tlun = 0; tlun < tluns; ++tlun) {
		struct nvm_ret ret = { 0 };
		struct nvm_addr lun_addr = { 0 };

		lun_addr.g.ch = tlun % geo->nchannels;
		lun_addr.g.lun = (tlun / geo->nchannels) % geo->nluns;

		if (cli->opts.status)
			nvm_cli_status_pr("bbt_get", tlun, tluns);

		bbts[tlun] = nvm_bbt_get(dev, lun_addr, &ret);
		if (!bbts[tlun]) {
			// errno: propagate from nvm_bbt_get
			err = -1;
			goto scan_exit;
		}
	}

	// Allocate lines
	lines = malloc(nlines * sizeof(*lines));
	if (!lines) {
		errno = ENOMEM;
		err = -1;
		goto scan_exit;
	}
	memset(lines, 0, nlines * sizeof(*lines));

	// Fill lines with: id, state [and addresses]
	for (size_t i = 0; i < nlines; ++i) {
		struct line *line = &lines[i];

		if (cli->opts.status)
			nvm_cli_status_pr("line_setup", i, nlines);

		line->id = i;
		line->state = PBLK_LINE_STATE_UNKNOWN;

		if (line_smeta_addr_calc(line, dev, bbts))
			line->state = PBLK_LINE_STATE_BAD;

		if (line_emeta_addr_calc(line, dev, bbts))
			line->state = PBLK_LINE_STATE_BAD;
	}

	// Fill lines with smeta or read-err
	for (size_t i = 0; i < nlines; ++i) {
		struct line *line = &lines[i];

		if (line->state == PBLK_LINE_STATE_BAD)
			continue;

		if (cli->opts.status)
			nvm_cli_status_pr("smeta_read", i, nlines);

		memset(smeta_buf, 0 , smeta_buf_len);
		if (!nvm_addr_read(dev, &line->smeta_addr, 1, smeta_buf, NULL,
				   0x0, &line->smeta_ret))
			line_smeta_from_buf(smeta_buf, &line->smeta);
	}

	// Fill lines with emeta or read-err
	for (size_t i = 0; i < nlines; ++i) {
		struct line *line = &lines[i];

		if (line->state == PBLK_LINE_STATE_BAD)
			continue;

		if (cli->opts.status)
			nvm_cli_status_pr("emeta_read", i, nlines);

		memset(emeta_buf, 0 , emeta_buf_len);
		if (!nvm_addr_read(dev, &line->emeta_addr, 1, emeta_buf, NULL,
				   0x0, &line->emeta_ret))
			line_emeta_from_buf(emeta_buf, &line->emeta);
	}

	// Update line-state
	for (size_t i = 0; i < nlines; ++i) {
		struct line *line = &lines[i];
		const int smeta_read = !(line->smeta_ret.status || line->smeta_ret.result);
		const int emeta_read = !(line->emeta_ret.status || line->emeta_ret.result);

		if (line->state == PBLK_LINE_STATE_BAD)
			continue;

		// TODO: Expand the state classification
		if (smeta_read && emeta_read) {
			line->state = PBLK_LINE_STATE_CLOSED;
		} else if (smeta_read && (!emeta_read)) {
			line->state = PBLK_LINE_STATE_OPEN;
		}
	}

scan_exit:
	free(smeta_buf);
	free(emeta_buf);
	free(bbts);
	if (err) {
		free(lines);
		lines = NULL;
	}

	return lines;
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

	// TODO: CRC-check smeta header

	// Compare headers
	if (memcmp(&line->smeta.header, &line->emeta.header, hdr_sz))
		return -1;

	// TODO: other checks?

	// Check return-code of read commands
	if (line->smeta_ret.status || line->smeta_ret.result ||
	    line->emeta_ret.status || line->emeta_ret.result)
		return -1;

	// Compare remainder of smeta bodies
	return (line->smeta.prev_id != line->emeta.prev_id) ||
		(line->smeta.seq_nr != line->emeta.seq_nr);
}

int cmd_meta_dump(struct nvm_cli *cli)
{
	const struct nvm_geo *geo = cli->args.geo;
	const size_t nlines = geo->nblocks;
	struct line *lines = NULL;

	lines = pblk_meta_scan(cli, 0, (geo->nchannels * geo->nluns) - 1);
	if (!lines) {
		return 1;
	}

	for (size_t i = 0; i < nlines; ++i) {	// Dump them stdout
		struct line *line = &lines[i];

		printf("\n");
		line_pr(line);
	}

	free(lines);

	return 0;
}

int cmd_meta_check(struct nvm_cli *cli)
{
	struct line_smeta smeta = { 0 };

	smeta.header.crc = 0x42862eb8;
	smeta.header.identifier = 0x70626c6b;
	smeta.header.type = PBLK_LINETYPE_DATA;
	smeta.header.version = 0x1;
	smeta.header.id = 16;

	line_smeta_pr(&smeta);

	smeta.header.crc = line_header_crc(&smeta.header);

	line_smeta_pr(&smeta);

	return 0;
}

//
// Remaining code is CLI boiler-plate
//

/* Define commands */
static struct nvm_cli_cmd cmds[] = {
	{
		"meta_dump",
		cmd_meta_dump,
		NVM_CLI_ARG_DEV_PATH,
		NVM_CLI_OPT_DEFAULT | NVM_CLI_OPT_STATUS | NVM_CLI_OPT_BRIEF
	},
	{
		"meta_check",
		cmd_meta_check,
		NVM_CLI_ARG_DEV_PATH,
		NVM_CLI_OPT_DEFAULT | NVM_CLI_OPT_STATUS | NVM_CLI_OPT_BRIEF
	},

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

