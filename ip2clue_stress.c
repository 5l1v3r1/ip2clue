#include <i_config.h>

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <Conn.h>

#include <i_types.h>
#include <i_util.h>
#include <i_conf.h>
#include <parser.h>

#define REQUESTS 50000

static FILE			*Logf = NULL;
static int			conf_port = 9999;

static char			ips[REQUESTS][64];
static unsigned int		ips_now = 0;

static void send_data(struct Conn *C)
{
	char cmd[64];

	snprintf(cmd, sizeof(cmd), "R%s\n", ips[ips_now]);
	ips_now++;
	if (Conn_enqueue(C, cmd, strlen(cmd)) == -1) {
		Log(0, "Cannot enqueue (%s)!\n",
			Conn_strerror());
	}
}

static void connected_cb(struct Conn *C)
{

	send_data(C);
}

static int data_cb(struct Conn *C, char *line)
{
	/*printf("%s\n", line);*/

	if (ips_now == REQUESTS)
		Conn_close(C);
	else
		send_data(C);

	return 0;
}

static void data(struct Conn *C)
{
	Conn_for_every_line(C, data_cb);
}


static void error(struct Conn *C)
{
	Log(0, "Error on id=%llu (%s).\n",
		Conn_getid(C), Conn_strerror());
}

int main(int argc, char *argv[])
{
	struct Conn *C;
	int ret;
	unsigned int i;

	Conn_debug(Logf, 0);

	if (argc >= 3)
		conf_port = strtol(argv[2], NULL, 10);

	ret = Conn_init(0);
	if (ret == -1) {
		Log(0, "Cannot init Conn (%s)!\n",
			Conn_strerror());
		return 1;
	}

	/* set callbacks */
	Conn_data_cb = data;
	Conn_error_cb = error;
	Conn_connected_cb = connected_cb;

	/* build data */
	for (i = 0; i < REQUESTS; i++) {
		snprintf(ips[i], sizeof(ips[i]), "%ld.%ld.%ld.%ld",
			random() % 255, random() % 255, random() % 255, random() %255);
	}

	/* prefer IPv6 */
	Log(1, "IPv6...\n");
	C = Conn_alloc();
	if (!C) {
		Log(0, "Cannot alloc a Conn v6 structure (%s)!\n",
			Conn_strerror());
		return 1;
	}

	Conn_set_socket_domain(C, PF_INET6);
	Conn_set_socket_type(C, SOCK_STREAM);
	Conn_set_socket_addr(C, "::1");
	Conn_set_socket_port(C, conf_port);
	ret = Conn_commit(C);
	if (ret != 0) {
		Log(0, "Cannot commit v6 (%s)!\n",
			Conn_strerror());

		Log(1, "IPv4...\n");
		Conn_set_socket_domain(C, PF_INET);
		Conn_set_socket_type(C, SOCK_STREAM);
		Conn_set_socket_addr(C, "127.0.0.1");
		Conn_set_socket_port(C, conf_port);
		ret = Conn_commit(C);
		if (ret != 0) {
			Log(0, "Cannot commit v4 (%s)!\n",
				Conn_strerror());
			return 1;
		}
	}

	Log(1, "Start polling...\n");
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

	return 0;
}
