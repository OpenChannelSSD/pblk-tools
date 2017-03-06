#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <liblightnvm_cli.h>

int mdck(struct nvm_cli *cli)
{
	return 0;
}

/**
 * Command-line interface (CLI) boiler-plate
 */

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
		perror("FAILED");
		return 1;
	}

	res = nvm_cli_run(&cli);
	if (res)
		perror(cli.cmd.name);
	
	nvm_cli_destroy(&cli);

	return res;
}
