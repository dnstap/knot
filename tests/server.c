/*  Copyright (C) 2011 CZ.NIC, z.s.p.o. <knot-dns@labs.nic.cz>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <tap/basic.h>
#include "knot/server/server.h"

// Signal handler
static void interrupt_handle(int s)
{
}

/*! API: run tests. */
int main(int argc, char *argv[])
{
	plan(2);

	server_t server;
	int ret = 0;

	/* Register service and signal handler */
	struct sigaction sa;
	sa.sa_handler = interrupt_handle;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, NULL); // Interrupt

	/* Test server for correct initialization */
	ret = server_init(&server);
	ok(ret == KNOT_EOK, "server: initialized");
	if (ret != KNOT_EOK) {
		return 1;
	}

	/* Test server startup */
	ret = server_start(&server);
	ok(ret == KNOT_EOK, "server: started ok");
	if (ret != KNOT_EOK) {
	        return 1;
	}

	server_stop(&server);

	/* Wait for server to finish. */
	server_wait(&server);

	/* Wait for server to finish. */
	server_deinit(&server);

	return 0;
}
