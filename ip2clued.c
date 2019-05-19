#include <i_config.h>

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include <Conn.h>

#include <i_types.h>
#include <i_util.h>
#include <i_conf.h>
#include <parser.h>

static FILE			*Logf = NULL;
static char			*log_file = "/var/log/ip2clued.log";

/* conf stuff */
static char			*conf_filename = "/etc/ip2clue/ip2clued.conf";
static char			*conf_datadir;
static char			*conf_files;
static char			*conf_format;
static unsigned int		conf_refresh;
static unsigned int		conf_port;
static unsigned int		conf_ipv4;
static unsigned int		conf_ipv6;
static unsigned int		conf_debug;
static unsigned int		conf_nodaemon;

/* This will protect accesses to list 'list' */
static pthread_rwlock_t		list_rwlock;

static struct ip2clue_list	list;


static int data_cb(struct Conn *C, char *line)
{
	int err, close = 0;
	char out[2048], *p;

	switch (line[0]) {
	case 'R':
		line += 1;

		p = strchr(line, ' ');
		if (p)
			*p = '\0';

		pthread_rwlock_rdlock(&list_rwlock);

		err = ip2clue_list_search(&list, out, sizeof(out) - 1,
			conf_format, line);
		if (err < 1)
			snprintf(out, sizeof(out), "ER ip=%s errmsg=\"%s\"",
				line, ip2clue_strerror());

		pthread_rwlock_unlock(&list_rwlock);

		break;

	case 'S':
		pthread_rwlock_rdlock(&list_rwlock);
		ip2clue_list_stats(out, sizeof(out) - 1, &list);
		pthread_rwlock_unlock(&list_rwlock);
		break;

	/* This is only for debug
	case 'Q':
		strcpy(out, "Bye!");
		Conn_stop();
		break;
	*/

	case 'q':
	case 'Q':
		strcpy(out, "Bye!");
		close = 1;
		break;

	default:
		strcpy(out, "errmsg=\"Invalid command\"");
		break;
	}

	strcat(out, "\n");

	err = Conn_enqueue(C, out, strlen(out));
	if (err == -1) {
		Log(0, "Error in enqueue (%s)!\n",
			Conn_strerror());
		Conn_close(C);
	}

	if (close == 1)
		Conn_close(C);

	return 0;
}

static void data(struct Conn *C)
{
	Conn_for_every_line(C, data_cb);
}


static void error(struct Conn *C)
{
	Log(1, "Error on id=%llu (%s).\n",
		Conn_getid(C), Conn_strerror());
}

static void *worker_loader(void *arg)
{
	int ret;
	struct ip2clue_list list2;

	Log(0, "Loader worker started...\n");
	while (1) {
		/* Test if we need to reload files */

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		ret = ip2clue_list_refresh(&list2, &list, conf_datadir, conf_files);
		if (ret != 0) {
			Log(0, "Cannot refresh list (%s)!"
				" Sleeping and try again later...\n",
				ip2clue_strerror());
		} else {
			pthread_rwlock_wrlock(&list_rwlock);
			ret = ip2clue_list_replace(&list, &list2);
			if (ret != 0)
				Log(0, "Cannot replace list (%s)!\n",
					ip2clue_strerror());
			pthread_rwlock_unlock(&list_rwlock);
		}

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		if (conf_refresh == 0)
			break;

		sleep(conf_refresh);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	struct Conn *C4 = NULL, *C6 = NULL;
	int ret;
	pthread_t workers[1];
	void *res;
	struct ip2clue_conf *conf;

	Logf = fopen(log_file, "w");
	if (!Logf) {
		fprintf(stderr, "Cannot open log file [%s] (%s)!\n",
			log_file, strerror(errno));
		return 1;
	}
	setlinebuf(Logf);


	/* Load config file */
	if (argc > 1)
		conf_filename = argv[1];
	Log(1, "Loading configuration from file [%s]...\n", conf_filename);
	conf = ip2clue_conf_load(conf_filename);
	if (conf == NULL) {
		fprintf(stderr, "Cannot load conf file [%s]!\n", conf_filename);
		return 1;
	}

	/* load variables */
	conf_datadir = ip2clue_conf_get(conf, "datadir");
	conf_files = ip2clue_conf_get(conf, "files");
	conf_format = ip2clue_conf_get(conf, "format");
	conf_refresh = ip2clue_conf_get_ul(conf, "refresh", 10);
	conf_port = ip2clue_conf_get_ul(conf, "port", 10);
	conf_ipv4 = ip2clue_conf_get_ul(conf, "ipv4", 10);
	conf_ipv6 = ip2clue_conf_get_ul(conf, "ipv6", 10);
	conf_debug = ip2clue_conf_get_ul(conf, "debug", 10);
	conf_nodaemon = ip2clue_conf_get_ul(conf, "nodaemon", 10);

	if (!conf_datadir || !conf_files) {
		fprintf(stderr, "ERROR: 'datadir = ' or 'files = '"
			" missing from conf file!\n");
		return 1;
	}

	if ((conf_ipv4 == 0) && (conf_ipv6 == 0)) {
		fprintf(stderr, "ERROR: Both IPv4 and IPv6 are disabled!"
			" Set ipv4 and/or ipv6 to '1'!\n");
		return 1;
	}

	if (!conf_format)
		conf_format = "OK ip=%P cs=%s tz=%t isp=%i";

	if (conf_port == 0)
		conf_port = 9999;

	Log(1, "Parameters: datadir=[%s] files=[%s] format=[%s]"
		" refresh=%lu port=%u ipv4=%u ipv6=%u"
		" debug=%u nodaemon=%u",
		conf_datadir, conf_files, conf_format,
		conf_refresh, conf_port, conf_ipv4, conf_ipv6,
		conf_debug, conf_nodaemon);


	if (conf_nodaemon == 0)
		daemon(0, 0);


	ip2clue_list_init(&list);


	Conn_debug(Logf, conf_debug);

	Log(0, "Starting 'files reload' thread...\n");
	ret = pthread_create(&workers[0], NULL, worker_loader, NULL);
	if (ret != 0) {
		Log(0, "Cannot start loader thread (%s)!\n",
			strerror(errno));
		return 1;
	}


	ret = Conn_init(0);
	if (ret == -1) {
		Log(0, "Cannot init Conn (%s)!\n",
			Conn_strerror());
		return 1;
	}

	/* set callbacks */
	Conn_data_cb = data;
	Conn_error_cb = error;

	if (conf_ipv4 == 1) {
		Log(0, "IPv4...\n");
		C4 = Conn_alloc();
		if (!C4) {
			Log(0, "Cannot alloc a Conn v4 structure (%s)!\n",
				Conn_strerror());
			return 1;
		}
		Conn_set_socket_domain(C4, PF_INET);
		Conn_set_socket_type(C4, SOCK_STREAM);
		Conn_set_socket_bind_addr(C4, "0.0.0.0");
		Conn_set_socket_bind_port(C4, conf_port);
		ret = Conn_commit(C4);
		if (ret != 0) {
			Log(0, "Cannot commit (%s)!\n",
				Conn_strerror());
			return 1;
		}
	}

	if (conf_ipv6 == 1) {
		Log(0, "IPv6...\n");
		C6 = Conn_alloc();
		if (!C6) {
			Log(0, "Cannot alloc a Conn v6 structure (%s)!\n",
				Conn_strerror());
			return 1;
		}
		Conn_set_socket_domain(C6, PF_INET6);
		Conn_set_socket_type(C6, SOCK_STREAM);
		Conn_set_socket_bind_addr(C6, "::");
		Conn_set_socket_bind_port(C6, conf_port);
		ret = Conn_commit(C6);
		if (ret != 0) {
			Log(0, "Cannot commit (%s)!\n",
				Conn_strerror());
			return 1;
		}
	}

	Log(0, "Master starts polling...\n");
	while (1) {
		ret = Conn_poll(-1);
		if (ret == -1) {
			Log(0, "Error in Conn_poll (%s)!\n",
				Conn_strerror());
			break;
		} else if (ret == 0) {
			break;
		}
	}

	Conn_shutdown();

	ret = pthread_cancel(workers[0]);
	if (ret != 0) {
		Log(0, "Cannot cancel thread (%s)!\n",
			strerror(errno));
	}

	ret = pthread_join(workers[0], &res);
	if (ret != 0) {
		Log(0, "Cannot join thread (%s)!\n",
			strerror(errno));
	}

	ip2clue_list_destroy(&list);

	ip2clue_conf_free(conf);

	fclose(Logf);

	return 0;
}
