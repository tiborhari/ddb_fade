# DeaDBeeF fade-in/fade-out plugin

This is a plugin for the [DeaDBeeF](http://deadbeef.sourceforge.net/)
music player. It adds a fade-in and fade-out effect, when starting, stopping
or seeking the playback.

It has 3 customisable intervals:

* Fade-in interval, when starting 
* Fade-out interval, when stopping 
* Fade-in and fade-out interval, when seeking 

# How to install

The following commands can be used to compile and install the plugin on Linux:

    git clone https://github.com/tiborhari/ddb_fade
    cd ddb_fade
    make
    cp ddb_fade.so ~/.local/lib/deadbeef/
