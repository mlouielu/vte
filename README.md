Virtual TErminal
================

VTE provides a virtual terminal widget for GTK+ applications.

Installation
------------

```
$ git clone https://gitlab.gnome.org/GNOME/vte  # Get the source code of Vte
$ cd vte                                        # Change to the toplevel directory
$ meson _build                                  # Run the `configure` script
$ ninja -C _build                               # Build Vte
[ Become root if necessary ]
$ ninja -C _build install                       # Install Vte
```

You can add options to meson, for example, install prefix or debug flag

```
$ meson _build -Dprefix=/usr -Ddebugg=true                  # If you didn't run meson before, otherwise
$ meson _build -Dprefix=/usr -Ddebugg=true --reconfigure    # You should add --reconfigure if you configure it before
```

Debugging
---------

After installing Vte with `-Ddebugg=true` flag, you can use `VTE_DEBUG` variable to control
Vte to print out the debug information

```
# Change `guake` to any program that depend on Vte
$ VTE_DEBUG=selection guake

# Or, you can mixup with multiple logging level
$ VTE_DEBUG=selection,draw,cell guake

$ Or, you can use `all` to print out all logging message
$ VTE_DEBUG=all guake
```

For logging level information, please refer to enum [VteDebugFlags](src/debug.h).


Contributing
------------

Bugs should be filed here: https://gitlab.gnome.org/GNOME/vte/issues/
Please note that this is *not a support forum*; if you are a end user,
always file bugs in your distribution's bug tracker, or use their
support forums.

If you want to provide a patch, please attach them to an issue in gnome
gitlab, in the format output by the git format-patch command.
