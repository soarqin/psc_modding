HOW-TO
======
1. Format your USB disk as FAT32.
2. Copy all files in `usb_root` to root directory of your USB disk.
3. (Optional) Copy [covers.zip](https://drive.google.com/open?id=1OWaQJp6KdcwAiiu9GG8Fxt2f3aEcFUqA) with game covers to /mod/share/db/ of your USB disk (for auto cover image use).
4. Copy PSX bin/img/iso files into games folder, multi-disc games will be recognized automatically, but you can create subfolders in games folder, each subfolder contains a set of discs of a single game (or split multi-disc games into different subfolders to load them as different games)
5. Connect USB disk to your PlayStation Classic and boot the machine, have fun!

CREDITS
=======
* [madmonkey](https://github.com/madmonkey1907) - USB disk support (lolhack)
* [BleemSync](https://github.com/pathartl/BleemSync) - boot scripts, and idea to sync database on booting PSC
* [sqlite](https://www.sqlite.org) - sqlite3 database support
* [inih](https://github.com/benhoyt/inih) - ini file parsing
* [miniz](https://github.com/richgel999/miniz) - zip file support
