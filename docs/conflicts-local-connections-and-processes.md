# @title Conflicts between Local Connections and Child Process Handling on Unix

Background
==========

When a local (not-TCP) Oracle connection is established on Unix,
Oracle client library changes signal handlers in the process to reap
all dead child processes. However it conflicts with child process
handling in ruby.

Problem 1: It trashes child process handling of Open3::popen.
==========

    require 'oci8'
    require 'open3'
    
    Open3::popen3('true') do |i, o, e, w|
      p w.value
    end
    
    conn = OCI8.new(username, password)
    puts "establish a local connection"
    
    Open3::popen3('true') do |i, o, e, w|
      p w.value
    end

The above code outputs the following result:

    #<Process::Status: pid 19236 exit 0>
    establish a local connection
    nil

`w.value` after a local connection doesn't work because Oracle reaps
the child process on the process termination before ruby detects it.

Problem 2: It creates defunct processes after disconnection if signal handlers are reset.
==========

The `system` function overwrites signal handlers.
It fixes the problem 1 as follows.

    require 'oci8'
    require 'open3'
    
    Open3::popen3('true') do |i, o, e, w|
      p w.value
    end
    
    conn = OCI8.new(username, password) # Signal handlers are changed here.
    puts "establish a local connection"
    system('true') # Signal handlers are reset here.
    
    Open3::popen3('true') do |i, o, e, w|
      p w.value
    end

The above code outputs the following result:

    #<Process::Status: pid 19652 exit 0>
    establish a local connection
    #<Process::Status: pid 19656 exit 0>

`w.value` after a local connection works.

However it makes another problem.

    require 'oci8'
    
    conn = OCI8.new(username, password) # Signal handlers are changed here.
    # An Oracle server process is created here.
    puts "establish a local connection"
    system('true') # Signal handlers are reset here.
    
    conn.logoff # The Oracle server process exits and become defunct.
    
    # ... do other stuffs...

If a program repeatedly creates a local connection and disconnects it,
the number of defunct processes increases gradually.

Solution: BEQUEATH_DETACH=YES
==========

By setting [BEQUEATH_DETACH=YES][] in `sqlnet.ora`, Oracle client library
doesn't change signal handlers. The above two problems are fixed.

Oracle client library reads `sqlnet.ora` in the following locations:

* `$TNS_ADMIN/sqlnet.ora` if the environment variable `TNS_ADMIN`
  is set and `$TNS_ADMIN/sqlnet.ora` exists.
  Otherwise, `$ORACLE_HOME/network/admin/sqlnet.ora`.
* `$HOME/.sqlnet.ora`

[BEQUEATH_DETACH=YES]: https://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF183
