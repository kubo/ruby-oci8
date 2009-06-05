# oci8.rb -- implements OCI8 and OCI8::Cursor
#
# Copyright (C) 2002-2009 KUBO Takehiro <kubo@jiubao.org>
#
# Original Copyright is:
#   Oracle module for Ruby
#   1998-2000 by yoshidam
#

require 'date'

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
  def exec(sql, *bindvars)
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

  # :call-seq:
  #   select_one(sql, *bindvars) -> first_one_row
  #
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

  def inspect
    "#<OCI8:#{username}>"
  end

  # :call-seq:
  #   oracle_server_version -> oraver
  #
  # Returns an OCI8::OracleVersion of the Oracle server version.
  #
  # See also: OCI8.oracle_client_version
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

  # The instance of this class corresponds to cursor in the term of
  # Oracle, which corresponds to java.sql.Statement of JDBC and statement
  # handle $sth of Perl/DBI.
  #
  # Don't create the instance by calling 'new' method. Please create it by
  # calling OCI8#exec or OCI8#parse.
  class Cursor

    # explicitly indicate the date type of fetched value. run this
    # method within parse and exec. pos starts from 1. lentgh is used
    # when type is String.
    # 
    # example:
    #   cursor = conn.parse("SELECT ename, hiredate FROM emp")
    #   cursor.define(1, String, 20) # fetch the first column as String.
    #   cursor.define(2, Time)       # fetch the second column as Time.
    #   cursor.exec()
    def define(pos, type, length = nil)
      __define(pos, make_bind_object(:type => type, :length => length))
      self
    end # define

    # Binds variables explicitly.
    # 
    # When key is number, it binds by position, which starts from 1.
    # When key is string, it binds by the name of placeholder.
    #
    # example:
    #   cursor = conn.parse("SELECT * FROM emp WHERE ename = :ename")
    #   cursor.bind_param(1, 'SMITH') # bind by position
    #     ...or...
    #   cursor.bind_param(':ename', 'SMITH') # bind by name
    #
    # To bind as number, Fixnum and Float are available, but Bignum is
    # not supported. If its initial value is NULL, please set nil to 
    # +type+ and Fixnum or Float to +val+.
    #
    # example:
    #   cursor.bind_param(1, 1234) # bind as Fixnum, Initial value is 1234.
    #   cursor.bind_param(1, 1234.0) # bind as Float, Initial value is 1234.0.
    #   cursor.bind_param(1, nil, Fixnum) # bind as Fixnum, Initial value is NULL.
    #   cursor.bind_param(1, nil, Float) # bind as Float, Initial value is NULL.
    #
    # In case of binding a string, set the string itself to
    # +val+. When the bind variable is used as output, set the
    # string whose length is enough to store or set the length.
    #
    # example:
    #   cursor = conn.parse("BEGIN :out := :in || '_OUT'; END;")
    #   cursor.bind_param(':in', 'DATA') # bind as String with width 4.
    #   cursor.bind_param(':out', nil, String, 7) # bind as String with width 7.
    #   cursor.exec()
    #   p cursor[':out'] # => 'DATA_OU'
    #   # Though the length of :out is 8 bytes in PL/SQL block, it is
    #   # bound as 7 bytes. So result is cut off at 7 byte.
    #
    # In case of binding a string as RAW, set OCI::RAW to +type+.
    #
    # example:
    #   cursor = conn.parse("INSERT INTO raw_table(raw_column) VALUE (:1)")
    #   cursor.bind_param(1, 'RAW_STRING', OCI8::RAW)
    #   cursor.exec()
    #   cursor.close()
    def bind_param(key, param, type = nil, length = nil)
      case param
      when Hash
      when Class
        param = {:value => nil,   :type => param, :length => length}
      else
        param = {:value => param, :type => type,  :length => length}
      end
      __bind(key, make_bind_object(param))
      self
    end # bind_param

    # Executes the SQL statement assigned the cursor. The type of
    # return value depends on the type of sql statement: select;
    # insert, update and delete; create, alter, drop and PL/SQL.
    #
    # In case of select statement, it returns the number of the
    # select-list.
    #
    # In case of insert, update or delete statement, it returns the
    # number of processed rows.
    #
    # In case of create, alter, drop and PL/SQL statement, it returns
    # true. In contrast with OCI8#exec, it returns true even
    # though PL/SQL. Use OCI8::Cursor#[] explicitly to get bind
    # variables.
    def exec(*bindvars)
      bind_params(*bindvars)
      __execute(nil) # Pass a nil to specify the statement isn't an Array DML
      case type
      when :select_stmt
        define_columns()
      else
        row_count
      end
    end # exec

    # Set the maximum array size for bind_param_array
    #
    # All the binds will be clean from cursor if instance variable max_array_size is set before
    #
    # Instance variable actual_array_size holds the size of the arrays users actually binds through bind_param_array
    #  all the binding arrays are required to be the same size
    def max_array_size=(size)
      raise "expect positive number for max_array_size." if size.nil? && size <=0
      __clearBinds if !@max_array_size.nil?
      @max_array_size = size
      @actual_array_size = nil
    end # max_array_size=

    # Bind array explicitly
    #
    # When key is number, it binds by position, which starts from 1.
    # When key is string, it binds by the name of placeholder.
    # 
    # The max_array_size should be set before calling bind_param_array
    #
    # example:
    #   cursor = conn.parse("INSERT INTO test_table VALUES (:str)")
    #   cursor.max_array_size = 3
    #   cursor.bind_param_array(1, ['happy', 'new', 'year'], String, 30)
    #   cursor.exec_array
    def bind_param_array(key, var_array, type = nil, max_item_length = nil)
      raise "please call max_array_size= first." if @max_array_size.nil?
      raise "expect array as input param for bind_param_array." if !var_array.nil? && !(var_array.is_a? Array) 
      raise "the size of var_array should not be greater than max_array_size." if !var_array.nil? && var_array.size > @max_array_size

      if var_array.nil? 
        raise "all binding arrays should be the same size." unless @actual_array_size.nil? || @actual_array_size == 0
        @actual_array_size = 0
      else
        raise "all binding arrays should be the same size." unless @actual_array_size.nil? || var_array.size == @actual_array_size
        @actual_array_size = var_array.size if @actual_array_size.nil?
      end
      
      param = {:value => var_array, :type => type, :length => max_item_length, :max_array_size => @max_array_size}
      first_non_nil_elem = var_array.nil? ? nil : var_array.find{|x| x!= nil}
      
      if type.nil?
        if first_non_nil_elem.nil?
          raise "bind type is not given."
        else
          type = first_non_nil_elem.class
        end
      end
      
      bindclass = OCI8::BindType::Mapping[type]
      raise "unsupported dataType: #{type}" if bindclass.nil?
      bindobj = bindclass.create(@con, var_array, param, @max_array_size)
      __bind(key, bindobj)
      self
    end # bind_param_array

    # Executes the SQL statement assigned the cursor with array binding
    def exec_array
      raise "please call max_array_size= first." if @max_array_size.nil?

      if !@actual_array_size.nil? && @actual_array_size > 0
        __execute(@actual_array_size)
      else
        raise "please set non-nil values to array binding parameters"
      end

      case type
      when :update_stmt, :delete_stmt, :insert_stmt
        row_count
      else
        true
      end
    end # exec_array

    # Gets the names of select-list as array. Please use this
    # method after exec.
    def get_col_names
      @names ||= @column_metadata.collect { |md| md.name }
    end # get_col_names

    # call-seq:
    #   column_metadata -> column information
    #
    # (new in 1.0.0 and 2.0)
    #
    # Gets an array of OCI8::Metadata::Column of a select statement.
    #
    # example:
    #   cursor = conn.exec('select * from tab')
    #   puts ' Name                                      Type'
    #   puts ' ----------------------------------------- ----------------------------'
    #   cursor.column_metadata.each do |colinfo|
    #     puts format(' %-41s %s',
    #                 colinfo.name,
    #                 colinfo.type_string)
    #   end
    def column_metadata
      @column_metadata
    end

    # call-seq:
    #   fetch_hash
    #
    # get fetched data as a Hash. The hash keys are column names.
    # If a block is given, acts as an iterator.
    def fetch_hash
      if iterator?
        while ret = fetch_a_hash_row()
          yield(ret)
        end
      else
        fetch_a_hash_row
      end
    end # fetch_hash

    # close the cursor.
    def close
      free()
      @names = nil
      @column_metadata = nil
    end # close

    private

    def make_bind_object(param)
      case param
      when Hash
        key = param[:type]
        val = param[:value]
        max_array_size = param[:max_array_size]

        if key.nil?
          if val.nil?
            raise "bind type is not given."
          elsif val.is_a? OCI8::Object::Base
            key = :named_type
            param = @con.get_tdo_by_class(val.class)
          else
            key = val.class
          end
        elsif key.class == Class && key < OCI8::Object::Base
          param = @con.get_tdo_by_class(key)
          key = :named_type
        end
      when OCI8::Metadata::Base
        key = param.data_type
        case key
        when :named_type
          if param.type_name == 'XMLTYPE'
            key = :xmltype
          else
            param = @con.get_tdo_by_metadata(param.type_metadata)
          end
        end
      else
        raise "unknown param #{param.intern}"
      end

      bindclass = OCI8::BindType::Mapping[key]
      raise "unsupported datatype: #{key}" if bindclass.nil?
      bindclass.create(@con, val, param, max_array_size)
    end

    def define_columns
      num_cols = __param_count
      1.upto(num_cols) do |i|
        parm = __paramGet(i)
        define_one_column(i, parm) unless __defined?(i)
        @column_metadata[i - 1] = parm
      end
      num_cols
    end # define_columns

    def define_one_column(pos, param)
      __define(pos, make_bind_object(param))
    end # define_one_column

    def bind_params(*bindvars)
      bindvars.each_with_index do |val, i|
	if val.is_a? Array
	  bind_param(i + 1, val[0], val[1], val[2])
	else
	  bind_param(i + 1, val)
	end
      end
    end # bind_params

    def fetch_a_hash_row
      if rs = fetch()
        ret = {}
        get_col_names.each do |name|
          ret[name] = rs.shift
        end
        ret
      else 
        nil
      end
    end # fetch_a_hash_row

  end # OCI8::Cursor
end # OCI8

class OraDate
  def to_time
    begin
      Time.local(year, month, day, hour, minute, second)
    rescue ArgumentError
      msg = format("out of range of Time (expect between 1970-01-01 00:00:00 UTC and 2037-12-31 23:59:59, but %04d-%02d-%02d %02d:%02d:%02d %s)", year, month, day, hour, minute, second, Time.at(0).zone)
      raise RangeError.new(msg)
    end
  end

  def to_date
    Date.new(year, month, day)
  end

  if defined? DateTime # ruby 1.8.0 or upper

    # get timezone offset.
    @@tz_offset = Time.now.utc_offset.to_r/86400

    def to_datetime
      DateTime.new(year, month, day, hour, minute, second, @@tz_offset)
    end
  end

  def yaml_initialize(type, val) # :nodoc:
    initialize(*val.split(/[ -\/:]+/).collect do |i| i.to_i end)
  end

  def to_yaml(opts = {}) # :nodoc:
    YAML.quick_emit(object_id, opts) do |out|
      out.scalar(taguri, self.to_s, :plain)
    end
  end

  def to_json(options=nil) # :nodoc:
    to_datetime.to_json(options)
  end
end

class OraNumber
  def yaml_initialize(type, val) # :nodoc:
    initialize(val)
  end

  def to_yaml(opts = {}) # :nodoc:
    YAML.quick_emit(object_id, opts) do |out|
      out.scalar(taguri, self.to_s, :plain)
    end
  end

  def to_json(options=nil) # :nodoc:
    to_s
  end
end

class Numeric
  def to_onum
    OraNumber.new(self)
  end
end

class String
  def to_onum(format = nil, nls_params = nil)
    OraNumber.new(self, format, nls_params)
  end
end
