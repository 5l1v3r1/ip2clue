/*
 * Author: Catalin(ux) M. BOIE
 * Description: parser for IP files
 */

#include <i_config.h>

/*
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
*/

#include <i_util.h>
#include <parser_core.h>


/*
 * Returns the format based on code
 */
char *ip2clue_format(enum ip2clue_format f)
{
	switch (f) {
	case IP2CLUE_FORMAT_WEBHOSTING: return "webhosting";
	case IP2CLUE_FORMAT_MAXMIND: return "maxmind";
	case IP2CLUE_FORMAT_MAXMIND_V6: return "maxmind-v6";
	case IP2CLUE_FORMAT_SOFTWARE77: return "software77";
	case IP2CLUE_FORMAT_IP2LOCATION: return "ip2location";
	default: return "unknown";
	}
}

