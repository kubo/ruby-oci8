# oci8.rb -- OCI8
#
# Copyright (C) 2002-2013 Kubo Takehiro <kubo@jiubao.org>
#
# Original Copyright is:
#   Oracle module for Ruby
#   1998-2000 by yoshidam
#

require 'date'
require 'yaml'

# A connection to a Oracle database server.
#
# example:
#   # output the emp table's content as CSV format.
#   conn = OCI8.new(username, password)
#   conn.exec('select * from emp') do |row|
#     puts row.join(',')
#   end
#
#   # execute PL/SQL block with bind variables.
#   conn = OCI8.new(username, password)
#   conn.exec('BEGIN procedure_name(:1, :2); END;',
#              value_for_the_first_parameter,
#              value_for_the_second_parameter)
class OCI8

  # @return [OCIError]
  attr_accessor :last_error

  # @overload initialize(username, password, dbname = nil, privilege = nil)
  #
  # Connects to an Oracle database server by +username+ and +password+
  # at +dbname+ as +privilege+.
  #
  # === connecting to the local server
  #
  # Set +username+ and +password+ or pass "username/password" as a
  # single argument.
  #
  #   OCI8.new('scott', 'tiger')
  # or
  #   OCI8.new('scott/tiger')
  #
  # === connecting to a remote server
  #
  # Set +username+, +password+ and +dbname+ or pass
  # "username/password@dbname" as a single argument.
  #
  #   OCI8.new('scott', 'tiger', 'orcl.world')
  # or
  #   OCI8.new('scott/tiger@orcl.world')
  #
  # The +dbname+ is a net service name or an easy connectection
  # identifier. The former is a name listed in the file tnsnames.ora.
  # Ask to your DBA if you don't know what it is. The latter has the
  # syntax as "//host:port/service_name".
  #
  #   OCI8.new('scott', 'tiger', '//remote-host:1521/XE')
  # or
  #   OCI8.new('scott/tiger@//remote-host:1521/XE')
  #
  # === connecting as a privileged user
  #
  # Set :SYSDBA or :SYSOPER to +privilege+, otherwise
  # "username/password as sysdba" or "username/password as sysoper"
  # as a single argument.
  #
  #   OCI8.new('sys', 'change_on_install', nil, :SYSDBA)
  # or
  #   OCI8.new('sys/change_on_install as sysdba')
  #
  # === external OS authentication
  #
  # Set nil to +username+ and +password+, or "/" as a single argument.
  #
  #   OCI8.new(nil, nil)
  # or
  #   OCI8.new('/')
  #
  # To connect to a remote host:
  #
  #   OCI8.new(nil, nil, 'dbname')
  # or
  #   OCI8.new('/@dbname')
  #
  # === proxy authentication
  #
  # Enclose end user's username with square brackets and add it at the
  # end of proxy user's username.
  #
  #   OCI8.new('proxy_user_name[end_user_name]', 'proxy_password')
  # or
  #   OCI8.new('proxy_user_name[end_user_name]/proxy_password')
  #
  def initialize(*args)
    if args.length == 1
      username, password, dbname, mode = parse_connect_string(args[0])
    else
      username, password, dbname, mode = args
    end

    if username.nil? and password.nil?
      cred = OCI_CRED_EXT
    end
    case mode
    when :SYSDBA
      mode = OCI_SYSDBA
    when :SYSOPER
      mode = OCI_SYSOPER
    when :SYSASM
      if OCI8.oracle_client_version < OCI8::ORAVER_11_1
        raise "SYSASM is not supported on Oracle version #{OCI8.oracle_client_version}"
      end
      mode = OCI_SYSASM
    when nil
      # do nothing
    else
      raise "unknown privilege type #{mode}"
    end

    stmt_cache_size = OCI8.properties[:statement_cache_size]
    stmt_cache_size = nil if stmt_cache_size == 0

    attach_mode = 0
    if dbname.is_a? OCI8::ConnectionPool
      @pool = dbname # to prevent GC from freeing the connection pool.
      dbname = dbname.send(:pool_name)
      attach_mode |= 0x0200 # OCI_CPOOL and OCI_LOGON2_CPOOL
    end
    if stmt_cache_size
      # enable statement caching
      attach_mode |= 0x0004 # OCI_STMT_CACHE and OCI_LOGON2_STMTCACHE
    end

    if true
      # logon by the OCI function OCISessionBegin().
      allocate_handles()
      @session_handle.send(:attr_set_string, OCI_ATTR_USERNAME, username) if username
      @session_handle.send(:attr_set_string, OCI_ATTR_PASSWORD, password) if password
      if @@oracle_client_version >= ORAVER_11_1
        # 'rubyoci8' is displayed in V$SESSION_CONNECT_INFO.CLIENT_DRIVER
        # if both the client and the server are Oracle 11g or upper.
        # 424: OCI_ATTR_DRIVER_NAME
        @session_handle.send(:attr_set_string, 424, 'rubyoci8')
      end
      server_attach(dbname, attach_mode)
      session_begin(cred ? cred : OCI_CRED_RDBMS, mode ? mode : OCI_DEFAULT)
    else
      # logon by the OCI function OCILogon2().
      logon2(username, password, dbname, attach_mode)
    end

    if stmt_cache_size
      # set statement cache size
      attr_set_ub4(176, stmt_cache_size) # 176: OCI_ATTR_STMTCACHESIZE
    end

    @prefetch_rows = nil
    @username = nil
  end

  # Returns a prepared SQL handle.
  #
  # @param [String]  sql  SQL statement
  # @return [OCI8::Cursor]
  def parse(sql)
    @last_error = nil
    parse_internal(sql)
  end

  # same with OCI8#parse except that this doesn't reset OCI8#last_error.
  #
  # @private
  def parse_internal(sql)
    cursor = OCI8::Cursor.new(self, sql)
    cursor.prefetch_rows = @prefetch_rows if @prefetch_rows
    cursor
  end

  # Executes the sql statement. The type of return value depends on
  # the type of sql statement: select; insert, update and delete;
  # create, alter and drop; and PL/SQL.
  #
  # When bindvars are specified, they are bound as bind variables
  # before execution.
  #
  # == select statements without block
  # It returns the instance of OCI8::Cursor.
  #
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   cursor = conn.exec('SELECT * FROM emp')
  #   while r = cursor.fetch()
  #     puts r.join(',')
  #   end
  #   cursor.close
  #   conn.logoff
  #
  # == select statements with a block
  # It acts as iterator and returns the processed row counts. Fetched
  # data is passed to the block as array. NULL value becomes nil in ruby.
  #
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   num_rows = conn.exec('SELECT * FROM emp') do |r|
  #     puts r.join(',')
  #   end
  #   puts num_rows.to_s + ' rows were processed.'
  #   conn.logoff
  #
  # == PL/SQL block (ruby-oci8 1.0)
  # It returns the array of bind variables' values.
  #
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   conn.exec("BEGIN :str := TO_CHAR(:num, 'FM0999'); END;", 'ABCD', 123)
  #   # => ["0123", 123]
  #   conn.logoff
  #
  # Above example uses two bind variables which names are :str
  # and :num. These initial values are "the string whose width
  # is 4 and whose value is 'ABCD'" and "the number whose value is
  # 123". This method returns the array of these bind variables,
  # which may modified by PL/SQL statement. The order of array is
  # same with that of bind variables.
  #
  # If a block is given, it is ignored.
  #
  # == PL/SQL block (ruby-oci8 2.0)
  # It returns the number of processed rows.
  #
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   conn.exec("BEGIN :str := TO_CHAR(:num, 'FM0999'); END;", 'ABCD', 123)
  #   # => 1
  #   conn.logoff
  #
  # If a block is given, the bind variables' values are passed to the block after
  # executed.
  #
  #   conn = OCI8.new('scott', 'tiger')
  #   conn.exec("BEGIN :str := TO_CHAR(:num, 'FM0999'); END;", 'ABCD', 123) do |str, num|
  #     puts str # => '0123'
  #     puts num # => 123
  #   end
  #   conn.logoff
  #
  # FYI, the following code do same on ruby-oci8 1.0 and ruby-oci8 2.0.
  #   conn.exec(sql, *bindvars) { |*outvars| outvars }
  #
  # == Other SQL statements
  # It returns the number of processed rows.
  #
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   num_rows = conn.exec('UPDATE emp SET sal = sal * 1.1')
  #   puts num_rows.to_s + ' rows were updated.'
  #   conn.logoff
  #
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   conn.exec('CREATE TABLE test (col1 CHAR(6))') # => 0
  #   conn.logoff
  #
  def exec(sql, *bindvars, &block)
    @last_error = nil
    exec_internal(sql, *bindvars, &block)
  end

  # same with OCI8#exec except that this doesn't reset OCI8#last_error.
  #
  # @private
  def exec_internal(sql, *bindvars)
    begin
      cursor = parse(sql)
      ret = cursor.exec(*bindvars)
      case cursor.type
      when :select_stmt
        if block_given?
          cursor.fetch { |row| yield(row) }   # for each row
          ret = cursor.row_count()
        else
          ret = cursor
          cursor = nil # unset cursor to skip cursor.close in ensure block
          ret
        end
      when :begin_stmt, :declare_stmt # PL/SQL block
        if block_given?
          ary = []
          cursor.keys.sort.each do |key|
            ary << cursor[key]
          end
          yield(*ary)
        else
          ret
        end
      else
        ret # number of rows processed
      end
    ensure
      cursor.nil? || cursor.close
    end
  end # exec

  # Executes a SQL statement and fetches the first one row.
  #
  # @param [String] sql        SQL statement
  # @param [Object] bindvars   bind variables
  # @return [Array] an array of first row.
  def select_one(sql, *bindvars)
    cursor = self.parse(sql)
    begin
      cursor.exec(*bindvars)
      row = cursor.fetch
    ensure
      cursor.close
    end
    return row
  end

  def username
    @username || begin
      exec('select user from dual') do |row|
        @username = row[0]
      end
      @username
    end
  end

  # Sets the prefetch rows size. The default value is one.
  # When a select statement is executed, the OCI library allocate
  # prefetch buffer to reduce the number of network round trips by
  # retrieving specified number of rows in one round trip.
  #
  # Note: Active record adaptors set 100 by default.
  def prefetch_rows=(num)
    @prefetch_rows = num
  end

  # @private
  def inspect
    "#<OCI8:#{username}>"
  end

  # Returns the Oracle server version.
  #
  # @see OCI8.oracle_client_version
  # @return [OCI8::OracleVersion]
  def oracle_server_version
    unless defined? @oracle_server_version
      if vernum = oracle_server_vernum
        # If the Oracle client is Oracle 9i or upper,
        # get the server version from the OCI function OCIServerRelease.
        @oracle_server_version = OCI8::OracleVersion.new(vernum)
      else
        # Otherwise, get it from v$version.
        self.exec('select banner from v$version') do |row|
          if /^Oracle.*?(\d+\.\d+\.\d+\.\d+\.\d+)/ =~ row[0]
            @oracle_server_version = OCI8::OracleVersion.new($1)
            break
          end
        end
      end
    end
    @oracle_server_version
  end

  # Returns the Oracle database character set name such as AL32UTF8.
  #
  # @since 2.1.0
  # @return [String] Oracle database character set name
  def database_charset_name
    charset_id2name(@server_handle.send(:attr_get_ub2, OCI_ATTR_CHARSET_ID))
  end

  # Returns the client-side Oracle character set name such as AL32UTF8.
  #
  # @since 2.1.0
  # @return [String] client-side character set name
  # @private
  # @see OCI8.encoding
  def self.client_charset_name
    @@client_charset_name
  end

  if OCI8.oracle_client_version >= OCI8::ORAVER_11_1
    # Returns send timeout in seconds.
    # Zero means no timeout.
    # This is equivalent to {http://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF228 SQLNET.SEND_TIMEOUT} in client-side sqlnet.ora.
    #
    # @return [Float] seconds
    # @see #recv_timeout
    # @since 2.1.8 and Oracle 11.1
    def send_timeout
      # OCI_ATTR_SEND_TIMEOUT = 435
      @server_handle.send(:attr_get_ub4, 435).to_f / 1000
    end

    # Sets send timeout in seconds.
    # Zero means no timeout.
    # This is equivalent to {http://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF228 SQLNET.SEND_TIMEOUT} in client-side sqlnet.ora.
    #
    # If you have trouble by setting this, don't use it because it uses
    # {http://blog.jiubao.org/2015/01/undocumented-oci-handle-attributes.html an undocumented OCI handle attribute}.
    #
    # @param [Float] timeout
    # @return [void]
    # @see #recv_timeout=
    # @since 2.1.8 and Oracle 11.1
    def send_timeout=(timeout)
      # OCI_ATTR_SEND_TIMEOUT = 435
      @server_handle.send(:attr_set_ub4, 435, timeout * 1000)
    end

    # Returns receive timeout in seconds.
    # Zero means no timeout.
    # This is equivalent to {http://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF227 SQLNET.RECV_TIMEOUT} in client-side sqlnet.ora.
    #
    # @return [Float] seconds
    # @see #send_timeout
    # @since 2.1.8 and Oracle 11.1
    def recv_timeout
      # OCI_ATTR_RECEIVE_TIMEOUT = 436
      @server_handle.send(:attr_get_ub4, 436).to_f / 1000
    end

    # Sets receive timeout in seconds.
    # Zero means no timeout.
    # This is equivalent to {http://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF227 SQLNET.RECV_TIMEOUT} in client-side sqlnet.ora.
    #
    # If you have trouble by setting this, don't use it because it uses
    # {http://blog.jiubao.org/2015/01/undocumented-oci-handle-attributes.html an undocumented OCI handle attribute}.
    #
    # @param [Float] timeout
    # @return [void]
    # @see #send_timeout=
    # @since 2.1.8 and Oracle 11.1
    def recv_timeout=(timeout)
      # OCI_ATTR_RECEIVE_TIMEOUT = 436
      @server_handle.send(:attr_set_ub4, 436, timeout * 1000)
    end
  else
    def send_timeout
      raise NotImplementedError, 'send_timeout is unimplemented in this Oracle version'
    end
    def send_timeout=(timeout)
      raise NotImplementedError, 'send_timeout= is unimplemented in this Oracle version'
    end
    def recv_timeout
      raise NotImplementedError, 'recv_timeout is unimplemented in this Oracle version'
    end
    def recv_timeout=(timeout)
      raise NotImplementedError, 'revc_timeout= is unimplemented in this Oracle version'
    end
  end
end

class OCIError

  # @overload initialize(message, error_code = nil, sql_stmt = nil, parse_error_offset = nil)
  #   Creates a new OCIError object with specified parameters.
  #
  #   @param [String]   message    error message
  #   @param [Integer]  error_code Oracle error code
  #   @param [String]   sql_stmt   SQL statement
  #   @param [Integer]  parse_error_offset
  #
  #   @example
  #     OCIError.new("ORA-00001: unique constraint (%s.%s) violated", 1, 'insert into table_name values (1)', )
  #     # => #<OCIError: ORA-00001: unique constraint (%s.%s) violated>
  #     #<OCIError: ORA-00923: FROM keyword not found where expected>
  #     "select sysdate"
  #     923
  #     14
  #
  # @overload initialize(error_code, *params)
  #   Creates a new OCIError object with the error message which corresponds to the specified
  #   Oracle error code.
  #
  #   @param [Integer]  error_code  Oracle error code
  #   @param [String, ...] params   parameters which replace '%s'
  #
  #   @example
  #     # without parameters
  #     OCIError.new(4043)
  #     # When NLS_LANG=american_america.AL32UTF8
  #     # => #<OCIError: ORA-04043: object %s does not exist>
  #     # When NLS_LANG=german_germany.AL32UTF8
  #     # => #<OCIError: ORA-04043: Objekt %s ist nicht vorhanden>
  #     
  #     # with one parameter
  #     OCIError.new(4043, 'table_name')
  #     # When NLS_LANG=american_america.AL32UTF8
  #     # => #<OCIError: ORA-04043: object table_name does not exist>
  #     # When NLS_LANG=german_germany.AL32UTF8
  #     # => #<OCIError: ORA-04043: Objekt table_name ist nicht vorhanden>
  #
  def initialize(*args)
    if args.length > 0
      if args[0].is_a? Fixnum
        @code = args.shift
        super(OCI8.error_message(@code).gsub('%s') {|s| args.empty? ? '%s' : args.shift})
        @sql = nil
        @parse_error_offset = nil
      else
        msg, @code, @sql, @parse_error_offset = args
        super(msg)
      end
    else
      super()
    end
  end
end

class OraDate

  # Returns a Time object which denotes self.
  def to_time
    begin
      Time.local(year, month, day, hour, minute, second)
    rescue ArgumentError
      msg = format("out of range of Time (expect between 1970-01-01 00:00:00 UTC and 2037-12-31 23:59:59, but %04d-%02d-%02d %02d:%02d:%02d %s)", year, month, day, hour, minute, second, Time.at(0).zone)
      raise RangeError.new(msg)
    end
  end

  # Returns a Date object which denotes self.
  def to_date
    Date.new(year, month, day)
  end

  if defined? DateTime # ruby 1.8.0 or upper

    # timezone offset of the time the command started
    # @private
    @@tz_offset = Time.now.utc_offset.to_r/86400

    # Returns a DateTime object which denotes self.
    #
    # Note that this is not daylight saving time aware.
    # The Time zone offset is that of the time the command started.
    def to_datetime
      DateTime.new(year, month, day, hour, minute, second, @@tz_offset)
    end
  end

  # @private
  def yaml_initialize(type, val)
    initialize(*val.split(/[ -\/:]+/).collect do |i| i.to_i end)
  end

  # @private
  def to_yaml(opts = {})
    YAML.quick_emit(object_id, opts) do |out|
      out.scalar(taguri, self.to_s, :plain)
    end
  end

  # @private
  def to_json(options=nil)
    to_datetime.to_json(options)
  end
end

class OraNumber

  if defined? Psych and YAML == Psych

    yaml_tag '!ruby/object:OraNumber'

    # @private
    def encode_with coder
      coder.scalar = self.to_s
    end

    # @private
    def init_with coder
      initialize(coder.scalar)
    end

  else

    # @private
    def yaml_initialize(type, val)
      initialize(val)
    end

    # @private
    def to_yaml(opts = {})
      YAML.quick_emit(object_id, opts) do |out|
        out.scalar(taguri, self.to_s, :plain)
      end
    end
  end

  # @private
  def to_json(options=nil)
    to_s
  end
end

class Numeric
  # Converts +self+ to {OraNumber}.
  #
  # @return [OraNumber]
  def to_onum
    OraNumber.new(self)
  end
end

class String # :nodoc:

  # Converts +self+ to {OraNumber}.
  # Optional <i>format</i> and <i>nls_params</i> is used as
  # {http://docs.oracle.com/cd/E11882_01/server.112/e17118/functions211.htm Oracle SQL function TO_NUMBER}
  # does.
  #
  # @example
  #   '123456.789'.to_onum # => #<OraNumber:123456.789>
  #   '123,456.789'.to_onum('999,999,999.999') # => #<OraNumber:123456.789>
  #   '123.456,789'.to_onum('999G999G999D999', "NLS_NUMERIC_CHARACTERS = ',.'") # => #<OraNumber:123456.789>
  #
  # @param [String] format
  # @param [String] nls_params
  # @return [OraNumber]
  def to_onum(format = nil, nls_params = nil)
    OraNumber.new(self, format, nls_params)
  end
end
