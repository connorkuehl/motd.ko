# motd.ko

Just a simple exercise to practice the concepts detailed in Linux Device
Drivers 3rd edition (Chapter 3: Char Devices) before I moved on to reproducing
the 'scull' device driver described in the textbook.

## Building

The Makefile should be sufficient for an out-of-tree build:

```
$ make
```

## Running

Load the module by running `motd_load.sh`. This will create a device special
file named `/dev/motd0`.

You should be able to do most things that involve reading and writing to the
device.

I committed a text file that can be used for some quick tests in this repo.

```
$ cp test-motd.txt /dev/motd0
$ cat /dev/motd0
=======================
  Message of the Day
=======================

Hello, potato!
```

Opening `/dev/motd0` in write only mode should truncate the contents of
the MOTD just like any other file.

### Remarks

The size of your MOTD is limited only by the amount of memory installed
on your system :-) -- I haven't added any limits to the driver.

## Getting rid of motd.ko :'-(

Close any file descriptors you have open to the device and then run the
`motd_unload.sh` script from this repository. This will unload the driver
and remove the special file.
