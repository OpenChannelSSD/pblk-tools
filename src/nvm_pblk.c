#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <zlib.h>
#include <liblightnvm_cli.h>

#define PBLK_META_VER 0x1
#define PBLK_META_IDENT 0x70626c6b

// NOTE: These limits are used to reduce memory dynamic memory management
#define PBLK_MAX_LINES 4096
#define PBLK_MAX_LUNS 512
#define PBLK_MAX_INSTS 64

enum pblk_line_type {
	PBLK_LINETYPE_FREE = 0,
	PBLK_LINETYPE_LOG = 1,
	PBLK_LINETYPE_DATA = 2,
};

const char *pblk_line_type_str(int ltype)
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

struct pblk_line_header {
	uint32_t crc;
	uint32_t identifier;
	uint32_t uuid[4];
	uint16_t type;
	uint16_t version;
	uint32_t id;
};

struct pblk_line_smeta {
	struct pblk_line_header header;
	uint32_t crc;
	uint32_t prev_id;
	uint64_t seq_nr;
	uint32_t window_wr_lun;
	uint32_t rsvd[2];
};

struct pblk_line_emeta {
	struct pblk_line_header header;
	uint32_t crc;
	uint32_t prev_id;
	uint64_t seq_nr;
	uint32_t window_wr_lun;
	uint32_t next_id;
	uint64_t nr_lbas;
	uint64_t lbas[];
};

enum pblk_line_state {
	PBLK_LINE_STATE_UNKNOWN = 0x0,
	PBLK_LINE_STATE_OPEN = 0x1,
	PBLK_LINE_STATE_CLOSED = 0x1 << 2,
	PBLK_LINE_STATE_BAD = 0x1 << 3,
};

const char *pblk_line_state_str(int lstate)
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

struct pblk_line {
	int id;
	enum pblk_line_state state;

	struct pblk_line_smeta smeta;
	struct nvm_addr smeta_addr;
	struct nvm_ret smeta_ret;

	struct pblk_line_emeta emeta;
	struct nvm_addr emeta_addr;
	struct nvm_ret emeta_ret;
};

struct pblk_inst {
	int lun_bgn;				///< LUN range begin
	int lun_end;				///< LUN range end
	int nluns;				///< Number of LUNs
	struct nvm_addr luns[PBLK_MAX_LUNS];	///< LUNs addresses
	const struct nvm_bbt *bbts[PBLK_MAX_LUNS];	///< bbts in stripe order
	int nlines;				///< Number of lines
	struct pblk_line lines[PBLK_MAX_LINES];	///< Array of lines
};

struct pblk {
	struct nvm_dev *dev;
	int tluns;				///< Total number of luns
	int ninsts;				///< Number of pblk instances
	struct pblk_inst insts[PBLK_MAX_INSTS];	///< pblk instances
};

void pblk_instance_pr(const struct pblk_inst *inst)
{
	if (!inst) {
		printf("pblk_instance: ~\n");
		return;
	}

	printf("pblk_instance:\n");
	printf("  lun_bgn: %d\n", inst->lun_bgn);
	printf("  lun_end: %d\n", inst->lun_end);
	printf("  nluns: %d\n", inst->nluns);
}

/**
 * Compute CRC of the given line_header
 */
static inline uint32_t pblk_line_header_crc(struct pblk_line_header *hdr)
{
	return crc32(0, ((unsigned char *)hdr) + sizeof(hdr->crc),
		     sizeof(*hdr) - sizeof(hdr->crc)
	) ^ (~(uint32_t)0);
}

/**
 * Compute CRC of the given smeta
 */
static inline uint32_t pblk_line_smeta_crc(struct pblk_line_smeta *smeta,
					   size_t len)
{
	return crc32(0, ((unsigned char *)smeta) +
			sizeof(smeta->header) + sizeof(smeta->crc),
			len -
			sizeof(smeta->header) - sizeof(smeta->crc)
	) ^ (~(uint32_t)0);
}

/**
 * Compute CRC of the given emeta
 */
static inline uint32_t pblk_line_emeta_crc(struct pblk_line_emeta *emeta,
					   size_t len)
{
	return crc32(0, ((unsigned char *)emeta) +
			sizeof(emeta->header) + sizeof(emeta->crc),
			len -
			sizeof(emeta->header) - sizeof(emeta->crc)
	) ^ (~(uint32_t)0);
}

void pblk_line_header_pr(const struct pblk_line_header *header)
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
	printf("    type: %s\n", pblk_line_type_str(header->type));
	printf("    version: %02x\n", header->version);
	printf("    id: %04u\n", header->id);
}

void pblk_line_smeta_pr(const struct pblk_line_smeta *smeta)
{
	if (!smeta) {
		printf("smeta: ~\n");
		return;
	}

	printf("smeta:\n");
	pblk_line_header_pr(&smeta->header);
	printf("  crc: 0x%04x\n", smeta->crc);
	printf("  prev_id: %04u\n", smeta->prev_id);
	printf("  seq_nr: %04lu\n", smeta->seq_nr);
	printf("  window_wr_lun: %08u\n", smeta->window_wr_lun);
}

void pblk_line_emeta_pr(const struct pblk_line_emeta *emeta)
{
	if (!emeta) {
		printf("emeta: ~\n");
		return;
	}

	printf("emeta:\n");
	pblk_line_header_pr(&emeta->header);
	printf("  crc: 0x%04x\n", emeta->crc);
	printf("  prev_id: %04u\n", emeta->prev_id);
	printf("  seq_nr: %04lu\n", emeta->seq_nr);
	printf("  window_wr_lun: %08u\n", emeta->window_wr_lun);
	printf("  next_id: %04u\n", emeta->next_id);
	printf("  nr_lbas: %04lu\n", emeta->nr_lbas);
}

void pblk_line_pr(const struct pblk_line *line)
{
	int smeta_read = !(line->smeta_ret.status || line->smeta_ret.result);
	int emeta_read = !(line->emeta_ret.status || line->emeta_ret.result);

	printf("line_%04d:\n", line->id);
	printf("  id: %04d:\n", line->id);
	printf("  state: %s\n", pblk_line_state_str(line->state));
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
		pblk_line_smeta_pr(&line->smeta);
	}
	if (emeta_read) {
		printf("line%04i_", line->id);
		pblk_line_emeta_pr(&line->emeta);
	}
}

int pblk_line_smeta_from_buf(char *buf, struct pblk_line_smeta *smeta)
{
	if ((!buf) || (!smeta)) {
		errno = EINVAL;
		return -1;
	}

	memcpy(smeta, buf, sizeof(*smeta));

	return 0;
}

int pblk_line_emeta_from_buf(char *buf, struct pblk_line_emeta *emeta)
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
int pblk_line_smeta_addr_calc(struct pblk_line *line, struct pblk_inst *inst,
			      const struct nvm_geo *geo)
{
	for (int lun = 0; lun < inst->nluns; ++lun) {
		const struct nvm_bbt *bbt = inst->bbts[lun];
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
int pblk_line_emeta_addr_calc(struct pblk_line *line, struct pblk_inst *inst,
			      const struct nvm_geo *geo)
{
	for (int lun = 0; lun < inst->nluns; ++lun) {
		const struct nvm_bbt *bbt = inst->bbts[lun];
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
 * Check whether the given smeta has a valid header
 *
 * @returns 0 When valid, some value otherwise
 */
static inline int pblk_line_smeta_hdr_check(struct pblk_line_smeta *smeta)
{
	uint32_t crc = pblk_line_header_crc(&smeta->header);

	if (smeta->header.identifier != PBLK_META_IDENT)
		return -1;

	if (smeta->header.version != PBLK_META_VER)
		return -1;

	if (smeta->header.crc != crc)
		return -1;

	return 0;
}

/**
 * Check whether the given smeta has a valid "first" header
 *
 * @returns 0 When valid, some value otherwise
 */
static inline int pblk_line_smeta_hdrf_check(struct pblk_line_smeta *smeta)
{
	if (pblk_line_smeta_hdr_check(smeta))
		return -1;

	if (smeta->header.id != 0)
		return -1;

	if (smeta->prev_id != ~(uint32_t)0)
		return -1;

	if (smeta->seq_nr != 0)
		return -1;

	return 0;
}

/**
 * Scan for line meta of the given pbkl instance
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param lun_bgn First lun in flattened range
 * @param lun_end Last lun in flattened range
 *
 * @returns On success, a pointer to array of 'struct line'. On error, NULL is
 * returned and errno set to indicate the error.
 */
int pblk_init_instance_lines(struct pblk *pblk, struct pblk_inst *inst)
{
	int err = 0;
	struct nvm_dev *dev = pblk->dev;
	const struct nvm_geo *geo = nvm_dev_get_geo(pblk->dev);
	char *smeta_buf = NULL;
	char *emeta_buf = NULL;
	size_t smeta_buf_len;
	size_t emeta_buf_len;

	if (inst->lun_bgn > inst->lun_end) {
		errno = ENOMEM;
		err = -1;
		goto scan_exit;
	}

	smeta_buf_len = geo->sector_nbytes;
	emeta_buf_len = geo->sector_nbytes * geo->nsectors * geo->nplanes * \
			inst->nluns;

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

	inst->nlines = geo->nblocks;

	// Fill lines with: id, state [and addresses]
	for (size_t i = 0; i < inst->nlines; ++i) {
		struct pblk_line *line = &inst->lines[i];

		line->id = i;
		line->state = PBLK_LINE_STATE_UNKNOWN;

		if (pblk_line_smeta_addr_calc(line, inst, geo))
			line->state = PBLK_LINE_STATE_BAD;

		if (pblk_line_emeta_addr_calc(line, inst, geo))
			line->state = PBLK_LINE_STATE_BAD;
	}

	// Fill lines with smeta or read-err
	for (size_t i = 0; i < inst->nlines; ++i) {
		struct pblk_line *line = &inst->lines[i];

		if (line->state == PBLK_LINE_STATE_BAD)
			continue;

		memset(smeta_buf, 0 , smeta_buf_len);
		if (!nvm_addr_read(dev, &line->smeta_addr, 1, smeta_buf, NULL,
				   0x0, &line->smeta_ret))
			pblk_line_smeta_from_buf(smeta_buf, &line->smeta);
	}

	// Fill lines with emeta or read-err
	for (size_t i = 0; i < inst->nlines; ++i) {
		struct pblk_line *line = &inst->lines[i];

		if (line->state == PBLK_LINE_STATE_BAD)
			continue;

		memset(emeta_buf, 0 , emeta_buf_len);
		if (!nvm_addr_read(dev, &line->emeta_addr, 1, emeta_buf, NULL,
				   0x0, &line->emeta_ret))
			pblk_line_emeta_from_buf(emeta_buf, &line->emeta);
	}

	// Update pblk_line-state
	for (size_t i = 0; i < inst->nlines; ++i) {
		struct pblk_line *line = &inst->lines[i];
		const int smeta_read = !(line->smeta_ret.status ||
							line->smeta_ret.result);
		const int emeta_read = !(line->emeta_ret.status ||
							line->emeta_ret.result);

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

	return err;
}

int pblk_init_instance(struct pblk_inst *inst, struct nvm_addr iaddr, int nluns,
		       struct pblk *pblk)
{
	const struct nvm_geo *geo = nvm_dev_get_geo(pblk->dev);

	inst->nluns = nluns;
	inst->lun_bgn = iaddr.g.ch * geo->nluns;
	inst->lun_end = inst->lun_bgn + inst->nluns;

	size_t nchannels = nluns / geo->nluns;

	for (int vlun = 0; vlun < inst->nluns; ++vlun) {
		size_t ch = vlun % nchannels;
		size_t lun = (vlun / nchannels) % geo->nluns;

		inst->luns[vlun] = iaddr;
		inst->luns[vlun].g.ch += ch;
		inst->luns[vlun].g.lun = lun;

		inst->bbts[vlun] = nvm_bbt_get(pblk->dev, inst->luns[vlun], NULL);
		if (!inst->bbts[vlun])
			return -1;
	}

	return 0;
}

/**
 * Initialize a pblk struct by scanning given device for instances
 *
 * NOTE: This does not take bbt info into account
 */
int pblk_init_instances(struct pblk *pblk, int flags)
{
	int err = 0;

	struct nvm_dev *dev = pblk->dev;
	const struct nvm_geo *geo = nvm_dev_get_geo(dev);

	struct pblk_line_smeta smeta = { 0 };
	char *smeta_buf = NULL;
	const size_t smeta_buf_len = geo->sector_nbytes * 20;

	// Allocate smeta read buffer
	smeta_buf = nvm_buf_alloc(geo, smeta_buf_len);
	if (!smeta_buf) {
		// errno: propagate from nvm_buf_alloc
		err = -1;
		goto scan_exit;
	}

	pblk->tluns = geo->nchannels * geo->nluns;

	// TODO: This should account for bbt-info but it will only fail if all
	// LUNs on the channel have died, due to the non-channel-sharing
	// assumption of pblk-instances
	for (size_t tlun = 0; tlun < pblk->tluns; ++tlun) {
		struct nvm_ret ret = { 0 };
		struct nvm_addr lun_addr = { 0 };
		struct nvm_addr inst_addr = { 0 };

		lun_addr.g.lun = tlun % geo->nluns;
		lun_addr.g.ch = (tlun / geo->nluns) % geo->nchannels;

		memset(smeta_buf, 0 , smeta_buf_len);
		if (nvm_addr_read(dev, &lun_addr, 1, smeta_buf, NULL, 0x0, &ret))
			continue;

		pblk_line_smeta_from_buf(smeta_buf, &smeta);
		if (pblk_line_smeta_hdrf_check(&smeta))
			continue;

		inst_addr.g.ch = lun_addr.g.ch;	// Assuming non-shared channels
		if (pblk_init_instance(&pblk->insts[pblk->ninsts], inst_addr,
				       smeta.window_wr_lun, pblk))
			continue;

		++(pblk->ninsts);
	}

scan_exit:
	free(smeta_buf);

	return err;
}

/**
 * Allocate and initialize dev, tluns, and bbts.
 */
struct pblk *pblk_init(struct nvm_dev *dev, int flags)
{
	const struct nvm_geo *geo = nvm_dev_get_geo(dev);
	const int tluns = geo->nchannels * geo->nluns;
	struct pblk *pblk = NULL;

	pblk = malloc(sizeof(*pblk));
	if (!pblk)
		return NULL;
	memset(pblk, 0, sizeof(*pblk));

	pblk->dev = dev;
	pblk->tluns = tluns;

	return pblk;
}

int check_assumptions(struct nvm_cli *cli)
{
	const struct nvm_geo *geo = cli->args.geo;

	if (PBLK_MAX_LINES < geo->nblocks) {
		nvm_cli_info_pr("Device has more blocks than PBLK_MAX_LINES");
		return -1;
	}
	if (PBLK_MAX_LUNS < (geo->nchannels * geo->nluns)) {
		nvm_cli_info_pr("Device has more LUNs than PBLK_MAX_LUNS");
		return -1;
	}
	if (PBLK_MAX_INSTS < (geo->nchannels)) {
		nvm_cli_info_pr("Potentially more than PBLK_MAX_INSTS");
		return -1;
	}

	return 0;
}

int cmd_meta_check(struct nvm_cli *cli)
{
	int res = 0;
	struct pblk *pblk = NULL;
	
	nvm_cli_info_pr("Initializing pblk -- fetching bbt etc.");
	pblk = pblk_init(cli->args.dev, 0x0);

	nvm_cli_info_pr("Scanning device for pblk instances");
	if (pblk_init_instances(pblk, 0x0)) {
		nvm_cli_info_pr("Scanning failed");
		res = 1;
		goto cmd_exit;
	}
	nvm_cli_info_pr("Found %d instances", pblk->ninsts);

	nvm_cli_info_pr("Scanning device for pblk instance line-meta");
	for (int i = 0; i < pblk->ninsts; ++i) {
		struct pblk_inst *inst = &pblk->insts[i];

		if (pblk_init_instance_lines(pblk, inst))
			nvm_cli_info_pr("Failed for instance %d", i);
	}

	nvm_cli_info_pr("Checking meta data for %d instances", pblk->ninsts);
	for (int i = 0; i < pblk->ninsts; ++i) {
		struct pblk_inst *inst = &pblk->insts[i];
	
		nvm_cli_info_pr("Checking instance %d", i);
		pblk_instance_pr(&pblk->insts[i]);

		uint32_t prev_id = ~(uint32_t)0;

		for (size_t j = 0; j < inst->nlines; ++j) {
			struct pblk_line *line = &inst->lines[j];

			switch (line->state) {
			case PBLK_LINE_STATE_OPEN:
				nvm_cli_info_pr("hazard: found an open line");
				nvm_line_pr(line);
				break;
			}
		}
	}

cmd_exit:
	free(pblk);
	return res;

}

int cmd_lines(struct nvm_cli *cli)
{
	int res = 0;
	struct pblk *pblk = NULL;
	
	nvm_cli_info_pr("Initializing pblk -- fetching bbt etc.");
	pblk = pblk_init(cli->args.dev, 0x0);

	nvm_cli_info_pr("Scanning device for pblk instances");
	if (pblk_init_instances(pblk, 0x0)) {
		nvm_cli_info_pr("Scanning failed");
		res = 1;
		goto cmd_exit;
	}
	nvm_cli_info_pr("Found %d instances", pblk->ninsts);

	nvm_cli_info_pr("Scanning device for pblk instance line-meta");
	for (int i = 0; i < pblk->ninsts; ++i) {
		struct pblk_inst *inst = &pblk->insts[i];

		if (pblk_init_instance_lines(pblk, inst))
			nvm_cli_info_pr("Failed for instance %d", i);
	}

	nvm_cli_info_pr("Dumping meta for %d instances", pblk->ninsts);
	for (int i = 0; i < pblk->ninsts; ++i) {
		struct pblk_inst *inst = &pblk->insts[i];
	
		nvm_cli_info_pr("Meta for instance %d", i);
		pblk_instance_pr(&pblk->insts[i]);

		for (size_t j = 0; j < inst->nlines; ++j) {
			struct pblk_line *line = &inst->lines[j];

			if (cli->opts.brief &&
					line->state == PBLK_LINE_STATE_UNKNOWN)
				continue;

			printf("\n");
			pblk_line_pr(line);
		}
	}

cmd_exit:
	free(pblk);
	return res;
}

int cmd_instances(struct nvm_cli *cli)
{
	int res = 0;
	struct pblk *pblk = NULL;
	
	nvm_cli_info_pr("Initializing pblk -- fetching bbt etc.");
	pblk = pblk_init(cli->args.dev, 0x0);

	nvm_cli_info_pr("Scanning device for pblk instances");
	if (pblk_init_instances(pblk, 0x0)) {
		nvm_cli_info_pr("Scanning failed");
		res = 1;
		goto cmd_exit;
	}

	nvm_cli_info_pr("Found %d instances", pblk->ninsts);
	for (int i = 0; i < pblk->ninsts; ++i)
		pblk_instance_pr(&pblk->insts[i]);

cmd_exit:
	free(pblk);
	return res;
}

//
// Remaining code is CLI boiler-plate
//

/* Define commands */
static struct nvm_cli_cmd cmds[] = {
	{
		"check",
		cmd_meta_check,
		NVM_CLI_ARG_DEV_PATH,
		NVM_CLI_OPT_HELP
	},
	{
		"lines",
		cmd_lines,
		NVM_CLI_ARG_DEV_PATH,
		NVM_CLI_OPT_HELP | NVM_CLI_OPT_BRIEF
	},
	{	"instances",
		cmd_instances,
		NVM_CLI_ARG_DEV_PATH,
		NVM_CLI_OPT_HELP
	},
};

/* Define the CLI */
static struct nvm_cli cli = {
	.title = "NVM pblk",
	.descr_short = "Dump and verify pblk meta data",
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
	if (!cli.opts.help && check_assumptions(&cli)) {
		goto exit;
	}

	res = nvm_cli_run(&cli);

exit:
	nvm_cli_destroy(&cli);

	return res;
}

