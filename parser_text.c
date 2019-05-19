/*
 * Author: Catalin(ux) M. BOIE
 * Description: parser for IP files
 */

#include <i_config.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <i_types.h>
#include <i_util.h>
#include <parser_core.h>
#include <parser_text.h>
#include <parser_ip2location.h>

static int ip2clue_set_fields(struct ip2clue_fields *f,
	const enum ip2clue_type type)
{
	/* defaults */
	strcpy(f->comment_chars, "");
	f->v4_or_v6 = IP2CLUE_TYPE_V4;

	switch (type) {
	case IP2CLUE_FORMAT_WEBHOSTING:
		f->ip_bin_start = 0;
		f->ip_bin_end = 1;
		f->country_short = 2;
		f->country_long = 4;
		f->total = 5;
		return 0;

	case IP2CLUE_FORMAT_MAXMIND:
		f->ip_bin_start = 2;
		f->ip_bin_end = 3;
		f->country_short = 4;
		f->country_long = 5;
		f->total = 6;
		return 0;

	case IP2CLUE_FORMAT_MAXMIND_V6:
		f->v4_or_v6 = IP2CLUE_TYPE_V6;
		f->ip_start = 0;
		f->ip_end = 1;
		f->country_short = 4;
		f->country_long = 5;
		f->total = 6;
		return 0;

	case IP2CLUE_FORMAT_SOFTWARE77:
		strcpy(f->comment_chars, "#");
		f->ip_bin_start = 0;
		f->ip_bin_end = 1;
		f->country_short = 4;
		f->country_long = 6;
		f->total = 7;
		return 0;

	default:
		return -1;
	}
}

/*
 * Add a cell to the db
 */
static int ip2clue_add_cell(struct ip2clue_db *db, const struct ip2clue_split *s,
	const struct ip2clue_fields *f)
{
	struct ip2clue_cell_v4 *p_cell4, *cell4;
	struct ip2clue_cell_v6 *p_cell6, *cell6;
	int i;

	if (db->v4_or_v6 == IP2CLUE_TYPE_V4) {
		p_cell4 = (struct ip2clue_cell_v4 *) db->cells;
		cell4 = &p_cell4[db->current];

		cell4->extra = NULL;

		cell4->ip_start = strtoul(s->fields[f->ip_bin_start], NULL, 10);

		cell4->ip_end = strtoul(s->fields[f->ip_bin_end], NULL, 10);

		if (f->country_short > 0)
			snprintf(cell4->country_short, sizeof(cell4->country_short),
				"%s",
				s->fields[f->country_short]);
		else
			strcpy(cell4->country_short, "ZZ");
	} else if (db->v4_or_v6 == IP2CLUE_TYPE_V6) {
		struct in6_addr addr;

		p_cell6 = (struct ip2clue_cell_v6 *) db->cells;
		cell6 = &p_cell6[db->current];

		cell6->extra = NULL;

		if (inet_pton(AF_INET6, s->fields[f->ip_start], (void *) &addr) != 1) {
			snprintf(ip2clue_error, sizeof(ip2clue_error),
				"malformed address [%s]",
				s->fields[f->ip_start]);
			return -1;
		}
		for (i = 0; i < 4; i++)
			cell6->ip_start[i] = ntohl(addr.s6_addr32[i]);

		if (inet_pton(AF_INET6, s->fields[f->ip_end], (void *) &addr) != 1) {
			snprintf(ip2clue_error, sizeof(ip2clue_error),
				"malformed address [%s]",
				s->fields[f->ip_end]);
			return -1;
		}
		for (i = 0; i < 4; i++)
			cell6->ip_end[i] = ntohl(addr.s6_addr32[i]);

		if (f->country_short > 0)
			snprintf(cell6->country_short, sizeof(cell6->country_short),
				"%s",
				s->fields[f->country_short]);
		else
			strcpy(cell6->country_short, "ZZ");
	} else {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"address is nor ipv4 neither ipv6 [%u]", db->v4_or_v6);
		return -1;
	}

	return 0;
}

/*
 * Parse a text file
 */
int ip2clue_parse_text(struct ip2clue_db *db)
{
	int err;
	FILE *f;
	char line[1024], *r;
	struct ip2clue_split s;
	unsigned long line_no, final_lines;
	struct ip2clue_fields fields;
	unsigned int pos;

	err = ip2clue_set_fields(&fields, db->format);
	if (err != 0)
		return -1;

	db->v4_or_v6 = fields.v4_or_v6;

	/* First, count the number of lines to know how many memory to alloc */
	db->no_of_cells = ip2clue_file_lines(db->file);
	if (db->no_of_cells == -1)
		return -1;

	/* Second, alloc memory for whole data */
	if (db->v4_or_v6 == IP2CLUE_TYPE_V4)
		db->mem = db->no_of_cells * sizeof(struct ip2clue_cell_v4);
	else if (db->v4_or_v6 == IP2CLUE_TYPE_V6)
		db->mem = db->no_of_cells * sizeof(struct ip2clue_cell_v6);
	else
		return -1;
	db->cells = malloc(db->mem);
	if (db->cells == NULL) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"cannot alloc %llu bytes",
			db->mem);
		return -1;
	}

	f = fopen(db->file, "r");
	if (f == NULL) {
		snprintf(ip2clue_error, sizeof(ip2clue_error),
			"cannot open [%s] (%s)",
			db->file, strerror(errno));
		goto out_free_db_cells;
	}

	/* Fourth, do the parsing */
	line_no = 0;
	final_lines = 0;
	while (1) {
		line_no++;

		r = fgets(line, sizeof(line) - 1, f);
		if (r == NULL)
			break;

		/* Remove comments */
		if (strchr(fields.comment_chars, line[0]))
			continue;

		/* Remove \n\r */
		pos = strlen(line) - 1;
		while ((pos > 0) && ((line[pos] == '\n') || (line[pos] == '\r')))
			line[pos--] = '\0';

		err = ip2clue_split(&s, line, ",; ");
		if (err < 0)
			goto out_parse_error;

		if (err != fields.total) {
			snprintf(ip2clue_error, sizeof(ip2clue_error),
				"error splitting file [%s], format [%s]"
				", line %lu [%s]:"
				" too less fields; found %d needed %d",
				db->file, ip2clue_format(db->format),
				line_no, line, err, fields.total);
			goto out_parse_error;
		}

		if (final_lines == db->no_of_cells) {
			snprintf(ip2clue_error, sizeof(ip2clue_error),
				"seems that the file changed behind our back");
			goto out_parse_error;
		}

		db->current = final_lines;

		if (ip2clue_add_cell(db, &s, &fields) != 0)
			goto out_parse_error;

		final_lines++;
	}
	fclose(f);

	db->no_of_cells = final_lines;

	return 0;

	out_parse_error:
	fclose(f);

	out_free_db_cells:
	free(db->cells);
	db->cells = NULL;

	return -1;
}
