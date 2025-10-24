# LLM Agent Instructions

* To compile `ModRuby`, run these commands:
  * `rm -f /usr/src/mod_ruby/CMakeCache.txt`
  * `rm -Rf /usr/src/mod_ruby/CMakeFiles`
  * `cmake3 /usr/src/mod_ruby`
  * `make -j4`
  * `make install`
  * If any compilation commands fail, the patch failed.
* To test `ModRuby`:
  * Reload Apache by running `killall httpd` -- it will automatically restart and load 
    the new `mod_ruby.so` shared object that was previously compiled.
  * Use curl to test `ModRuby` with this: `curl -m1 localhost`
  * It should output the string `Ruby check_access()\nHi there from ruby`
  * If curl times out and Apache segfaults, the patch failed
  * To inspect the backtrace, look at the file `/var/log/httpd/gdb_backtrace.txt`
  * After each crash, Apache is restarted
  * Each new crash will delete the previous contents of `/var/log/httpd/gdb_backtrace.txt`
  * If Apache completely fails to start up due to a symbol linking problem or other error,
    the output from the server will be in `/var/log/httpd/error_log` and it will 
    continue to try and restart itself every 10 seconds.  The log file might be large, 
    so something like `tail -20 /var/log/httpd/error_log` is a safer method to inspect
    this file.
* codex is running inside an Oracle Linux 8 Docker container, running as the root user.
  If you need tools or packages installed, you may use `dnf install` for distro packages
  or use other methods.
* NodeJS 22 is installed and you may install npm packages
* Python 3.12 is installed and you may construct Python utility scripts and execute them.
* If you want to inspect the Ruby source code, it is installed in `/usr/local/rvm/src/ruby-3.2.3/`
  * If you want to inspect the Ruby C++ headers, they are in `/usr/local/rvm/src/ruby-3.2.3/include/`
* If you want to inspect the Ruby binary installation, it is installed in `/usr/local/rvm/rubies/ruby-3.2.3/`
