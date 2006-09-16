/*
 * $Id$
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "cli.h"
#include "cli_priv.h"
#include "cli_common.h"
#include "mgt.h"
#include "mgt_cli.h"

#include "vsb.h"
#include "heritage.h"

struct parspec;

typedef void tweak_t(struct cli *, struct parspec *, const char *arg);

struct parspec {
	const char	*name;
	tweak_t		*func;
	const char	*expl;
	const char	*def;
};

/*--------------------------------------------------------------------*/

static void
tweak_generic_timeout(struct cli *cli, unsigned *dst, const char *arg)
{
	unsigned u;

	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		if (u == 0) {
			cli_out(cli, "Timeout must be greater than zero\n");
			cli_result(cli, CLIS_PARAM);
			return;
		}
		*dst = u;
	} else
		cli_out(cli, "%u [seconds]\n", *dst);
}

/*--------------------------------------------------------------------*/

static void
tweak_default_ttl(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	if (arg != NULL)
		params->default_ttl = strtoul(arg, NULL, 0);
	else
		cli_out(cli, "%u [seconds]\n", params->default_ttl);
}

/*--------------------------------------------------------------------*/

static void
tweak_thread_pool_min(struct cli *cli, struct parspec *par, const char *arg)
{
	unsigned u;

	(void)par;
	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		if (u >= params->wthread_max) {
			cli_out(cli, "Minimum must be less than maximum\n");
			cli_result(cli, CLIS_PARAM);
			return;
		}
		params->wthread_min = u;
	} else
		cli_out(cli, "%u [threads]\n", params->wthread_min);
}

/*--------------------------------------------------------------------*/

static void
tweak_thread_pool_max(struct cli *cli, struct parspec *par, const char *arg)
{
	unsigned u;

	(void)par;
	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		if (u <= params->wthread_min) {
			cli_out(cli, "Maximum must be greater than minimum\n");
			cli_result(cli, CLIS_PARAM);
			return;
		}
		params->wthread_max = u;
	}
	if (params->wthread_max == UINT_MAX) 
		cli_out(cli, "unlimited\n");
	else 
		cli_out(cli, "%u [threads]\n", params->wthread_max);
}

/*--------------------------------------------------------------------*/

static void
tweak_thread_pool_timeout(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_timeout(cli, &params->wthread_timeout, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_http_workspace(struct cli *cli, struct parspec *par, const char *arg)
{
	unsigned u;

	(void)par;
	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		if (u <= 1024) {
			cli_out(cli, "Workspace must be at least 1024 bytes\n");
			cli_result(cli, CLIS_PARAM);
			return;
		}
		params->mem_workspace = u;
	} else
		cli_out(cli, "%u [bytes]\n", params->mem_workspace);
}

/*--------------------------------------------------------------------*/

static void
tweak_sess_timeout(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_timeout(cli, &params->sess_timeout, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_pipe_timeout(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_timeout(cli, &params->pipe_timeout, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_send_timeout(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_timeout(cli, &params->send_timeout, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_auto_restart(struct cli *cli, struct parspec *par, const char *arg)
{
	unsigned u;

	(void)par;
	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		if (u != 0 && u != 1) {
			cli_out(cli, "Only zero and one allowed.\n");
			cli_result(cli, CLIS_PARAM);
			return;
		}
		params->auto_restart = u;
	} else
		cli_out(cli, "%u {1 = yes, 0 = no}\n", params->auto_restart);
}

/*--------------------------------------------------------------------*/

static void
tweak_fetch_chunksize(struct cli *cli, struct parspec *par, const char *arg)
{
	unsigned u;

	(void)par;
	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		params->fetch_chunksize = u * 1024;
	} else
		cli_out(cli, "%u [kb]\n", params->fetch_chunksize * 1024);
}

#ifdef HAVE_SENDFILE
/*--------------------------------------------------------------------*/

static void
tweak_sendfile_threshold(struct cli *cli, struct parspec *par, const char *arg)
{
	unsigned u;

	(void)par;
	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		params->sendfile_threshold = u;
	} else
		cli_out(cli, "%u [bytes]\n", params->sendfile_threshold);
}
#endif /* HAVE_SENDFILE */

/*--------------------------------------------------------------------*/

static void
tweak_vcl_trace(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	if (arg != NULL) {
		if (!strcasecmp(arg, "off"))
			params->vcl_trace = 0;
		else if (!strcasecmp(arg, "disable"))
			params->vcl_trace = 0;
		else if (!strcasecmp(arg, "no"))
			params->vcl_trace = 0;
		else if (!strcasecmp(arg, "on"))
			params->vcl_trace = 1;
		else if (!strcasecmp(arg, "enable"))
			params->vcl_trace = 1;
		else if (!strcasecmp(arg, "yes"))
			params->vcl_trace = 1;
		else {
			cli_out(cli, "use \"on\" or \"off\"\n");
			cli_result(cli, CLIS_PARAM);
			return;
		}
	} else
		cli_out(cli, params->vcl_trace ? "on\n" : "off\n");
}

/*--------------------------------------------------------------------*/

static void
tweak_listen_address(struct cli *cli, struct parspec *par, const char *arg)
{
	char *a, *p;

	(void)par;
	if (arg != NULL) {
		if (TCP_parse(arg, &a, &p) != 0) {
			cli_out(cli, "Invalid listen address");
			cli_result(cli, CLIS_PARAM);
			return;
		}
		if (p == NULL) {
			p = strdup("http");
			AN(p);
		}
		TCP_check(cli, a, p);
		if (cli->result != CLIS_OK)
			return;
		free(params->listen_address);
		free(params->listen_host);
		free(params->listen_port);
		params->listen_address = strdup(arg);
		AN(params->listen_address);
		params->listen_host = a;
		params->listen_port = p;
	} else 
		cli_out(cli, "%s\n", params->listen_address);
}
/*--------------------------------------------------------------------*/

/*
 * Make sure to end all lines with either a space or newline of the
 * formatting will go haywire.
 */

#define DELAYED_EFFECT \
	"\nNB: This parameter will take some time to take effect.\n"

#define SHOULD_RESTART \
	"\nNB: This parameter will not take full effect until the " \
	"child process has been restarted.\n"

#define MUST_RESTART \
	"\nNB: This parameter will not take any effect until the " \
	"child process has been restarted.\n"


static struct parspec parspec[] = {
	{ "default_ttl", tweak_default_ttl,
		"The TTL assigned to objects if neither the backend nor "
		"the VCL code assigns one.\n"
		"Objects already cached will not be affected by changes "
		"made until they are fetched from the backend again.\n"
		"To force an immediate effect at the expense of a total "
		"flush of the cache use \"url.purge .\"\n"
		"Default is 120 seconds. ", "120" },
	{ "thread_pool_max", tweak_thread_pool_max,
		"The maximum number of threads in the worker pool.\n"
		DELAYED_EFFECT
		"Default is no limit.", "-1" },
	{ "thread_pool_min", tweak_thread_pool_min,
		"The minimum number of threads in the worker pool.\n"
		DELAYED_EFFECT
		"Default is 1 thread. " 
		"Minimum is 1 thread. ", "1" },
	{ "thread_pool_timeout", tweak_thread_pool_timeout,
		"Thread dies after this many seconds of inactivity.\n"
		"Default is 120 seconds. "
		"Minimum is 1 second. ", "120" },
	{ "http_workspace", tweak_http_workspace,
		"Bytes of HTTP protocol workspace allocated. "
		"This space must be big enough for the entire HTTP protocol "
		"header and any edits done to it in the VCL code.\n"
		SHOULD_RESTART
		"Default is 8192 bytes. "
		"Minimum is 1024 bytes. ", "8192" },
	{ "sess_timeout", tweak_sess_timeout,
		"Idle timeout for persistent sessions. "
		"If a HTTP request has not been received in this many "
		"seconds, the session is closed.\n"
		"Default is 5 seconds. ", "5" },
	{ "pipe_timeout", tweak_pipe_timeout,
		"Idle timeout for PIPE sessions. "
		"If nothing have been received in either directoin for "
	        "this many seconds, the session is closed.\n"
		"Default is 60 seconds. ", "60" },
	{ "send_timeout", tweak_send_timeout,
		"Send timeout for client connections. "
		"If no data has been sent to the client in this many seconds, "
		"the session is closed.\n"
		DELAYED_EFFECT
		"See getopt(3) under SO_SNDTIMEO for more information.\n"
		"Default is 600 seconds. ", "600" },
	{ "auto_restart", tweak_auto_restart,
		"Restart child process automatically if it dies. "
		"1 = yes, 0 = no.\n"
		"Default is 1. ", "1" },
	{ "fetch_chunksize", tweak_fetch_chunksize,
		"The default chunksize used by fetcher.\n"
		"Default is 128 kilobytes. ", "128" },
#ifdef HAVE_SENDFILE
	{ "sendfile_threshold", tweak_sendfile_threshold,
		"The minimum size of objects transmitted with sendfile.\n"
		"Default is 8192 bytes.", "8192" },
#endif /* HAVE_SENDFILE */
	{ "vcl_trace", tweak_vcl_trace,
		"Trace VCL execution in the shmlog\n"
		"Default is off", "off" },
	{ "listen_address", tweak_listen_address,
		"The network address/port where Varnish services requests.\n"
		MUST_RESTART
		"Default is \"0.0.0.0:80\"", "0.0.0.0:80" },
	{ NULL, NULL, NULL }
};

/*--------------------------------------------------------------------*/

void
mcf_param_show(struct cli *cli, char **av, void *priv)
{
	struct parspec *pp;
	const char *p, *q;
	int lfmt;

	(void)priv;
	if (av[2] == NULL || strcmp(av[2], "-l"))
		lfmt = 0;
	else
		lfmt = 1;
	for (pp = parspec; pp->name != NULL; pp++) {
		if (av[2] != NULL && !lfmt && strcmp(pp->name, av[2]))
			continue;
		cli_out(cli, "%-20s ", pp->name);
		if (pp->func == NULL) {
			cli_out(cli, "Not implemented.\n");
			if (av[2] != NULL && !lfmt) 
				return;
			else
				continue;
		}
		pp->func(cli, pp, NULL);
		if (av[2] != NULL) {
			/* Format text to 72 col width */
			for (p = pp->expl; *p != '\0'; ) {
				q = strchr(p, '\n');
				if (q == NULL)
					q = strchr(p, '\0');
				if (q > p + 52) {
					q = p + 52;
					while (q > p && *q != ' ')
						q--;
					AN(q);
				}
				cli_out(cli, "%20s %.*s\n", "", q - p, p);
				p = q;
				if (*p == ' ' || *p == '\n')
					p++;
			}
			if (!lfmt)
				return;
			else
				cli_out(cli, "\n");
		}
	}
	if (av[2] != NULL && !lfmt) {
		cli_result(cli, CLIS_PARAM);
		cli_out(cli, "Unknown paramter \"%s\".", av[2]);
	}
}

/*--------------------------------------------------------------------*/

void
MCF_ParamSet(struct cli *cli, const char *param, const char *val)
{
	struct parspec *pp;

	for (pp = parspec; pp->name != NULL; pp++) {
		if (!strcmp(pp->name, param)) {
			pp->func(cli, pp, val);
			return;
		}
	}
	cli_result(cli, CLIS_PARAM);
	cli_out(cli, "Unknown paramter \"%s\".", param);
}


/*--------------------------------------------------------------------*/

void
mcf_param_set(struct cli *cli, char **av, void *priv)
{

	(void)priv;
	MCF_ParamSet(cli, av[2], av[3]);
}

/*--------------------------------------------------------------------*/

void
MCF_ParamInit(struct cli *cli)
{
	struct parspec *pp;

	for (pp = parspec; pp->name != NULL; pp++)
		pp->func(cli, pp, pp->def);
}
