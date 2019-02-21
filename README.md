# mod_ruby

## About

mod_ruby is an Apache module built from the ground up which embeds Ruby into
Apache. It has no relation to the original mod_ruby
(https://github.com/shugo/mod_ruby). It is written from scratch specifically for
Apache 2.x. It is designed to run in UN*X environments using Apache's classic
prefork MPM. It does not run on Windows or within the worker or event MPM's.

I started it in 2007 and put the first version in production in 2008. It is now
on the third version. It has been meticulously built, tested, documented and is
now very stable. It has been used in production 24/7/365 for medium and
high-traffic sites for years on both Debian/Ubuntu Linux and FreeBSD systems.

mod_ruby makes it possible for you to run Ruby natively in Apache. You run
straight up Ruby in CGI as well as create custom Apache handlers in which to
plug in your own framework.

mod_ruby also includes a fast, embedded RHTML parser implemented in C which
works exactly like eRuby.

## Building

This was developed on Ubuntu Linux, FreeBSD, and Mac OSX. It has not been ported
to Windows. To build, you need to the following packages:

  * Ruby 1.9 or 2.x. (Ruby header files also needed).
  * Apache, APR and APR Util headers
  * CMake

On Ubuntu, you can build as follows:

    bash $ /usr/lib/pbuilder/pbuilder-satisfydepends
    bash $ fakeroot debian/rules clean
    bash $ dpkg-buildpackage
  
On other systems you can build generally as follows:

    bash $ cmake .
    bash $ make
    bash $ make install

## Building with Docker

To build the binary files using a CentOS 7 Docker environment:

    bash $ ./script/build_docker

Edit the Dockerfile to change ruby versions and make other small tweaks.

The build will create these files in the local repo:

  * lib/librhtml.so.3.0.0
  * src/mod_ruby.so 


## Documentation

Documentation is [here](http://mikeowens.github.io/mod_ruby/mod_ruby.html).

## Developing with Docker

If you want to try fixing bugs, or patching new features into mod_ruby, 
try using the Docker image as a handy developer's environment:

    bash $ ./script/docker_run
    ... 
    0x00007f30bb13733f in accept4 () from /lib64/libc.so.6

A single Apache worker child is running in a gdb shell.  Smoke test
mod_ruby in a separate terminal window:

    bash $ curl localhost:8080
    Hi there from ruby
    <html><body>Hello World from HTML!</body></html>

If mod_ruby crashes, gdb will print a full stack trace.  You may also do 
`Ctrl + C` to break out to a gdb prompt to inspect the running Apache child.

Set breakpoints or alter gdb's startup commands in `docker/gdb.input` and rebuild
the Docker image.  Alter httpd.conf and test.rb to create different test cases.

Submit pull requests using feature branches, and have fun!

## Additional Information

Redistribution and use in source and binary forms, with or without modification,
are permitted under the terms of the Apache License Version 2.0. Copyright
information is located in the COPYING file. The software license is located in
the LICENSE file.
