HOW-TO
======
1. Format your USB disk as FAT32
2. Copy all files in `usb_root` to root directory of your USB disk
3. (Optional) Copy covers.zip with game covers to /mod/share/db/ of your USB disk (for auto cover image use)
4. Copy PSX cue/bin pairs into games/{number}/GameData folder
5. Add a Game.ini to fit the game info. Or just remove it, game info will be fetched automatically on boot
6. Connect USB disk to your PlayStation Classic and boot the machine, have fun!

CREDITS
=======
* [madmonkey](https://github.com/madmonkey1907) - created lolhack
* [BleemSync](https://github.com/pathartl/BleemSync) - modification to boot scripts, idea to sync database on booting PSC
* [sqlite](https://www.sqlite.org)
* [inih](https://github.com/benhoyt/inih)
* [miniz](https://github.com/richgel999/miniz)
