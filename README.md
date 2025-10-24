# ModRuby

## About

mod_ruby is an Apache module built from the ground up which embeds Ruby into
Apache. It makes it possible for you to run Ruby natively in Apache. You can run
straight up Ruby in CGI as well as create custom Apache handlers with which to
plug in your own web framework(s). It also includes a fast, embedded RHTML
parser implemented in C which works exactly like eRuby.

This implementation is written from scratch specifically for Apache 2.x and has
no relation to the original mod_ruby (https://github.com/shugo/mod_ruby). I
started it in 2007 and put the first version in production in 2008. It is now on
the third version. It has been meticulously built, tested, documented and is now
very stable. It has been used in production 24/7/365 for medium and high-traffic
sites for years on both Debian/Ubuntu Linux and FreeBSD systems.

This implementation is designed to run in UNIX environments using Apache's
classic prefork MPM. It does not run on Windows or within the worker or event
MPMs.

## Documentation

The user guide is [here](http://mikeowens.github.io/modruby/modruby.html). This
covers building, configuring and programming with the module along with a full
module API reference.

## Installation

### Building with Docker

To build the binary files using a CentOS 7 Docker environment:

    bash $ ./script/build_docker

Edit the Dockerfile to change ruby versions and make other small tweaks.

The build will create these files in the local repo:

  * lib/librhtml.so.3.0.0
  * src/mod_ruby.so 

### Building from Source

To build from source, you need to the following packages:

  * Ruby 2.x or 3.x. (Ruby header files also needed).
  * Apache, APR and APR Util headers
  * CMake

On Debian systems, you can build as follows:

    bash $ /usr/lib/pbuilder/pbuilder-satisfydepends
    bash $ fakeroot debian/rules clean
    bash $ dpkg-buildpackage
  
On other systems you can build generally as follows:

    bash $ cmake .
    bash $ make
    bash $ make install

## Contributing

### Developing with Docker

If you want to try fixing bugs, or patching new features into mod_ruby, try
using the Docker image as a handy developer's environment:

    bash $ ./script/docker_run
    ... 
    0x00007f7a7adfc7b4 in read () from /lib64/libpthread.so.0

A single Apache worker child is running in a gdb shell. Smoke test mod_ruby in a
separate terminal window:

    bash $ curl localhost:8080
    Hi there from ruby
    <html><body>Hello World from HTML!</body></html>
    
If mod_ruby crashes, gdb will print a full stack trace. You may also do
`Ctrl + C` to break out to a gdb prompt to inspect the running Apache child.

Set breakpoints or alter gdb's startup commands in `docker/gdb.input` and
rebuild the Docker image.  Alter httpd.conf and test.rb to create different test
cases.

Submit pull requests using feature branches, and have fun!

### Developing with OpenAI Codex

Working with the Ruby C API can be frustrating.  Documentation is sparse
and a lot of the experience is trial & error.

This project integrates OpenAI's Codex for an agentic approach.  There
is a separate `Dockerfile.codex` which loads in the full Ruby C++ headers
and source code and installs the Codex CLI.  The project root contains
an AGENTS.md system prompt to tell the LLM how to compile, install
and test ModRuby.  It can run web searches, install packages as root, 
search the Ruby source and behave like an entitled twerp.

Apache is started up with the container and will run in a reload loop 
if it crashes.  The agent is given instructions to test changes and 
inspect the stack trace if Apache segfaults.  It should keep iterating 
until the goal is met, which could be a very long time.  

**Use with caution.  Don't leave it run unattended.**

First, install [Codex CLI](https://developers.openai.com/codex/cli/)
on your workstation and get the credentials set up by logging into ChatGPT.  
The `ModRuby` container bind mounts your workstation's `$HOME/.codex` 
into the container and runs the agent inside the container.  The `ModRuby` 
project root is bind mounted into `/usr/src/mod_ruby` so changes made by the 
agent will be reflected immediately on your host OS.

Start up the container with:

    bash $ ./script/docker_codex 
    . . . 
    To get started, describe a task or try one of these commands: 
    >

Go wild and have fun.  

```
> Search for a potential security vulnerability in ModRuby.  Identify the 
  attack vector and write a test case to confirm the vulnerability.  Write 
  a patch to fix the vulnerability and supply code comments.
```

```
Updated Plan
  High-level roadmap before diving into the source and modifications.
    Survey key ModRuby modules for input handling to spot potential security issues.
    Create a regression test that demonstrates the vulnerability.
    Implement a fix with comments, rebuild, and rerun the new test.
 
I'm focusing on confirming an overflow issue in url_encode by running a 
death test with MALLOC_CHECK set to catch heap errors.  I'm thinking about 
creating a dedicated test compiled with AddressSanitizer to catch the integer 
overflow vulnerability in url_encode by triggering a heap-buffer-overflow if present.
```

## License

Redistribution and use in source and binary forms, with or without modification,
are permitted under the terms of the Apache License Version 2.0. Copyright
information is located in the COPYING file. The software license is located in
the LICENSE file.

## Acknowledgments

Cal Heldenbrand for Docker integration, access handler and corresponding
documentation.
