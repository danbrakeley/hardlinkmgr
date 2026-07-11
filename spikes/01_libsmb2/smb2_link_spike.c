/*
 * smb2_link_spike.c - Hard Link Manager viability spike
 *
 * Answers the one load-bearing question for this project: over SMB2/3, against
 * the REAL target server, can we
 *
 *   1. enumerate a directory and read per-entry inode (smb2_ino) + link count
 *      (smb2_nlink) - the two columns the app's file list needs;
 *   2. stat a single file and read the same; and
 *   3. CREATE a hard link (SMB2 FILE_LINK_INFORMATION) and then observe the
 *      source file's link count go 1 -> 2 with a SHARED inode number.
 *
 * If all three pass against your server, the SMB half of the app is viable in
 * C. If (3) fails, or the server reports bogus ino/nlink, that changes the
 * language/architecture decision - see spike/README.md.
 *
 * Requires libsmb2 patched with smb2_link()/smb2_link_async() - see README.md.
 *
 * Usage:
 *   SMB2_PASSWORD=secret ./smb2_link_spike \
 *       smb://[DOMAIN;]user@server/share/subdir  <source-file>  <new-link-name>
 *
 * <source-file> and <new-link-name> are plain names inside the URL's subdir.
 * Put a throwaway file in that subdir first. The spike creates the link,
 * verifies, then removes the link it created (set KEEP_LINK=1 to keep it).
 */

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

static const char *type_str(uint32_t t)
{
        switch (t) {
        case SMB2_TYPE_DIRECTORY: return "DIR ";
        case SMB2_TYPE_LINK:      return "LINK";
        default:                  return "FILE";
        }
}

static void join(char *out, size_t n, const char *base, const char *name)
{
        if (base && base[0]) {
                snprintf(out, n, "%s/%s", base, name);
        } else {
                snprintf(out, n, "%s", name);
        }
}

static int stat_print(struct smb2_context *smb2, const char *path,
                      struct smb2_stat_64 *st, const char *label)
{
        if (smb2_stat(smb2, path, st) < 0) {
                fprintf(stderr, "  stat(%s) failed: %s\n", path,
                        smb2_get_error(smb2));
                return -1;
        }
        printf("  %-8s %s\n", label, path);
        printf("           ino=%" PRIu64 "  nlink=%" PRIu32 "  size=%" PRIu64 "\n",
               st->smb2_ino, st->smb2_nlink, st->smb2_size);
        return 0;
}

int main(int argc, char *argv[])
{
        struct smb2_context *smb2 = NULL;
        struct smb2_url *url = NULL;
        struct smb2dir *dir = NULL;
        struct smb2dirent *ent;
        struct smb2_stat_64 st_src0, st_src1, st_link;
        char src_path[1024], link_path[1024];
        const char *password;
        int connected = 0;
        int rc = 1;

#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

        if (argc != 4) {
                fprintf(stderr,
                    "Usage: %s smb://[DOMAIN;]user@server/share/subdir "
                    "<source-file> <new-link-name>\n"
                    "Password is read from the SMB2_PASSWORD environment "
                    "variable.\n", argv[0]);
                return 2;
        }

        smb2 = smb2_init_context();
        if (smb2 == NULL) {
                fprintf(stderr, "smb2_init_context failed\n");
                return 1;
        }

        url = smb2_parse_url(smb2, argv[1]);
        if (url == NULL) {
                fprintf(stderr, "parse url failed: %s\n", smb2_get_error(smb2));
                goto out;
        }

        password = getenv("SMB2_PASSWORD");
        if (password) {
                smb2_set_password(smb2, password);
        }
        smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

        if (smb2_connect_share(smb2, url->server, url->share, url->user) < 0) {
                fprintf(stderr, "connect_share failed: %s\n",
                        smb2_get_error(smb2));
                goto out;
        }
        connected = 1;
        printf("Connected to //%s/%s  (subdir='%s')\n\n",
               url->server, url->share, url->path);

        /* ---- 1. Enumerate the directory, showing ino + nlink per entry ---- */
        printf("=== Directory listing: '%s' ===\n", url->path);
        dir = smb2_opendir(smb2, url->path);
        if (dir == NULL) {
                fprintf(stderr, "opendir('%s') failed: %s\n", url->path,
                        smb2_get_error(smb2));
                goto out;
        }
        printf("  %-4s  %20s  %6s  %s\n", "type", "ino", "nlink", "name");
        while ((ent = smb2_readdir(smb2, dir)) != NULL) {
                printf("  %-4s  %20" PRIu64 "  %6" PRIu32 "  %s\n",
                       type_str(ent->st.smb2_type),
                       ent->st.smb2_ino, ent->st.smb2_nlink, ent->name);
        }
        smb2_closedir(smb2, dir);
        dir = NULL;
        printf("\n");

        join(src_path,  sizeof(src_path),  url->path, argv[2]);
        join(link_path, sizeof(link_path), url->path, argv[3]);

        /* ---- 2. Baseline stat of the source file ---- */
        printf("=== Before link ===\n");
        if (stat_print(smb2, src_path, &st_src0, "source") < 0) {
                goto out;
        }
        printf("\n");

        /* ---- 3. Create the hard link, then re-check both names ---- */
        printf("=== Creating hard link: '%s' -> '%s' ===\n", argv[3], argv[2]);
        if (smb2_link(smb2, src_path, link_path) != 0) {
                fprintf(stderr, "smb2_link failed: %s\n", smb2_get_error(smb2));
                goto out;
        }
        printf("smb2_link returned success.\n\n");

        printf("=== After link ===\n");
        if (stat_print(smb2, src_path,  &st_src1, "source") < 0) {
                goto out;
        }
        if (stat_print(smb2, link_path, &st_link, "link") < 0) {
                goto out;
        }
        printf("\n");

        /* ---- Verdict ---- */
        printf("=== Verdict ===\n");
        printf("  source nlink: %" PRIu32 " -> %" PRIu32 "\n",
               st_src0.smb2_nlink, st_src1.smb2_nlink);
        printf("  inode:        source=%" PRIu64 "  link=%" PRIu64 "  (%s)\n",
               st_src1.smb2_ino, st_link.smb2_ino,
               (st_src1.smb2_ino == st_link.smb2_ino) ? "EQUAL" : "DIFFERENT");

        if (st_link.smb2_nlink >= 2 && st_src1.smb2_nlink >= 2 &&
            st_src1.smb2_ino != 0 && st_src1.smb2_ino == st_link.smb2_ino) {
                printf("\n  PASS: a real hard link was created and the server "
                       "reports the shared\n  inode and incremented link count "
                       "this app depends on.\n");
                rc = 0;
        } else {
                printf("\n  INCONCLUSIVE: the link call succeeded, but the "
                       "server did not report\n  the shared inode / incremented "
                       "link count. The protocol works; your\n  server's SMB2 "
                       "metadata does not. See README 'Server capability'.\n");
                rc = 3;
        }

        /* ---- Cleanup: remove the link we created (unless told to keep it) ---- */
        if (getenv("KEEP_LINK") == NULL) {
                if (smb2_unlink(smb2, link_path) < 0) {
                        fprintf(stderr, "\nwarning: cleanup unlink('%s') failed: "
                                "%s\n", link_path, smb2_get_error(smb2));
                } else {
                        printf("\nCleaned up: removed '%s'.\n", argv[3]);
                }
        }

out:
        if (dir) {
                smb2_closedir(smb2, dir);
        }
        if (connected) {
                smb2_disconnect_share(smb2);
        }
        if (url) {
                smb2_destroy_url(url);
        }
        if (smb2) {
                smb2_destroy_context(smb2);
        }
#ifdef _WIN32
        WSACleanup();
#endif
        return rc;
}
