/***
  This file is part of systemd.

  Copyright 2011 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "sd-id128.h"

#include "fd-util.h"
#include "hexdecoct.h"
#include "id128-util.h"
#include "io-util.h"
#include "macro.h"
#include "random-util.h"
#include "util.h"

_public_ char *sd_id128_to_string(sd_id128_t id, char s[SD_ID128_STRING_MAX]) {
        unsigned n;

        assert_return(s, NULL);

        for (n = 0; n < 16; n++) {
                s[n*2] = hexchar(id.bytes[n] >> 4);
                s[n*2+1] = hexchar(id.bytes[n] & 0xF);
        }

        s[32] = 0;

        return s;
}

_public_ int sd_id128_from_string(const char s[], sd_id128_t *ret) {
        unsigned n, i;
        sd_id128_t t;
        bool is_guid = false;

        assert_return(s, -EINVAL);

        for (n = 0, i = 0; n < 16;) {
                int a, b;

                if (s[i] == '-') {
                        /* Is this a GUID? Then be nice, and skip over
                         * the dashes */

                        if (i == 8)
                                is_guid = true;
                        else if (i == 13 || i == 18 || i == 23) {
                                if (!is_guid)
                                        return -EINVAL;
                        } else
                                return -EINVAL;

                        i++;
                        continue;
                }

                a = unhexchar(s[i++]);
                if (a < 0)
                        return -EINVAL;

                b = unhexchar(s[i++]);
                if (b < 0)
                        return -EINVAL;

                t.bytes[n++] = (a << 4) | b;
        }

        if (i != (is_guid ? 36 : 32))
                return -EINVAL;

        if (s[i] != 0)
                return -EINVAL;

        if (ret)
                *ret = t;
        return 0;
}

_public_ int sd_id128_get_machine(sd_id128_t *ret) {
        static thread_local sd_id128_t saved_machine_id = {};
        int r;

        assert_return(ret, -EINVAL);

        if (sd_id128_is_null(saved_machine_id)) {
                r = id128_read("/etc/machine-id", ID128_PLAIN, &saved_machine_id);
                if (r < 0)
                        return r;

                if (sd_id128_is_null(saved_machine_id))
                        return -EINVAL;
        }

        *ret = saved_machine_id;
        return 0;
}

_public_ int sd_id128_get_boot(sd_id128_t *ret) {
        static thread_local sd_id128_t saved_boot_id = {};
        int r;

        assert_return(ret, -EINVAL);

        if (sd_id128_is_null(saved_boot_id)) {
                r = id128_read("/proc/sys/kernel/random/boot_id", ID128_UUID, &saved_boot_id);
                if (r < 0)
                        return r;
        }

        *ret = saved_boot_id;
        return 0;
}

_public_ int sd_id128_get_invocation(sd_id128_t *ret) {
        static thread_local sd_id128_t saved_invocation_id = {};
        int r;

        assert_return(ret, -EINVAL);

        if (sd_id128_is_null(saved_invocation_id)) {
                const char *e;

                e = secure_getenv("INVOCATION_ID");
                if (!e)
                        return -ENXIO;

                r = sd_id128_from_string(e, &saved_invocation_id);
                if (r < 0)
                        return r;
        }

        *ret = saved_invocation_id;
        return 0;
}

static sd_id128_t make_v4_uuid(sd_id128_t id) {
        /* Stolen from generate_random_uuid() of drivers/char/random.c
         * in the kernel sources */

        /* Set UUID version to 4 --- truly random generation */
        id.bytes[6] = (id.bytes[6] & 0x0F) | 0x40;

        /* Set the UUID variant to DCE */
        id.bytes[8] = (id.bytes[8] & 0x3F) | 0x80;

        return id;
}

_public_ int sd_id128_randomize(sd_id128_t *ret) {
        sd_id128_t t;
        int r;

        assert_return(ret, -EINVAL);

        r = dev_urandom(&t, sizeof(t));
        if (r < 0)
                return r;

        /* Turn this into a valid v4 UUID, to be nice. Note that we
         * only guarantee this for newly generated UUIDs, not for
         * pre-existing ones. */

        *ret = make_v4_uuid(t);
        return 0;
}
