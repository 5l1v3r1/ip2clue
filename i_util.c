/*
 * Author: Catalin(ux) M. BOIE
 * Description: parser for IP files
 */

#include <i_config.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <i_util.h>

char		ip2clue_error[256];

/*
 * Returns ip2clue_error content
 */
char *ip2clue_strerror(void)
{
	return ip2clue_error;
}

/*
 * Show a nice IPv6 address
 * TODO: Replace with inet_ntop!
 */
int ip2clue_addr_v6(char *out, size_t out_size, const unsigned int *a)
{
	snprintf(out, out_size, "%x:%x:%x:%x:%x:%x:%x:%x",
		a[0] >> 16, a[0] & 0xFFFF,
		a[1] >> 16, a[1] & 0xFFFF,
		a[2] >> 16, a[2] & 0xFFFF,
		a[3] >> 16, a[3] & 0xFFFF);

	return 0;
}

/*
 * Compare two IPv6 addresses
 */
int ip2clue_compare_v6(const unsigned int *a, const unsigned int *b)
{
	unsigned int i;

	for (i = 0; i < 4; i++) {
		if (a[i] > b[i])
			return 1;
		else if (a[i] < b[i])
			return -1;
	}

	return 0;
}

/*
 * Split a line in fields.
 * @sep is an string that contains all possible separators
 * Returns number of fields.
 */
int ip2clue_split(struct ip2clue_split *s, const char *line, const char *sep)
{
	int len, i, j, inside_quote, separator_area;

	len = strlen(line);
	if (len == 0) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"empty string passed for splitting");
		return -1;
	}

	inside_quote = 0;
	separator_area = 0;
	s->count = 1;	/* There is at least one field! */
	j = 0;
	s->fields[s->count - 1][j] = '\0';
	for (i = 0; i < len; i++) {
		/* Parse only first 32 fields */
		if (s->count == 32)
			break;

		if ((line[i] == '\r') || (line[i] == '\n'))
			break;

		if (line[i] == '"') {
			if (inside_quote == 1) {
				inside_quote = 0;
			} else {
				inside_quote = 1;
			}
			continue;
		}

		if ((inside_quote == 0) && (strchr(sep, line[i]))) {
			/* Found separator */
			if (separator_area == 1)
				continue;

			/* Terminate previous string */
			s->fields[s->count - 1][j] = '\0';
			/* Move to next field */
			s->count++;
			j = 0;
			s->fields[s->count - 1][j] = '\0';
			separator_area = 1;
			continue;
		}

		separator_area = 0;

		/* Field unusually large? */
		if (j == sizeof(s->fields[s->count - 1]) - 2) {
			snprintf(ip2clue_error, sizeof(ip2clue_error),
				"field is too long");
			return -1;
		}

		s->fields[s->count - 1][j++] = line[i];
		s->fields[s->count - 1][j] = '\0';
	}

	/* Unterminated line? */
	if (inside_quote == 1) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"invalid number of quotes");
		return -1;
	}

	return s->count;
}

/*
 * Returns the number of lines for a file
 */
long ip2clue_file_lines(const char *file)
{
	char buf[1024 * 1024];
	int fd;
	ssize_t n, i;
	long ret = -1;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"cannot open file [%s] (%s)",
				file, strerror(errno));
		return -1;
	}

	ret = 0;
	while (1) {
		n = read(fd, buf, sizeof(buf));
		if (n == -1) {
			snprintf(ip2clue_error, sizeof(ip2clue_error),
				"cannot read (%s)", strerror(errno));
			close(fd);
			return -1;
		} else if (n == 0) {
			break;
		}

		for (i = 0; i < n; i++)
			if (buf[i] == '\n')
				ret++;
	}

	close(fd);

	return ret;
}

/*
 * Destroy data
 */
void ip2clue_destroy(struct ip2clue_db *db)
{
	unsigned int i;
	struct ip2clue_cell_v4 *p_cell4, *cell4;
	struct ip2clue_cell_v6 *p_cell6, *cell6;

	if (db == NULL)
		return;

	db->usage_count--;
	if (db->usage_count > 0)
		return;

	/* TODO: put 'for' under 'p_cell4 = ' */
	for (i = 0; i < db->no_of_cells; i++) {
		if (db->v4_or_v6 == IP2CLUE_TYPE_V4) {
			p_cell4 = (struct ip2clue_cell_v4 *) db->cells;
			cell4 = &p_cell4[i];
			if (cell4->extra != NULL)
				free(cell4->extra);
		} else {
			p_cell6 = (struct ip2clue_cell_v6 *) db->cells;
			cell6 = &p_cell6[i];
			if (cell6->extra != NULL)
				free(cell6->extra);
		}
	}

	if (db->cells != NULL)
		free(db->cells);

	free(db);
}


/*
 * Search for an IPv4
 */
struct ip2clue_cell_v4 *ip2clue_search_v4(struct ip2clue_db *db,
	const char *s_ip)
{
	unsigned int ip;
	struct in_addr in;
	long left, middle, right;
	struct ip2clue_cell_v4 *cells;

	if (inet_pton(AF_INET, s_ip, &in) != 1) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"malformed address");
		db->lookup_malformed++;
		return NULL;
	}

	ip = ntohl(in.s_addr);

	left = 0;
	right = db->no_of_cells - 1;
	cells = (struct ip2clue_cell_v4 *) db->cells;
	while (right >= left) {
		middle = (right + left + 1) / 2;

		/*
		printf("ip=%u left=%ld (%u->%u), middle=%ld (%u->%u), right=%ld (%u->%u)\n",
			ip,
			left, db->cells[left].ip_start, db->cells[left].ip_end,
			middle, db->cells[middle].ip_start, db->cells[middle].ip_end,
			right, db->cells[right].ip_start, db->cells[right].ip_end);
		*/

		if (ip > cells[middle].ip_end) {
			left = middle + 1;
			if (left == db->no_of_cells)
				break;
		} else if (ip < cells[middle].ip_start) {
			right = middle - 1;
			if (right == -1)
				break;
		} else {
			db->lookup_ok++;
			return &cells[middle];
		}
	}

	snprintf(ip2clue_error, sizeof(ip2clue_error),
		"cannot find address");
	db->lookup_notfound++;

	return NULL;
}

/*
 * search for an IPv6
 */
struct ip2clue_cell_v6 *ip2clue_search_v6(struct ip2clue_db *db, const char *s_ip)
{
	unsigned int ip[4], i;
	struct in6_addr in;
	long left, middle, right;
	struct ip2clue_cell_v6 *cells;
	/*char a1[64], a2[64], a3[64];*/

	if (inet_pton(AF_INET6, s_ip, &in) != 1) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"malformed address");
		db->lookup_malformed++;
		return NULL;
	}

	for (i = 0; i < 4; i++)
		ip[i] = ntohl(in.s6_addr32[i]);

	left = 0;
	right = db->no_of_cells - 1;
	cells = (struct ip2clue_cell_v6 *) db->cells;
	while (right >= left) {
		middle = (right + left + 1) / 2;

		/*
		ip2clue_addr_v6(a1, sizeof(a1), ip);
		ip2clue_addr_v6(a2, sizeof(a2), cells[middle].ip_start);
		ip2clue_addr_v6(a3, sizeof(a3), cells[middle].ip_end);
		printf("Comparing ip=%s with ip_start=%s ip_end=%s...\n",
			a1, a2, a3);
		*/
		if (ip2clue_compare_v6(ip, cells[middle].ip_end) > 0) {
			left = middle + 1;
			if (left == db->no_of_cells)
				break;
		} else if (ip2clue_compare_v6(ip, cells[middle].ip_start) < 0) {
			right = middle - 1;
			if (right == -1)
				break;
		} else {
			db->lookup_ok++;
			return &cells[middle];
		}
	}

	snprintf(ip2clue_error, sizeof(ip2clue_error),
		"cannot find address");
	db->lookup_notfound++;

	return NULL;
}

/*
 * Dump info about a cell
 */
void ip2clue_dump_cell_v6(char *out, size_t out_size, struct ip2clue_cell_v6 *cell)
{
	char s[64], e[64];

	ip2clue_addr_v6(s, sizeof(s), cell->ip_start);
	ip2clue_addr_v6(e, sizeof(e), cell->ip_end);
	snprintf(out, out_size, "%s %s -> %s", cell->country_short, s, e);
	/* TODO: dump extra! */
}

/*
 * Print extra structure
 */
void ip2clue_print_extra(char *out, size_t out_size, const struct ip2clue_extra *e)
{
	snprintf(out, out_size, "country_long=%s region=%s city=%s isp=%s latitude=%f"
		" longitude=%f zip=%s domain=%s timezone=%s netspeed=%s"
		" idd=%s areacode=%s ws_code=%s ws_name=%s\n",
		e->country_long, e->region, e->city, e->isp, e->latitude,
		e->longitude, e->zip, e->domain, e->timezone, e->netspeed,
		e->idd, e->areacode, e->ws_code, e->ws_name);
}

/*
 * Search an IP in a list
 */
struct ip2clue_cell_v4 *ip2clue_list_search_v4(struct ip2clue_list *list,
	const char *ip)
{
	unsigned int i;
	struct ip2clue_db *db;
	struct ip2clue_cell_v4 *ret = NULL;

	for (i = 0; i < list->number; i++) {
		db = list->entries[i];
		if (db->v4_or_v6 != IP2CLUE_TYPE_V4)
			continue;

		ret = ip2clue_search_v4(db, ip);
		if (ret != NULL)
			break;
	}

	if (ret == NULL)
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"not found");

	return ret;
}

/*
 * Returns a IPv4 address from a special IPv6 address
 * 2002:xxyy:zztt::/48, ::a.b.c.d and ::ffff:a.b.c.d
 */
/* TODO */

/*
 * Search an IP in a list
 */
struct ip2clue_cell_v6 *ip2clue_list_search_v6(struct ip2clue_list *list,
	const char *ip)
{
	unsigned int i;
	struct ip2clue_db *db;
	struct ip2clue_cell_v6 *ret = NULL;

	for (i = 0; i < list->number; i++) {
		db = list->entries[i];
		if (db->v4_or_v6 != IP2CLUE_TYPE_V6)
			continue;

		ret = ip2clue_search_v6(db, ip);
		if (ret != NULL)
			break;
	}

	if (ret == NULL) {
		/* TODO: Now, try special IPv4 encapsulated in IPv6 addresses */
	}

	if (ret == NULL)
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"not found");

	return ret;
}

/*
 * Builds a string answer
 * Returns 0 if address not found or other errors received, else 1.
 */
int ip2clue_list_search(struct ip2clue_list *list, char *out,
	const unsigned int out_size, const char *format, const char *ip)
{
	struct ip2clue_cell_v4 *v4;
	struct ip2clue_cell_v6 *v6;
	struct ip2clue_extra *e;
	char cs[4];
	unsigned int rest, i, format_size, special, a_len;
	char a[256];

	if (strchr(ip, '.')) {
		v4 = ip2clue_list_search_v4(list, ip);
		if (v4 == NULL)
			return 0;

		strcpy(cs, v4->country_short);
		e = v4->extra;
	} else {
		v6 = ip2clue_list_search_v6(list, ip);
		if (v6 == NULL)
			return 0;

		strcpy(cs, v6->country_short);
		e = v6->extra;
	}

	strcpy(out, "");
	rest = out_size - 1;
	format_size = strlen(format);
	special = 0;
	for (i = 0; i < format_size; i++) {
		strcpy(a, "");

		if (format[i] == '%') {
			if (special == 1) {
				strcpy(a, "%");
				special = 0;
			} else {
				special = 1;
				continue;
			}
		} else if (special == 1) {
			if (format[i] == 's') {
				snprintf(a, sizeof(a), "%s", cs);
			} else if (format[i] == 'P') {
				snprintf(a, sizeof(a), "%s", ip);
			} else if (e != NULL) {
				switch (format[i]) {
				case 'L': snprintf(a, sizeof(a), "%s", e->country_long); break;
				case 'r': snprintf(a, sizeof(a), "%s", e->region); break;
				case 'c': snprintf(a, sizeof(a), "%s", e->city); break;
				case 'i': snprintf(a, sizeof(a), "%s", e->isp); break;
				case 'x': snprintf(a, sizeof(a), "%f", e->latitude); break;
				case 'y': snprintf(a, sizeof(a), "%f", e->longitude); break;
				case 'z': snprintf(a, sizeof(a), "%s", e->zip); break;
				case 'd': snprintf(a, sizeof(a), "%s", e->domain); break;
				case 't': snprintf(a, sizeof(a), "%s", e->timezone); break;
				case 'n': snprintf(a, sizeof(a), "%s", e->netspeed); break;
				case 'k': snprintf(a, sizeof(a), "%s", e->idd); break;
				case 'a': snprintf(a, sizeof(a), "%s", e->areacode); break;
				case 'w': snprintf(a, sizeof(a), "%s", e->ws_code); break;
				case 'W': snprintf(a, sizeof(a), "%s", e->ws_name); break;
				}
			}
		} else {
			snprintf(a, 2, "%c", format[i]);
		}

		special = 0;

		a_len = strlen(a);
		if (a_len > rest) {
			snprintf(ip2clue_error, sizeof(ip2clue_error),
				"output buffer too short to add"
				" more %u bytes (%s)",
				a_len, a);
			return 0;
		}

		strcat(out, a);
		rest -= a_len;
	}

	return 1;
}

