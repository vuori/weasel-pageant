weasel-pageant
--------------

`weasel-pageant` allows you to use SSH keys held by [PuTTY's](https://www.chiark.greenend.org.uk/~sgtatham/putty/)
Pageant "daemon" (or compatible, such as the version of Pageant included in 
[PuTTY-CAC](https://github.com/NoMoreFood/putty-cac) or the SSH agent mode in
[Gpg4win](https://www.gpg4win.org/)) from inside the
[Windows Subsystem for Linux](https://msdn.microsoft.com/en-us/commandline/wsl/about).

The source (and this documentation) is heavily based on
[`ssh-pageant`](https://github.com/cuviper/ssh-pageant) 1.4 by Josh Stone, which allows
interacting with Pageant from Cygwin/MSYS programs.

`weasel-pageant` works like `ssh-agent`, except that it leaves the key storage to
PuTTY's Pageant.  It sets up an authentication socket and prints the environment
variables, which allows the OpenSSH client to use it. It works by executing from the
WSL side a Win32 helper program which interfaces with Pageant and communicating with
it through pipes.

It is probably the most useful if your SSH keys can't be copied to the WSL environment,
such as when using a smart card for SSH authentication. Both Pageant-CAC and Gpg4win
have been tested (note that when using Gpg4win, only the SSH authentication functionality
will be forwarded; the Windows-side `gpg-agent` will not be available on the WSL side).

**SECURITY NOTICE:** All the usual security caveats applicable to WSL apply here too.
Most importantly, all interaction with the Win32 world happens with the credentials of
the user who started the WSL environment. In practice, *if you allow someone else to
log in to your WSL environment remotely, they can access the SSH keys stored in your
Pageant with `weasel-pageant`.* This is a fundamental part of how WSL works; if you
are not sure of what you're doing, do not  allow remote access to your WSL environment
(i.e. by starting an SSH server).

**COMPATIBILITY NOTICE:** `weasel-pageant` does not, and will never work on
a version of Windows 10 older than 1703 ("Creators Update"), because
it requires the new [Windows/Ubuntu interoperability support](https://blogs.msdn.microsoft.com/wsl/2016/10/19/windows-and-ubuntu-interoperability/)
feature shipped with version 1703 (compatibility with older Insider
builds is unclear). Presumably it should work with newer Insider builds,
but has not been tested on anything more recent than build 15063.413.

If you are still using Anniversary Update, you may be able to use the (unrelated)
[wsl-ssh-pageant](https://github.com/benpye/wsl-ssh-pageant).

## Installation

### From binaries

Download the zip file from the [releases page](https://github.com/vuori/weasel-pageant/releases)
and unpack it in a convenient location *on the Windows part of your drive*.
Because WSL can only execute Win32 binaries from `drvfs` locations, `weasel-pageant`
*will not work* if unpacked inside the WSL filesystem (onto an `lxfs` mount).
(Advanced users may place only `helper.exe` on `drvfs`, but in general it is easier
to keep the pieces together.)

### From source

A VS2017 project is included. You will need the "Desktop development with C++" and
"Linux development with C++" features.

1. In VS2017, set up a connection to your WSL environment (or a remote Linux machine)
   in Options → Cross Platform → Connection Manager.

2. Optional: If you intend to work on the Linux sources, copy the contents of
   `/usr/include` into `linux/include` under the project directory.
   This is not required for the build, but will make Intellisense more useful.

3. Hit Build Solution and both the Linux executable and the Win32 helper will be built.

If you want to create a binary package, you can use the `create_pkg.py` script
at the root of the project. This should work with Python 3.4 or newer on either
Windows or Linux.

Alternatively you can build the Linux executable directly on Linux and only use
Visual Studio for the Win32 helper (no Makefile or similar is supplied at the moment).
In theory the helper should be buildable with MinGW-w64 for a fully Linux-based
build, but this has not been tested.

The release binaries have been built with VS2017 15.3.0 Preview 2.0.

## Usage

Using `weasel-pageant` is generally similar to using `ssh-agent` on Linux and
similar operating systems. 

1. Ensure that PuTTY's Pageant is running (and holds your SSH keys).
    * weasel-pageant does not start Pageant itself.
    * Recommended: Add Pageant to your Windows startup/Autostart configuration
      so it is always available.

2. Edit your `~/.bashrc` (or `~/.bash_profile`) to add the following:

        eval $(<location where you unpacked the zip>/weasel-pageant -r -a "/tmp/.weasel-pageant-$USERNAME")

    To explain:

    * This leverages the `-r`/`--reuse` option in combination with `-a SOCKET`,
      which will only start a new daemon if the specified path does not accept connections
      already.  If the socket appears to be active, it will just set `SSH_AUTH_SOCK` and exit.

    * The exact path used for `-a` is arbitrary.  The socket will be created
      with only user-accessible permissions, but you may still want to use a more
      private path than shown above if multiple users can access your WSL instance.

    * When using this, the `weasel-pageant` daemon and its helper process `helper.exe`
      remains active (both will be visible in the Windows task manager).  You should not
      kill these processes, since open shells might still be using the socket. 

    * Using `eval` will set the environment variables in the current shell.
      By default, `weasel-pageant` tries to detect the current shell and output
      appropriate commands. If detection fails, then use the `-S SHELL` option
      to define a shell type manually.

3. Restart your shell or type (when using bash) `. ~/.bashrc`. Typing `ssh-add -l`
   should now list the keys you have registered in Pageant.

## Options

`weasel-pageant` aims to be compatible with `ssh-agent` options, with a few extras:

    $ weasel-pageant -h
    Usage: weasel-pageant [options] [command [arg ...]]
    Options:
      -h, --help     Show this help.
      -v, --version  Display version information.
      -c             Generate C-shell commands on stdout.
      -s             Generate Bourne shell commands on stdout.
      -S SHELL       Generate shell command for "bourne", "csh", or "fish".
      -k             Kill the current weasel-pageant.
      -d             Enable debug mode.
      -q             Enable quiet mode.
      -a SOCKET      Create socket on a specific path.
      -r, --reuse    Allow to reuse an existing -a SOCKET.
      -H, --helper   Path to the Win32 helper binary (default: /mnt/c/Program Files/weasel-pageant/helper.exe).
      -t TIME        Limit key lifetime in seconds (not supported by Pageant).

By default, the Win32 helper will be searched for in the same directory where `weasel-pageant`
is stored. If you have placed it elsewhere, the `-H` flag can be used to set the location.

## Known issues

* The Win32 helper cannot be restarted if it's killed or crashes. There appears to be a bug
  in WSL that causes pipes passed to Win32 executables to be unusable after some point
  in program execution (possibly related to forking processes or opening sockets). Currently,
  `weasel-pageant` will exit if it detects that the helper has exited.

## Uninstallation

To uninstall, just remove the extracted files and any modifications you made
to your shell initialization files (i.e. `.bashrc`).

## Version History

* 2017-06-25: 1.0 - Initial release.

## Contributions

Please send bug reports using Github's [issues feature](https://github.com/vuori/weasel-pageant/issues).
Pull requests are also welcome, though if you intend to do major changes it's recommended to open an
issue first.

------------------------------------------------------------------------------
Copyright 2017  Valtteri Vuorikoski

Based on `ssh-pageant`, copyright (C) 2009-2014  Josh Stone  

Licensed under the GNU GPL version 3 or later, http://gnu.org/licenses/gpl.html

This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

See the `COPYING` file for license details.  
Part of `weasel-pageant` is derived from the PuTTY program, whose original license is
in the file `COPYING.PuTTY`.
