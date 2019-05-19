/*
 * Author: Catalin(ux) M. BOIE
 * Description: parser for IP files
 * ip2location stuff
 */

#include <i_config.h>

#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <error.h>
#include <string.h>

#include <i_types.h>
#include <i_util.h>
#include <parser_ip2location.h>

enum ip2clue_ip2location_item
{
	IP2CLUE_IP2LOCATION_COUNTRY = 0,
	IP2CLUE_IP2LOCATION_REGION,
	IP2CLUE_IP2LOCATION_CITY,
	IP2CLUE_IP2LOCATION_ISP,
	IP2CLUE_IP2LOCATION_LAT,
	IP2CLUE_IP2LOCATION_LON,
	IP2CLUE_IP2LOCATION_DOMAIN,
	IP2CLUE_IP2LOCATION_ZIPCODE,
	IP2CLUE_IP2LOCATION_TZ,
	IP2CLUE_IP2LOCATION_NETSPEED,
	IP2CLUE_IP2LOCATION_IDD,
	IP2CLUE_IP2LOCATION_AREACODE,
	IP2CLUE_IP2LOCATION_WSC,
	IP2CLUE_IP2LOCATION_WSN
};

#define IP2CLUE_IP2LOCATION_ITEMS 14
static unsigned char ip2clue_ip2location_lut[IP2CLUE_IP2LOCATION_ITEMS][19] =
{
	{0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
	{0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
	{0, 0, 3, 0, 5, 0, 7, 5, 7, 0, 8, 0, 9, 0, 9, 0, 9, 0, 9},
	{0, 0, 0, 0, 0, 5, 5, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
	{0, 0, 0, 0, 0, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
	{0, 0, 0, 0, 0, 0, 0, 6, 8, 0, 9, 0,10, 0,10, 0,10, 0,10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 7, 0, 7, 7, 7, 0, 7},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 7, 8, 8, 8, 7, 8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8,11, 0,11, 8,11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9,12, 0,12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,10,13, 0,13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9,14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,10,15}
};

/*
 * Returns position for a db_type and for an item
 */
static unsigned char ip2clue_ip2location_pos(const unsigned db_type,
	const enum ip2clue_ip2location_item item)
{
	return ip2clue_ip2location_lut[item][db_type];
}

/*
 * Read 8 bits
 */
static int xread8(unsigned char *ret, const int fd, off_t off)
{
	off_t x;
	ssize_t n;

	x = lseek(fd, off, SEEK_SET);
	if (x != off) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"cannot search to position %lld (%s)",
			(long long) off, strerror(errno));
		return -1;
	}

	n = read(fd, ret, 1);
	if (n != 1) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"cannot read (%s) n=%d at off %lld",
			strerror(errno), n, (long long) off);
		return -1;
	}

	/*
	printf("%s: readed value %u at offset %lld!\n",
		__FUNCTION__, *ret, (long long) off);
	*/

	return 0;
}

/*
 * Read 32 bits
 */
static int xread32(unsigned int *ret, const int fd, off_t off)
{
	off_t x;
	ssize_t n;
	unsigned char c[4];

	x = lseek(fd, off, SEEK_SET);
	if (x != off)
		return -1;

	n = read(fd, &c, 4);
	if (n != 4)
		return -1;

	*ret = (c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];

	return 0;
}

/*
 * Read 128 bits
 * Put in out 128 bits
 */
static int xread128(unsigned int *out, const int fd, off_t off)
{
	unsigned int v;

	if (xread32(&v, fd, off) != 0)
		return -1;
	out[3] = v;

	if (xread32(&v, fd, off + 4) != 0)
		return -1;
	out[2] = v;

	if (xread32(&v, fd, off + 8) != 0)
		return -1;
	out[1] = v;

	if (xread32(&v, fd, off + 12) != 0)
		return -1;
	out[0] = v;

	return 0;
}
/*
 * Read a float
 */
static int xread_float(float *ret, const int fd, off_t off)
{
	off_t x;
	ssize_t n;

	x = lseek(fd, off, SEEK_SET);
	if (x != off)
		return -1;

	n = read(fd, ret, 4);
	if (n != 4)
		return -1;

	/*
	printf("Readed float %f from offset %lld!\n",
		*ret, (long long) off);
	*/

	return 0;
}

/*
 * Read a string
 */
static char *xread_str(char *out, const size_t out_len, const int fd,
	const off_t off, unsigned int extra_add)
{
	unsigned char len;
	static char v[128];
	ssize_t n;
	unsigned int real_offset;

	/* first, read offset */
	if (xread32(&real_offset, fd, off) != 0)
		return NULL;

	real_offset += extra_add;

	if (xread8(&len, fd, real_offset) != 0)
		return NULL;

	strcpy(v, "");

	if (len == 0)
		return v;

	if (len > out_len - 1)
		len = out_len - 1;

	n = read(fd, out, len);
	if (n != len) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"cannot read at offset %lld, len=%d, n=%d (%s)",
			(long long) off, len, n, strerror(errno));
		return NULL;
	}
	out[n] = '\0';

	/*
	printf("String at offset %lld, with len=%d is [%s]!\n",
		(long long) off, len, out);
	*/

	return 0;
}

/*
 * Substract one from IPv6 address
 */
void substract(unsigned int *a)
{
	int i;

	for (i = 3; i >= 0; i--)
		if (a[i] == 0) {
			a[i] = 0xFFFFFFFFUL;
		} else {
			a[i] = a[i] - 1;
			break;
		}
}

/*
 * IP2country specific file
 */
int ip2clue_parse_ip2location(struct ip2clue_db *db)
{
	int fd;
	unsigned int i, j;
	unsigned char db_type, db_columns, db_year, db_month, db_day;
	unsigned int db_count, db_addr, ip_version;
	struct ip2clue_cell_v4 *p_cell4, *cell4 = NULL;
	struct ip2clue_cell_v6 *p_cell6, *cell6 = NULL;
	struct ip2clue_extra x;
	unsigned int x_set = 0;
	unsigned char pos;
	unsigned int off, cur, next;
	char *s;
	unsigned int add, final, s_len;
	struct ip2clue_extra *e;
	/*char src[60], dst[60];*/
	struct tm tm;
	/*char dump[256];*/

	fd = open(db->file, O_RDONLY);
	if (fd == -1) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"cannot open [%s] (%s)",
			db->file, strerror(errno));
		return -1;
	}

	if (xread8(&db_type, fd, 0) != 0)
		goto out_close;
	if (xread8(&db_columns, fd, 1) != 0)
		goto out_close;
	if (xread8(&db_year, fd, 2) != 0)
		goto out_close;
	if (xread8(&db_month, fd, 3) != 0)
		goto out_close;
	if (xread8(&db_day, fd, 4) != 0)
		goto out_close;
	if (xread32(&db_count, fd, 5) != 0)
		goto out_close;
	db_count--;
	if (xread32(&db_addr, fd, 9) != 0)
		goto out_close;
	db_addr--;
	if (xread32(&ip_version, fd, 13) != 0)
		goto out_close;

	/*
	printf("type=%u, columns=%u, year=%u, month=%u, day=%u, count=%u"
		", addr=%u, ip_version=%u.\n",
		db_type, db_columns, db_year, db_month, db_day, db_count,
		db_addr, ip_version);
	*/

	memset(&tm, 0, sizeof(struct tm));
	tm.tm_year = db_year + 2000 - 1900;
	tm.tm_mon = db_month - 1;
	tm.tm_mday = db_day;
	db->ts = mktime(&tm);

	if (ip_version == 0)
		db->v4_or_v6 = 4;
	else
		db->v4_or_v6 = 6;
	db->no_of_cells = db_count;

	/* alloc memory for entries */
	if (db->v4_or_v6 == IP2CLUE_TYPE_V6) {
		db->mem = db->no_of_cells * sizeof(struct ip2clue_cell_v6);
	} else {
		db->mem = db->no_of_cells * sizeof(struct ip2clue_cell_v4);
	}
	db->cells = malloc(db->mem);
	if (db->cells == NULL)
		goto out_close;

	/* parse file */
	db->current = 0;
	while (db->current < db->no_of_cells) {
		/*
		printf("Loading struct %llu/%llu...\r",
			db->current + 1, db->no_of_cells);
		*/

		x_set = 0; /* alloc or not extra structure? Default, not. */
		memset(&x, 0, sizeof(struct ip2clue_extra));

		if (db->v4_or_v6 == IP2CLUE_TYPE_V6) {
			p_cell6 = (struct ip2clue_cell_v6 *) db->cells;
			cell6 = &p_cell6[db->current];
			memset(cell6, 0, sizeof(struct ip2clue_cell_v6));

			cur = db_addr + db->current * (db_columns * 4 + 12);
			next = db_addr + (db->current + 1) * (db_columns * 4 + 12);
			/*printf("cur=%u, next=%u\n", cur, next);*/

			if (xread128(cell6->ip_start, fd, cur) != 0)
				goto out_parse_error;
			if (xread128(cell6->ip_end, fd, next) != 0)
				goto out_parse_error;

			/* final? */
			final = 1;
			for (j = 0; j < 4; j++) {
				if (cell6->ip_end[j] != 0xFFFFFFFFUL) {
					final = 0;
					break;
				}
			}

			/* We need to substract 1, else the interval clashes on ends */
			if (final == 0)
				substract(cell6->ip_end);

			/*
			ip2clue_addr_v6(src, sizeof(src), cell6->ip_start);
			ip2clue_addr_v6(dst, sizeof(src), cell6->ip_end);

			printf("\tstart=%s, end=%s\n", src, dst);
			*/

			add = 12;
		} else {
			p_cell4 = (struct ip2clue_cell_v4 *) db->cells;
			cell4 = &p_cell4[db->current];
			memset(cell4, 0, sizeof(struct ip2clue_cell_v4));

			cur = db_addr + db->current * (db_columns * 4);
			next = db_addr + (db->current + 1) * (db_columns * 4);
			/*printf("cur=%u, next=%u\n", cur, next);*/

			if (xread32(&cell4->ip_start, fd, cur) != 0)
				goto out_parse_error;
			if (xread32(&cell4->ip_end, fd, db_addr + (db->current + 1) * db_columns * 4) != 0)
				goto out_parse_error;
			cell4->ip_end--;

			/* final? */
			final = 1;
			if (cell4->ip_end != 4294967295UL)
				final = 0;

			/*
			printf("\tstart=%u, end=%u\n",
				cell4->ip_start, cell4->ip_end);
			*/

			add = 0;
		}

		for (i = 0; i < IP2CLUE_IP2LOCATION_ITEMS; i++) {
			pos = ip2clue_ip2location_pos(db_type, i);
			if (pos == 0)
				continue;

			/* default offset */
			off = cur + add + 4 * (pos - 1);

			switch (i) {
			case IP2CLUE_IP2LOCATION_COUNTRY:
				/* short */
				if (db->v4_or_v6 == IP2CLUE_TYPE_V6) {
					s = cell6->country_short;
					s_len = sizeof(cell6->country_short);
				} else {
					s = cell4->country_short;
					s_len = sizeof(cell4->country_short);
				}
				if (xread_str(s, s_len, fd, off, 0) != 0)
					goto out_parse_error;
				/*printf("Short: %s, ", s);*/

				/* long */
				if (xread_str(x.country_long,
					sizeof(x.country_long), fd, off, 3) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_REGION:
				if (xread_str(x.region, sizeof(x.region), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_CITY:
				if (xread_str(x.city, sizeof(x.city), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_ISP:
				if (xread_str(x.isp, sizeof(x.isp), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_LAT:
				if (xread_float(&x.latitude, fd, off) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_LON:
				if (xread_float(&x.longitude, fd, off) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_DOMAIN:
				if (xread_str(x.domain, sizeof(x.domain), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_ZIPCODE:
				if (xread_str(x.zip, sizeof(x.zip), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_TZ:
				if (xread_str(x.timezone, sizeof(x.timezone), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_NETSPEED:
				if (xread_str(x.netspeed, sizeof(x.netspeed), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_IDD:
				if (xread_str(x.idd, sizeof(x.idd), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_AREACODE:
				if (xread_str(x.areacode, sizeof(x.areacode), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_WSC:
				if (xread_str(x.ws_code, sizeof(x.ws_code), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			case IP2CLUE_IP2LOCATION_WSN:
				if (xread_str(x.ws_name, sizeof(x.ws_name), fd, off, 0) != 0)
					goto out_parse_error;
				x_set = 1;
				break;

			default:
				snprintf(ip2clue_error, sizeof(ip2clue_error),
					"unknown item type (%u)", i);
				goto out_parse_error;
			}
		}

		if (x_set == 1) {
			e = (struct ip2clue_extra *) malloc(sizeof(struct ip2clue_extra));
			if (e == NULL) {
				snprintf(ip2clue_error, sizeof(ip2clue_error),
					"cannot alloc memory");
				goto out_parse_error;
			}
			memcpy(e, &x, sizeof(struct ip2clue_extra));
			/*
			ip2clue_print_extra(dump, sizeof(dump), e);
			printf("Extra: %s.\n", dump);
			*/
		} else {
			e = NULL;
		}

		if (db->v4_or_v6 == IP2CLUE_TYPE_V6) {
			cell6->extra = e;
		} else {
			cell4->extra = e;
		}

		db->current++;

		/* just a safety */
		if (final == 1)
			break;
	}

	close(fd);

	return 0;

	out_parse_error:
	ip2clue_destroy(db);
	free(db);

	out_close:
	close(fd);

	return -1;
}
