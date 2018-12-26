HOW-TO
======
1. Format your USB disk as FAT32.
2. Copy all files in `usb_root` to root directory of your USB disk.
3. (Optional) Copy covers.zip with game covers to /mod/share/db/ of your USB disk (for auto cover image use).
4. Copy PSX cue/bin pairs into games/{number}/GameData folder, add a png file with size 226x226 with the same filename as .cue file which is used as cover image.
5. (Optional) You can put a Game.ini in GameData folder to set game info manually:

        [Game]
        Discs=SLUS-00594,SLUS-00776
        Title=Metal Gear Solid
        Publisher=Konami
        Players=1
        Year=1998

   Otherwise game info is automatically fetched from embedded PSX game info database.
6. Connect USB disk to your PlayStation Classic and boot the machine, have fun!

CREDITS
=======
* [madmonkey](https://github.com/madmonkey1907) - USB disk support (lolhack)
* [BleemSync](https://github.com/pathartl/BleemSync) - boot scripts, and idea to sync database on booting PSC
* [sqlite](https://www.sqlite.org) - sqlite3 database support
* [inih](https://github.com/benhoyt/inih) - ini file parsing
* [miniz](https://github.com/richgel999/miniz) - zip file support
