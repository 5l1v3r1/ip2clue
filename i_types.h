/*
 * Author: Catalin(ux) M. BOIE
 * Description: types definition
 */

#ifndef IP2CLUE_I_TYPES_H
#define IP2CLUE_I_TYPES_H 1

#include <i_config.h>

#include <time.h>

enum ip2clue_type
{
	IP2CLUE_TYPE_V4 = 4,
	IP2CLUE_TYPE_V6 = 6
};

enum ip2clue_format
{
	IP2CLUE_FORMAT_WEBHOSTING = 1,
	IP2CLUE_FORMAT_MAXMIND,
	IP2CLUE_FORMAT_MAXMIND_V6,
	IP2CLUE_FORMAT_SOFTWARE77,
	IP2CLUE_FORMAT_IP2LOCATION
};

struct ip2clue_extra
{
	char		country_long[32];
	char		region[32];
	char		city[32];
	char		isp[64];
	float		latitude;
	float		longitude;
	char		zip[16];
	char		domain[64];
	char		timezone[16];
	char		netspeed[16];
	char		idd[16];
	char		areacode[32];
	char		ws_code[16];
	char		ws_name[32];
};

struct ip2clue_fields
{
	unsigned char		ip_start, ip_end;
	unsigned char		ip_bin_start, ip_bin_end;
	unsigned char		country_short, country_long;
	unsigned char		total;				/* Total number of expected fields */
	char			comment_chars[8];		/* Put here the chars to ignore at the beggining of line */
	unsigned char		bin_file;
	enum ip2clue_format	format;
	enum ip2clue_type	v4_or_v6;
};

struct ip2clue_split
{
	unsigned int		count;
	char			fields[32][256];
};

/* common */
struct ip2clue_db
{
	enum ip2clue_format	format;
	enum ip2clue_type	v4_or_v6;
	unsigned long long	no_of_cells, current;
	void			*cells;
	time_t			ts;		/* Db building time */
	time_t			ts_load;	/* Time when the table was loaded. */
	unsigned int		elap_load_ms;	/* How much time was needed for load */
	char			file[128];	/* Input file */
	unsigned long long	mem;		/* How many bytes this table is using */
	unsigned int		usage_count;	/* If 0, we can safely drop it */
	unsigned long long	lookup_ok;
	unsigned long long	lookup_notfound;
	unsigned long long	lookup_malformed;
};

/*
 * This is a chain of ip2clue_db, ordered by preference
 */
struct ip2clue_list
{
	unsigned int		number;
	struct ip2clue_db	**entries;
};

/* v4 */
struct ip2clue_cell_v4
{
	unsigned int		ip_start, ip_end;
	char			country_short[4];
	struct ip2clue_extra	*extra;
};

/* v6 */
struct ip2clue_cell_v6
{
	unsigned int		ip_start[4], ip_end[4];
	char			country_short[4];
	struct ip2clue_extra	*extra;
};

#endif
