// Command gosmb2-link-spike is the Go counterpart to spikes/01_libsmb2.
//
// It answers the same load-bearing question against the REAL target server,
// but through go-smb2 (a pure-Go userspace SMB2/3 client): can we
//
//  1. enumerate a directory and read per-entry inode + link count;
//  2. stat a file and read the same; and
//  3. CREATE a hard link and observe the source's link count go 1 -> 2 with a
//     shared inode.
//
// Vanilla go-smb2 exposes none of this, so this spike builds against a patched
// fork (see ./go-smb2 and README.md) that adds Share.Link and surfaces
// FileStat.NumberOfLinks / FileStat.IndexNumber. Both come from protocol data
// go-smb2 already decodes internally; the patch just exposes it.
//
// Usage:
//
//	SMB2_PASSWORD=secret go run . \
//	    -host HOST[:445] -share SHARE [-path SUBDIR] -user USER [-domain DOM] \
//	    <source-file> <new-link-name>
package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"path"
	"time"

	"github.com/hirochachacha/go-smb2"
)

func fail(format string, args ...any) {
	fmt.Fprintf(os.Stderr, format+"\n", args...)
	os.Exit(1)
}

func typeStr(fi os.FileInfo) string {
	if fi.IsDir() {
		return "DIR "
	}
	return "FILE"
}

// statFields returns (ino, nlink) for a path, via the patched FileStat.
func statFields(fs *smb2.Share, p string) (ino int64, nlink uint32, size int64, err error) {
	fi, err := fs.Stat(p)
	if err != nil {
		return 0, 0, 0, err
	}
	st, ok := fi.Sys().(*smb2.FileStat)
	if !ok {
		return 0, 0, fi.Size(), fmt.Errorf("Sys() was not *smb2.FileStat")
	}
	return st.IndexNumber, st.NumberOfLinks, st.EndOfFile, nil
}

func statPrint(fs *smb2.Share, p, label string) (ino int64, nlink uint32, err error) {
	ino, nlink, size, err := statFields(fs, p)
	if err != nil {
		fmt.Fprintf(os.Stderr, "  stat(%s) failed: %v\n", p, err)
		return 0, 0, err
	}
	fmt.Printf("  %-8s %s\n", label, p)
	fmt.Printf("           ino=%d  nlink=%d  size=%d\n", ino, nlink, size)
	return ino, nlink, nil
}

func main() {
	host := flag.String("host", "", "server host, optionally host:port (default port 445)")
	share := flag.String("share", "", "share name, e.g. media")
	sub := flag.String("path", "", "subdirectory within the share (optional)")
	user := flag.String("user", "", "username")
	domain := flag.String("domain", "", "domain/workgroup (optional)")
	flag.Parse()

	if *host == "" || *share == "" || flag.NArg() != 2 {
		fmt.Fprintf(os.Stderr,
			"Usage: %s -host HOST[:445] -share SHARE [-path SUBDIR] -user USER "+
				"[-domain DOM] <source-file> <new-link-name>\n"+
				"Password is read from the SMB2_PASSWORD environment variable.\n",
			os.Args[0])
		os.Exit(2)
	}
	source, linkName := flag.Arg(0), flag.Arg(1)

	// Split host/port; keep the bare host for the UNC tree path.
	hostOnly := *host
	addr := *host
	if h, _, err := net.SplitHostPort(*host); err == nil {
		hostOnly = h
	} else {
		addr = net.JoinHostPort(*host, "445")
	}

	conn, err := net.DialTimeout("tcp", addr, 10*time.Second)
	if err != nil {
		fail("tcp dial %s: %v", addr, err)
	}
	defer conn.Close()

	d := &smb2.Dialer{
		Initiator: &smb2.NTLMInitiator{
			User:     *user,
			Password: os.Getenv("SMB2_PASSWORD"),
			Domain:   *domain,
		},
	}
	s, err := d.Dial(conn)
	if err != nil {
		fail("smb dial/auth: %v", err)
	}
	defer s.Logoff()

	fs, err := s.Mount(`\\` + hostOnly + `\` + *share)
	if err != nil {
		fail("mount \\\\%s\\%s: %v", hostOnly, *share, err)
	}
	defer fs.Umount()

	fmt.Printf("Connected to \\\\%s\\%s  (subdir='%s')\n\n", hostOnly, *share, *sub)

	// ---- 1. Enumerate the directory. ReadDir alone does NOT carry ino/nlink
	// (go-smb2 uses FileDirectoryInformation), so we Stat each entry to fill
	// those columns -- one extra round trip per file. ----
	listDir := *sub
	if listDir == "" {
		listDir = "."
	}
	fmt.Printf("=== Directory listing: '%s'  (ino/nlink via per-entry Stat) ===\n", *sub)
	fis, err := fs.ReadDir(listDir)
	if err != nil {
		fail("readdir('%s'): %v", listDir, err)
	}
	fmt.Printf("  %-4s  %20s  %6s  %s\n", "type", "ino", "nlink", "name")
	for _, fi := range fis {
		var ino int64
		var nlink uint32
		if !fi.IsDir() {
			ino, nlink, _, _ = statFields(fs, path.Join(*sub, fi.Name()))
		}
		fmt.Printf("  %-4s  %20d  %6d  %s\n", typeStr(fi), ino, nlink, fi.Name())
	}
	fmt.Println()

	srcPath := path.Join(*sub, source)
	linkPath := path.Join(*sub, linkName)

	// ---- 2. Baseline stat of the source file ----
	fmt.Println("=== Before link ===")
	if _, _, err := statPrint(fs, srcPath, "source"); err != nil {
		os.Exit(1)
	}
	fmt.Println()

	// ---- 3. Create the hard link and re-check ----
	fmt.Printf("=== Creating hard link: '%s' -> '%s' ===\n", linkName, source)
	if err := fs.Link(srcPath, linkPath); err != nil {
		fail("fs.Link failed: %v", err)
	}
	fmt.Println("fs.Link returned success.")
	fmt.Println()

	fmt.Println("=== After link ===")
	srcIno, srcNlink, err := statPrint(fs, srcPath, "source")
	if err != nil {
		os.Exit(1)
	}
	linkIno, linkNlink, err := statPrint(fs, linkPath, "link")
	if err != nil {
		os.Exit(1)
	}
	fmt.Println()

	// ---- Verdict ----
	fmt.Println("=== Verdict ===")
	fmt.Printf("  source nlink: (>=2 expected) now %d\n", srcNlink)
	eq := "DIFFERENT"
	if srcIno == linkIno {
		eq = "EQUAL"
	}
	fmt.Printf("  inode:        source=%d  link=%d  (%s)\n", srcIno, linkIno, eq)

	rc := 0
	if linkNlink >= 2 && srcNlink >= 2 && srcIno != 0 && srcIno == linkIno {
		fmt.Println("\n  PASS: a real hard link was created and the server reports the shared")
		fmt.Println("  inode and incremented link count this app depends on.")
	} else {
		fmt.Println("\n  INCONCLUSIVE: the link call succeeded, but the server did not report")
		fmt.Println("  the shared inode / incremented link count. See spike 01 README's")
		fmt.Println("  'Server capability' notes -- this is a server property, not a client one.")
		rc = 3
	}

	// ---- Cleanup ----
	if os.Getenv("KEEP_LINK") == "" {
		if err := fs.Remove(linkPath); err != nil {
			fmt.Fprintf(os.Stderr, "\nwarning: cleanup remove('%s') failed: %v\n", linkPath, err)
		} else {
			fmt.Printf("\nCleaned up: removed '%s'.\n", linkName)
		}
	}

	os.Exit(rc)
}
