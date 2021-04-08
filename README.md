# What is EnsoniqFS?

EnsoniqFS is a file system plugin for the file manager Total Commander created
by Christian Ghisler (http://www.ghisler.com).

With EnsoniqFS you can mount the following Ensoniq file systems:
- SCSI & IDE harddisks
- SCSI & IDE removable disks (i. e. ZIP drives)
- SCSI & IDE CDROMs
- floppy disks (direct file access using driver OmniFlop, see below)
- image files ISO, IMG, Mode1CDROM (BIN), GKH, EDE, EDA, EDT, EDV

Once mounted, you can do the following with Total Commander:
- read, write, copy, move, rename, delete files on Ensoniq media
- copy, move files from/to Windows drives
- create, rename, copy, move whole directories

CDROMs and BIN (Mode1 CDROM images)/GKH/EDE/EDA/EDT/EDV files are read only, 
writing is not supported (write support for the image files could be 
implemented in the future). Full write support is available for ISO images.

With the above, you get a complete file manager for Ensoniq disks.

# Where to get it

All the project related stuff is located at SourceForge:

http://sourceforge.net/projects/ensoniqfs/

There you can download EnsoniqFS and related tools. If you're
interested, you can check out the source codes as well.

# Installation

## EnsoniqFS

To use EnsoniqFS, you first need to have Total Commander installed. Download
your copy from http://www.ghisler.com. Installation is straightforward, there
shouldn't be any problems. Be sure to get the latest version (6.5 minimum).

Install EnsoniqFS (which you should have downloaded from SourceForge) by 
opening the ZIP file from within Total Commander. This is also the preferred 
method for updates, since Total Commander handles all the files automatically.

## OmniFlop driver

To be able to access Ensoniq formatted floppy disks you need to have a special
disk driver. If you do not plan using floppies, then you can leave out this
step. The EnsoniqFS plugin will also work without this driver (you're then
limited to the usage of image files and SCSI disks).

First, get the driver from here (be sure you get the latest version, 2.01m 
minimum):
http://www.shlock.co.uk/Utils/OmniFlop/OmniFlop.htm

The driver package comes with detailed installation instructions. Be sure to
read them and follow them closely.

###############################################################################
TODO:
	Licensing of omniflop?
###############################################################################

Now you're just one step far from being able to use the driver. You need a
license to use it with EnsoniqFS. Don't be afraid - the license is free and
it is available via instant online registration. Here's how you get the
license:
	
From the OmniFlop package, start OmniFlop.exe. The "OmniFlop Wizard" opens.
Click "Next", choose "Get a license" and click "Next" again. Then a warning
appears that you normally do not need a license, but trust me - in this case
you do - so click "OK" and from the next dialog choose "TotalCommander 
EnsoniqFS plugin" and click "Finish". The registration dialog will open.
Click "Register on-line" and a browser window will be opened with the 
appropriate values already filled in. Enter your email address and submit
the form. A new license will be emailed to you instantly. Copy the license 
key you got via mail and paste it into the OmniFlop registration window.
Click OK and you are done!

# Using EnsoniqFS

## EnsoniqFS

If you successfully mastered the installation, the plugin is available through
your network neighbourhood inside Total Commander. Hit Alt+F1 (or Alt+F2) or
click on the drive combo box and choose "network neighbourhood". The directory
listing should then contain "Ensoniq filesystems". If not, something is really
wrong (try to re-download and re-install the file).

If you then enter the "Ensoniq filesystems" folder, EnsoniqFS tries to detect
all your drives. This may take a few seconds (a progress dialog will be shown).
After that, you'll see three folders: "CDROMs", "Image files" and "Physical
disks".

Along with that, you should see entries "Options", "Rescan devices" and "Run
Ensoniq Filesystem Tools" (see below for an explanation).

How does EnsoniqFS deal with multi-disk files?

Simply put, a multi-disk file is an instrument, which doesn't fit on one floppy
disk. The Ensoniq samplers automatically split a large instrument while saving, 
so it fills the disk to their maximum. When loading, the sampler asks for all
disks sequentially and loads the instrument from it parts.

Multi-disk file parts all reside in the root directory of a disk. Although it
is possible to put these parts into subdirectories with EnsoniqFS, your sampler
will most likely have problems with that. Ensoniq samplers put multi-disk files
into the root directory, and so should you.

When you copy multi-disk files, three cases are possible:
	
(a) Copy multi-disk from Ensoniq media to Windows media

If you copy the first part of a multi-disk file from Ensoniq media to a Windows
drive, EnsoniqFS will ask you whether to enter multi-disk mode or to copy the
file as single part. If you choose multi-disk, after the first part you will
be asked to provide the location of the next part (which can be on the same
disk or another one; you can also exchange disks, if necessary). EnsoniqFS 
automatically merges all parts into one single file.

You can also choose to copy only one single part of a multi-disk file. This way
EnsoniqFS will not ask for the next part and will not try to merge all parts
together.
	
(b) Copy multi-disk parts to Ensoniq media

(c) split large instruments into multi-disk files

###############################################################################
TODO:
	Explain file naming [xx]
	ASR audio tracks
	EnsoniqUnpackerEFE
###############################################################################



###############################################################################
TODO:
	Vista/Windows 7: Run TC with Admin rights
###############################################################################

### CDROMs

If you insert an Ensoniq formatted CDROM into any of your CDROM drives or if
you use an appropriate image file together with a CDROM emulation driver, you
will see these CDROMs inside the CDROM folder. You can than browse them the
same way as you do with Windows files and folders.

### Image files

Initially, the image folder is empty. To add and remove image files, go one
level up to the main directory of EnsoniqFS and call the "Options" dialog
(see Options). 

Once an image is mounted, it will show up in the "Image files" folder. You
can browse it with Total Commander the same way as you do with other folders
and files.

Note: Having many images mounted can be a real memory eater. For each image
file a cache and lots of other data structures are created. So be sure to
unmount unused image files.

### Physical disks

In this folder, all harddisks, floppy disks and other removable media which
were found during the detection process will be shown. Enter this folder to
browse them.

### Options

If you hit "Enter" on the "Options" entry (or if you double click it), an
options dialog is shown. Here you can modify EnsoniqFS's behaviour.

[ ] Enable floppy disk access
Activate this checkbox to allow EnsoniqFS to use your floppy disk drives.

[ ] Enable CDROM access
Activate this checkbox to allow EnsoniqFS to use your CDROM drives.

[ ] Enable removable/fixed disk access
Activate this checkbox to allow EnsoniqFS to use your SCSI and IDE harddisks 
and removable disks (e. g. ZIP drives should show up there). 

[ ] Enable image file support
Activate this checkbox to allow EnsoniqFS to use image files. EnsoniqFS
supports ISO (read/write) and BIN, GKH, EDE, EDA, EDT, EDV (read only).

If you uncheck any of the above options, EnsoniqFS doesn't scan the associated
drives or files. This can speed up the detection process.

[ ] Re-scan device list everytime the \\\Ensoniq filesystems folder is entered
Normally, EnsoniqFS updates the device list every time you enter the "Root"
directory of EnsoniqFS. This causes a delay, a progress dialog will be shown.
If you do not want EnsoniqFS to rescan that often, you should disable this
checkbox. You can update the device list manually (see below).

[ ] Enable logging to C:\EnsoniqFS-LOG.txt
If you activate this option, the EnsoniqFS plugin will create a log file at
your C: drive. EnsoniqFS will then write extensive logging information to this
file. This logging information can come handy for the developer if you've found
some bug. Note: activating this option slows down EnsoniqFS, and the log file
can grow quite fast to megabytes.


Group "Bank file adaption"
The three parameters in this group are related to the optional adaption of bank
files during copy / move operations. See section 4.3 for a detailed description
of this feature.

[ ] Automatically adapt references in banks
If you activate this option, bank files are automatically adapted when performing
a copy / move operation. The following two parameters are only relevant when
this option is active.

Adapt source device references from [Combobox]
Select here which entries in a bank file should be adapted. You can either adapt
all entries regardless of the device of the bank entry or select a specific 
device. In the second case, only bank entries are adapted that are pointing to
a file on the selected device. The device is either the floppy drive or any of
the eight possible SCSI devices. 
Note that your Ensoniq is assigned to SCSI ID 3. It is unlikely that you have 
bank entries pointing to this device (only software errors could have caused 
such entries), so you won't need this value normally.

Target device for adapted references [Combobox]
Select a device for all bank entries that are adapted during the adaption process.
In order to have working bank files the target medium must be visible for your
Ensoniq under this device.
Since the Ensoniq is assigned to SCSI ID 3 this value is not available.


If you select an image file from the dropdown box at the bottom of the options
dialog, you can click on the "Unmount selected image" button to remove that
image file from the list. With "Mount new image..." you can add new image files
to the list.

### Rescan devices

In case you disabled the automatic device detection, you can issue a new
detection by hitting "Enter" on "Rescan devices" (or by double clicking it).
EnsoniqFS will then scan all available devices for EnsoniqMedia.

You should do this if either your configuration changed (maybe you plugged in
another SCSI disk or ZIP drive) or if you inserted new removable media into
one of your devices.

### Run Ensoniq Filesystem Tools

Hit "Enter" on that item to start ETools, the Ensoniq Filesystem backup/
restore/format/check/repair tool (see below for a description).

## Ensoniq Filesystem Tools (ETools)

ETools is a piece of software which can do the following things for you:
- completely backup an Ensoniq drive to an ISO file
- restore ISO file to drive
- format drive (at the moment, only QuickFormat is supported
- check Ensoniq file systems for errors and fix these errors if possible
  
To operate on a drive, first select it from the dropdown list on top of the
application. Then choose either to backup, restore, check or format this drive.

For backup, at the moment two methods are supported: Full backup and partition
backup (the third option, file backup, is not implemented yet). If you choose
partition backup, only the space which is allocated by the Ensoniq file system
will be written to an ISO file. This could be useful if you formatted a big
drive to a lower capacity and don't want to have a huge full drive backup.

## Automatic adaption of bank files during copy / move

### Introduction to Ensoniq bank files

The Ensoniq bank files store references for up to eight instrument files and
an optional reference to a song file. When you create a bank file on your
Ensoniq, you normally follow the procedure described in the Ensoniq manual:

In the first step, you save all the instruments (and the song, if used) to
the desired location(s). In the second step, you save the bank file.

During the first step, the Ensoniq records internally, which file has gone
where i.e. it stores all information in order to load the instrument or the
song later.
In the second step, this information is stored in the bank file.

The reference to a file includes the device type (floppy, SCSI drive
including SCSI ID 0 up to 7), the disk label (only EPS16+ and ASR-10) and
a list of pointers to the file. 
The pointer list is simply the chain of file numbers that lead to the file
on the specified disk. The list starts in the root directory and the numbers
are the file numbers you see when you browse through the directory in LOAD
mode on your Ensoniq. The last pointer in the list always points to a file,
the other pointers are always directories.

The bank file itself can be moved without breaking the references. The only
restriction applies for the EPS: the bank file and the referenced files must
reside on the same disk.

A detailed technical description of the contents inside the bank files can be
found in Thoralt Franz's document "The Ensoniq EPS/EPS16+/ASR-10 Bank Format"
(http://www.thoralt.de/download/The_Ensoniq_Bank_Format.pdf).


### Problems with handling bank files

Due to this concept used for the banks, some problems arouse:
1) instruments and songs cannot be moved without invalidating the bank files
   which point to these instruments/songs.
2) the SCSI ID of a disk or the disk label cannot be changed (again
   invalidates the references used in the banks).
3) even when copying / moving banks together with the referenced files while
   keeping the directory structure and using the same device type and disk
   label, the references can get broken: this can happen when files are saved
   in a different order to the target directory. E.g. the source directory
   contains two instruments and a bank, which references the two instruments:
    * File 1: Instrument1
    * File 2: Instrument2
    * File 3: Bank
  Now you copy these three files to a new directory in a different order:
    * File 1: Instrument2
    * File 2: Instrument1
    * File 3: Bank
  Since the bank works only with the file numbers, it will load the
  instruments to the wrong tracks. Especially copying with TotalCommander
  will certainly change the order since it processes the files in alphabetical
  order.

The Ensoniq does not give any information when loading a bank where references
are broken. If a file cannot be found, it skips it silently and goes on to the
next entry. There is no chance to query why a file could not be loaded and
where it would have been expected.

As a consequence of the design of the bank files, almost all operations in
Total Commander which involve copying or moving files will break the
references in bank files. This is where the automatic adaption of the bank
files come into play.


### Automatic adaption of bank files

The basic idea of adaption of the bank files is to change the references
inside bank files in order that they point to the correct file locations.
The only way to make this possible is to collect the necessary information
during a copy / move operation in Total Commander. We refer here to a
single copy / move operation regardless how many files or directories you
have selected for the operation. 

The adaption process of the banks works only when you select a group of
files and directories that contain the bank files and the referenced
instrument / song files. This restriction becomes clearer when you see,
what is done behind the scenes:
 - you have turned on in the options of the ensoniqfs plugin the automatic
   adaption of the bank files.
 - you select a collection of files / directories from one Ensoniq media
   (a floppy disk, an attached SCSI device or an image file) and start a
   copy / move action to another Ensoniq media by pressing e.g. the
   F5-key (copy).
 - Total Commander performs the copy / move action.
 - during the processing of each single file, the ensoniqfs plugin stores
   its old and its new location as a pair in a translation table. In addition,
   the location of all bank files are recorded in a second table.
 - when the copy / move operation has been completed, the ensoniqfs plugin
   looks into its table with bank files. Each bank file is processed by
   checking every reference inside. For each reference:
    * It is first checked, if the device matches the parameter
      "source device". 
    * If it matches, the file pointer list of the bank reference is
      searched in the translation table. 
    * If an entry is found, the file pointer list is updated with the
      new location of the referenced file. If no entry is found the processing
      skips to the next bank entry.
    * Finally, the device is set to the value given by the parameter
      "target device" and the disk label entry is set equal to the disk
      label of the target disk.

 - when the adaption of all banks has been completed, the translation table
   and the list of banks for processing is cleared. A new copy / move action
   can be performed.

As you can see, the ensoniqfs plugin can only gather information during the
processing of the copy / move action and use this information after the last
file has been copied / moved.


### Some recommendations / typical scenarios

Just a note before starting: always work on copies of your disks, especially
for the disks / media you are writing to. The most flexible way is using
image files. After compiling your target medium write it once to the target
device (floppy disk, hard disk, MO cartridge, CF card) and test the result
with your Ensoniq.

1) Copying single floppy disks to a larger media e.g. to a dedicated
   directory of a SCSI disk.
   
It is assumed that the bank file(s) on the floppy disk only references
instruments / songs on the same floppy disk (excluding multi disk files).
Select all files on the top level of the source disk and copy them in a
single action to the target directory. Thus the bank file(s) get adapted
to the new location on the target disk.
You can leave the parameter "source device" either to "all devices" or set
to the device which is represented by the source media. When in doubt use
the setting "all devices".
Set the parameter "target device" to the device under which the target
media is later used by the Ensoniq.


2) Copying larger collections of instruments/banks/songs e.g. an 
   Ensoniq Sound Collection CD-ROM to some kind of other media.
   
Normally these media contain banks that reference only files on the same disk.
This is similar to the first case: select all files on the top level of the
source disk and copy them in a single action to the target directory. Please
note that depending on the bank format used, only six entries are allowed in
each file pointer list. This means that files can reside in maximum inside
five directory levels (the first five file pointers can be used for the 
directory hierarchy, the last file pointer is for the file itself). The
Ensoniq CD-ROM CDR-1 e.g. uses itself already four directory levels. Thus
when planning to copy it to the target media where e.g. you want to join
several CD-ROMs, you can use at most one directory on the root level which
can be the target for the whole CD-ROM content. My CompactFlash card e.g.
for all my sound libraries contains on the top level a directory "CDR-1"
which contains the content of the CD-ROM.


3) Bank files that reference files from different devices

The EPS16+ and the ASR-10 allow banks to reference instruments / songs from
different devices. Although this is convenient its a nightmare to update these
banks when the referenced files move.
Since the concept of Total Commander includes only one source and one target
device for the copy / move actions you can only adapt those bank entries that
are located on the same device as the bank file itself. The parameter 
"source device" allows you to restrict the adaption only on the bank entries
which point to the specified device. 

There is currently no easy way to determine the device for the entries of a
bank. Either you have designed the bank yourself so you know which file is
loaded from where or you can load the bank from the original medium and
observe what is happening.
If you happen to have some software tools (Ensoniq Disk Tools or similar)
for displaying the information inside the bank file, you are better off.

To summarize: restrict the parameter "source device" to a specific device only
when you are sure that the greater amount of mixed bank entries use this
device, otherwise use the setting "all devices". 
All remaining bank entries that could not be processed remain unchanged.
You have to edit them manually using either a software tool or loading,
correcting and resaving with your Ensoniq.


4) Adapting bank files located on other media

As we have seen, the automatic adaption of bank files can only be performed
when the banks and referenced files are processed within the same copy / move
action.
Now we have the following scenario, a real life situation from my
sound library:
 * a CD-ROM with instruments / songs
 * an extra floppy disks full of bank files which
   are referencing files on the CD-ROM.
I made the floppy disk once in order to easily change the banks
(volumes, effects) for all my performances.
My intention is to have all this files together on a new medium, a Compact
Flash card.
  Step 1: Create an image of the CD-ROM.
  Step 2: Mount the image in Total Commander.
  Step 3: Copy the bank files from floppy disk to the image. They can be copied
          to an extra directory (note that changing the location of a bank file
          itself does not invalidates its content). Important: make sure to
          turn off the option "Automatically adapt references in banks"!
  Step 4: Now you have banks and the referenced files together on a single
          medium. Turn on the automatic adaption of the banks. Copy now all
          files at once to the target medium.
  Step 5: Now you have working banks together with your referenced data.





# Bugs and contact

If you find any bugs, please don't hesitate to tell me! To keep the best
overview, I ask you to file a bug report on the SourceForge page of this
project if you find something odd. You can also directly send a mail to
ensoniqfs/at/thoralt.de (substitute /at/ with @ to get a valid email address,
this is to prevent spam bots from collecting this address).

# License

The project is published under the GNU GENERAL PUBLIC LICENSE (GPL) v2. Please
read the LICENSE file for further information.

# Disclaimer

Although being constantly under test and development, this software might
contain bugs which, in worst case, do damage to your data. Please keep backups
every time (use ETools for easy backup)!

The next two paragraphs were copied from the GPL (see also the LICENSE file):
	
	NO WARRANTY

	BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
	FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN
	OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
	PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED
	OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS
	TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE
	PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,
	REPAIR OR CORRECTION.
	
	IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
	WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
	REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,
	INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING
	OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED
	TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY
	YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
	PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGES.

Please keep that in mind when using this software.
