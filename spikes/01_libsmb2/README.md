# SMB2 hard-link spike (libsmb2)

Goal: settle the riskiest unknown in the whole project before choosing a
language or GUI toolkit — **can we read inode/link-count and _create a hard
link_ over SMB2, against the actual target server?**

`smb2_link_spike.c` connects to a share, lists a directory (printing `ino` and
`nlink` per entry), stats a source file, creates a hard link to it, and checks
that the source's link count went `1 -> 2` with a **shared inode**.

## Finding up front

libsmb2 already exposes everything the app's *file list* needs:

- `struct smb2_stat_64` carries `smb2_ino` (inode) and `smb2_nlink` (link count).
- `smb2_readdir()` returns both per directory entry, and `smb2_stat()` per file.

What it does **not** ship is hard-link *creation*: there is `smb2_rename`,
`smb2_unlink`, `smb2_readlink` — but no `smb2_link`. However, on the SMB2 wire a
hard link is a `SET_INFO` with `FILE_LINK_INFORMATION` (class `0x0B`), and per
MS-FSCC that Type-2 structure is **byte-for-byte identical** to
`FILE_RENAME_INFORMATION` (class `0x0A`), which libsmb2 already encodes and sends
for `smb2_rename`. So adding `smb2_link` is a tiny, low-risk patch: reuse the
rename encoder and flip one info-class byte.

## Patch libsmb2 (4 small edits)

Clone the library you'll link against and apply these. They mirror the existing
`rename` paths exactly; only the info-class differs.

```
git clone https://github.com/sahlberg/libsmb2
cd libsmb2
```

### 1. `lib/smb2-cmd-set-info.c` — let the encoder accept the link class

In `smb2_encode_set_info_request()`, inside `switch (req->file_info_class)`, add
a fallthrough label above the rename case (the body already produces the exact
bytes a link needs):

```diff
+                case SMB2_FILE_LINK_INFORMATION:   /* identical Type-2 layout to rename */
                 case SMB2_FILE_RENAME_INFORMATION:
                         rni = req->input_data;
```

### 2. `lib/libsmb2.c` — add `smb2_link_async`

Paste this right after `smb2_rename_async`. It is a verbatim copy of that
function with the function name changed and **one line** different
(`file_info_class`). It reuses the file-static `rename_cb_*` callbacks,
`free_rename_data`, and `compound_file_id`, so it must live in this file.

```c
int
smb2_link_async(struct smb2_context *smb2, const char *oldpath,
                const char *newpath, smb2_command_cb cb, void *cb_data)
{
        struct rename_cb_data *rename_data;
        struct smb2_create_request cr_req;
        struct smb2_set_info_request si_req;
        struct smb2_close_request cl_req;
        struct smb2_pdu *pdu, *next_pdu;
        struct smb2_file_rename_info rn_info _U_; /* same wire layout as link info */
        uint8_t *ptr;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        rename_data = calloc(1, sizeof(struct rename_cb_data));
        if (rename_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate rename_data");
                return -ENOMEM;
        }

        rename_data->cb = cb;
        rename_data->cb_data = cb_data;
        rename_data->newpath = (uint8_t *)strdup(newpath);
        if (rename_data->newpath == NULL) {
                free_rename_data(rename_data);
                smb2_set_error(smb2, "Failed to allocate rename_data->newpath");
                return -ENOMEM;
        }
        for (ptr = rename_data->newpath; *ptr; ptr++) {
                if (*ptr == '/') {
                        *ptr = '\\';
                }
        }

        /* CREATE command: open the EXISTING (primary) file. Note: like rename
         * this requests SMB2_DELETE on the source; hard-linking does not
         * strictly require it, so you may drop SMB2_DELETE here if your share
         * withholds delete rights on the primary. */
        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.desired_access = SMB2_GENERIC_READ | SMB2_FILE_READ_ATTRIBUTES | SMB2_DELETE;
        cr_req.file_attributes = 0;
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE | SMB2_FILE_SHARE_DELETE;
        cr_req.create_disposition = SMB2_FILE_OPEN;
        cr_req.create_options = 0;
        cr_req.name = oldpath;

        pdu = smb2_cmd_create_async(smb2, &cr_req, rename_cb_1, rename_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                free_rename_data(rename_data);
                return -EINVAL;
        }

        /* SET INFO command: FILE_LINK_INFORMATION instead of RENAME. */
        rn_info.replace_if_exist = 0;
        rn_info.file_name = rename_data->newpath;

        memset(&si_req, 0, sizeof(struct smb2_set_info_request));
        si_req.info_type = SMB2_0_INFO_FILE;
        si_req.file_info_class = SMB2_FILE_LINK_INFORMATION;   /* <-- the only real change */
        si_req.additional_information = 0;
        memcpy(si_req.file_id, compound_file_id, SMB2_FD_SIZE);
        si_req.input_data = &rn_info;

        next_pdu = smb2_cmd_set_info_async(smb2, &si_req,
                                           rename_cb_2, rename_data);
        if (next_pdu == NULL) {
                smb2_set_error(smb2, "Failed to create set command. %s",
                               smb2_get_error(smb2));
                free_rename_data(rename_data);
                smb2_free_pdu(smb2, pdu);
                return -EINVAL;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        /* CLOSE command */
        memset(&cl_req, 0, sizeof(struct smb2_close_request));
        cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(cl_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_close_async(smb2, &cl_req, rename_cb_3, rename_data);
        if (next_pdu == NULL) {
                rename_data->cb(smb2, -ENOMEM, NULL, rename_data->cb_data);
                free_rename_data(rename_data);
                smb2_free_pdu(smb2, pdu);
                return -EINVAL;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}
```

### 3. `lib/sync.c` — add the blocking `smb2_link` wrapper

Copy the existing `smb2_rename` sync wrapper in this file, rename it to
`smb2_link`, and have it call `smb2_link_async` instead of `smb2_rename_async`
(same `sync_cb_data` / `wait_for_reply` / status-callback plumbing the other
wrappers use). It ends up as:

```c
int
smb2_link(struct smb2_context *smb2, const char *oldpath, const char *newpath)
{
        struct sync_cb_data cb_data;

        memset(&cb_data, 0, sizeof(cb_data));

        if (smb2_link_async(smb2, oldpath, newpath,
                            sync_generic_status_cb, &cb_data) != 0) {
                smb2_set_error(smb2, "smb2_link_async failed");
                return -EIO;
        }
        if (wait_for_reply(smb2, &cb_data) < 0) {
                return -EIO;
        }
        return cb_data.status;
}
```

(If your libsmb2 version names the shared callback differently, use whatever the
neighbouring `smb2_rename` wrapper uses — just swap `_rename_` for `_link_`.)

### 4. `include/smb2/libsmb2.h` — declare the two functions

Next to the `smb2_rename` / `smb2_rename_async` declarations:

```c
int smb2_link_async(struct smb2_context *smb2, const char *oldpath,
                    const char *newpath, smb2_command_cb cb, void *cb_data);
int smb2_link(struct smb2_context *smb2, const char *oldpath,
              const char *newpath);
```

### 5. (MSVC / Windows DLL only) `lib/libsmb2.syms` — export the new symbols

When building the DLL with MSVC, libsmb2 generates its export `.def` from
`lib/libsmb2.syms`; a function not listed there compiles into the DLL but is not
exported (you'll get `LNK2019: unresolved external symbol smb2_link`). Add both
names next to `smb2_rename`:

```
 smb2_truncate_async
+smb2_link
+smb2_link_async
 smb2_rename
 smb2_rename_async
```

The `.def` is regenerated at *configure* time, and editing `.syms` does **not**
auto-trigger a reconfigure — force one before rebuilding (see Build below).

### Pre-existing libsmb2 bug: `examples/CMakeLists.txt` breaks MSVC

Not related to our feature, but you'll hit it: the last line of
`examples/CMakeLists.txt` unconditionally applies GCC-only flags
(`-Werror` and `-D_U_=__attribute__((unused))`), which makes `cl.exe` abort with
`D8021: invalid numeric argument '/Werror'`. Guard it for MSVC, mirroring what
`lib/CMakeLists.txt` already does:

```cmake
if(NOT MSVC)
  add_definitions(-Werror "-D_U_=__attribute__((unused))")
else()
  add_definitions("-D_U_=")
endif()
```

## Build

libsmb2 uses plain NTLMSSP by default, so **no Kerberos/krb5 is required** for a
workgroup share. Drop `smb2_link_spike.c` into the library's `examples/` folder
and register it, then build the library + examples together.

Add to `examples/CMakeLists.txt`:

```cmake
add_executable(smb2_link_spike smb2_link_spike.c)
target_link_libraries(smb2_link_spike smb2)
```

### Linux

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_EXAMPLES=ON
cmake --build build -j
# -> build/examples/smb2_link_spike
```

### Windows (MSVC)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DENABLE_EXAMPLES=ON
# If you edited libsmb2.syms after a previous configure, force a reconfigure so
# the export .def regenerates (editing .syms alone does not trigger it):
cmake -S . -B build
cmake --build build --config Release --target smb2_link_spike
# -> build\examples\Release\smb2_link_spike.exe

# The DLL builds to build\lib\Release, not next to the exe. Copy it alongside
# (or add build\lib\Release to PATH) before running:
Copy-Item build\lib\Release\smb2.dll build\examples\Release\
```

## Run

Create a throwaway file (e.g. `original.bin`) in a test subdir on the share
first, then:

```sh
# Linux
export SMB2_PASSWORD='secret'
./build/examples/smb2_link_spike smb://dan@192.168.1.10/media/dedup-test original.bin link.bin
```

```powershell
# Windows
$env:SMB2_PASSWORD = 'secret'
.\build\examples\Release\smb2_link_spike.exe smb://dan@192.168.1.10/media/dedup-test original.bin link.bin
```

Domain/workgroup: use `smb://WORKGROUP;dan@host/share/dir` if needed. Set
`KEEP_LINK=1` to leave the created link in place instead of cleaning it up.

Expected on success — the source's `nlink` climbs and the inodes match:

```text
=== Verdict ===
  source nlink: 1 -> 2
  inode:        source=1234567890  link=1234567890  (EQUAL)

  PASS: a real hard link was created and the server reports the shared
  inode and incremented link count this app depends on.
```

## Interpreting the result — this is a Go/No-Go gate

- **PASS (rc 0):** SMB + hard links + inode/link-count all work in C against your
  server. libsmb2 is viable; the language decision can proceed on GUI/velocity
  grounds. This same result is what the parallel `go-smb2` spike must reproduce.
- **`smb2_link` fails:** the server rejected `FILE_LINK_INFORMATION` (permissions,
  or the SMB server doesn't support server-side hard links). Re-test with an
  account that has delete/write on the source; if it still fails, hard-linking
  over raw SMB is out and you fall back to the **OS-mount** approach (map the
  share, use native `link()` / `CreateHardLinkW`), which also reopens Rust+egui.
- **INCONCLUSIVE (rc 3):** the link was created but `ino`/`nlink` came back
  zero/unchanged — see below.

## Server capability — the real risk, not the library

This spike is as much a probe of **your server** as of libsmb2. The app's UI
(inode column, hard-link-count column, matching duplicates by inode) only works
if the SMB server actually reports these fields:

- **Windows Server / NTFS shares:** report `FileInternalInformation` IndexNumber
  and `FileStandardInformation` NumberOfLinks accurately. Expect PASS.
- **Samba:** depends on config. Inode numbers may not be stable/unique and
  `NumberOfLinks` can read `1` even for hard-linked files depending on the VFS
  modules and `unix extensions` setting. **Test against your real box.**
- **Consumer NAS SMB stacks:** some zero or fake these fields.

If your server zeroes `ino`/`nlink`, no amount of client work fixes it — that
pushes the whole design toward mounting the share and using the OS filesystem
layer (where the CIFS client synthesizes inode numbers) instead of a userspace
SMB client, and correspondingly reopens the language/GUI decision.

## Next

Run the equivalent `go-smb2` spike and compare. If both PASS on your server, the
choice comes down to GUI/velocity (the earlier discussion); if only one PASSes,
that decides it.
