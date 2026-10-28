These gptk files are gamepad-to-keyboard mappings for use with
gptokeyb which is often bundled with PortMaster.

If planning to use gptokeyb with a controller for games that
don't have native joystick support, place this directory
alongside the linapple executable and launch with --chooser.
LinApple launched with --chooser will then automatically
detect if gptokeyb is running and/or if PortMaster is present
and will take advantage of preset gptk files with a filename
matching that of the loaded disk image.

e.g.

bin/
+-- linapple
+-- controls/
¦   +-- chooser.gptk
¦   +-- linapple.gptk
¦   +-- pitfall2.gptk
+-- disks/
    +-- pitfall2.dsk

$ cd bin
$ ./linapple --chooser bin/disks

*Select pitfall2.dsk for the first floppy drive, hit 'START' and
 LinApple will then launch gptokeyb using controls/pitfall2.gptk

NOTE: PortMaster and it's directory structure must be present
      and loaded with an accommodating shell script in order for
      the above to actually work.

If a PortMaster environment is not present or set up for use,
LinApple will run normally and the 'controls' folder will not
be used.  The 'chooser' menu is required to take advantage of
PortMaster's functionality, but PortMaster is not required in
order to use the 'chooser' menu and can be used anytime and if
one does not plan or require to use a controller with gptokeyb.
