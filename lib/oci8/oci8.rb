#   --*- ruby -*--
# This is based on yoshidam's oracle.rb.

require 'date'

# The database connection class.
class OCI8
  # Executes the sql statement. The type of return value depends on
  # the type of sql statement: select; insert, update and delete;
  # create, alter and drop; and PL/SQL.
  # 
  # When bindvars are specified, they are bound as bind variables
  # before execution.
  # 
  # In case of select statement with no block, it returns the
  # instance of OCI8::Cursor.
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
  # In case of select statement with a block, it acts as iterator and
  # returns the processed row counts. Fetched data is passed to the
  # block as array. NULL value becomes nil in ruby.
  #
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   num_rows = conn.exec('SELECT * FROM emp') do |r|
  #     puts r.join(',')
  #   end
  #   puts num_rows.to_s + ' rows were processed.'
  #   conn.logoff
  #
  # In case of insert, update or delete statement, it returns the
  # number of processed rows.
  # 
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   num_rows = conn.exec('UPDATE emp SET sal = sal * 1.1')
  #   puts num_rows.to_s + ' rows were updated.'
  #   conn.logoff
  #
  # In case of create, alter or drop statement, it returns true.
  #
  # example:
  #   conn = OCI8.new('scott', 'tiger')
  #   conn.exec('CREATE TABLE test (col1 CHAR(6))')
  #   conn.logoff
  #
  # In case of PL/SQL statement, it returns the array of bind
  # variables.
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
  def exec(sql, *bindvars)
    begin
      cursor = OCI8::Cursor.new(self)
      cursor.parse(sql)
      ret = cursor.exec(*bindvars)
      case cursor.type
      when :select_stmt
        if block_given?
          cursor.fetch { |row| yield(row) }   # for each row
          cursor.row_count()
        else
          ret = cursor
          cursor = nil # unset cursor to skip cursor.close in ensure block
          ret
        end
      when :begin_stmt, :declare_stmt # PL/SQL block
        ary = []
        cursor.keys.sort.each do |key|
          ary << cursor[key]
        end
        ary
      else
        ret
      end
    ensure
      cursor.nil? || cursor.close
    end
  end # exec

  # Creates cursor, prepare to execute SQL statement and return the
  # instance of OCI8::Cursor.
  def parse(sql)
    cursor = OCI8::Cursor.new(self)
    cursor.parse(sql)
    cursor
  end # parse

  module BindType
    Mapping = {}

    # get/set Time
    class Time < OCI8::BindType::OraDate
      def set(val)
        super(val && ::OraDate.new(val.year, val.mon, val.mday, val.hour, val.min, val.sec))
      end
      def get()
        (val = super()) && val.to_time
      end
    end

    # get/set Date
    class Date < OCI8::BindType::OraDate
      def set(val)
        super(val && ::OraDate.new(val.year, val.mon, val.mday))
      end
      def get()
        (val = super()) && val.to_date
      end
    end

    # get/set DateTime
    class DateTime < OCI8::BindType::OCITimestampTZ
      def set(val)
        super(val && ::OCITimestampTZ.new(val))
      end
      def get()
        (val = super()) && val.to_datetime
      end
    end

    # get/set Number (for OCI8::SQLT_NUM)
    class Number
      def self.dispatch(val, length, precision, scale) # :nodoc:
        if scale == -127
          if precision == 0
            # NUMBER declared without its scale and precision. (Oracle 9.2.0.3 or above)
            bind_type = OCI8::Cursor.instance_eval do ::OCI8::Cursor.bind_default_number end
          else
            # FLOAT or FLOAT(p)
            bind_type = OCI8::BindType::Float
          end
        elsif scale == 0
          if precision == 0
            # NUMBER whose scale and precision is unknown
            # or
            # NUMBER declared without its scale and precision. (Oracle 9.2.0.2 or below)
            bind_type = OCI8::Cursor.instance_eval do ::OCI8::Cursor.bind_unknown_number end
          elsif precision <= 9
            # NUMBER(p, 0); p is less than or equals to the precision of Fixnum
            bind_type = OCI8::BindType::Fixnum
          else
            # NUMBER(p, 0); p is greater than the precision of Fixnum
            bind_type = OCI8::BindType::Integer
          end
        else
          # NUMBER(p, s)
          if precision < 15 # the precision of double.
            bind_type = OCI8::BindType::Float
          else
            # use BigDecimal instead?
            bind_type = OCI8::BindType::OraNumber
          end
        end
        bind_type
      end
    end

    # bind or explicitly define
    Mapping[::String]       = ::OCI8::BindType::String
    Mapping[::OraNumber]    = ::OCI8::BindType::OraNumber
    Mapping[::OCINumber]    = ::OCI8::BindType::OCINumber
    Mapping[::Fixnum]       = ::OCI8::BindType::Fixnum
    Mapping[::Float]        = ::OCI8::BindType::Float
    Mapping[::Integer]      = ::OCI8::BindType::Integer
    Mapping[::Bignum]       = ::OCI8::BindType::Integer
    Mapping[::OraDate]      = ::OCI8::BindType::OraDate
    Mapping[::Time]         = ::OCI8::BindType::Time
    Mapping[::Date]         = ::OCI8::BindType::Date
    Mapping[::DateTime]     = ::OCI8::BindType::DateTime
    Mapping[::OCIRowid]     = ::OCI8::BindType::OCIRowid
    Mapping[::OCI8::BLOB]   = ::OCI8::BindType::BLOB
    Mapping[::OCI8::CLOB]   = ::OCI8::BindType::CLOB
    Mapping[::OCI8::Cursor] = ::OCI8::BindType::Cursor

    # implicitly define

    # datatype        type     size prec scale
    # -------------------------------------------------
    # CHAR(1)       SQLT_AFC      1    0    0
    # CHAR(10)      SQLT_AFC     10    0    0
    Mapping[:char] = ::OCI8::BindType::String

    # datatype        type     size prec scale
    # -------------------------------------------------
    # VARCHAR(1)    SQLT_CHR      1    0    0
    # VARCHAR(10)   SQLT_CHR     10    0    0
    # VARCHAR2(1)   SQLT_CHR      1    0    0
    # VARCHAR2(10)  SQLT_CHR     10    0    0
    Mapping[:varchar2] = ::OCI8::BindType::String

    # datatype        type     size prec scale
    # -------------------------------------------------
    # RAW(1)        SQLT_BIN      1    0    0
    # RAW(10)       SQLT_BIN     10    0    0
    Mapping[:raw] = ::OCI8::BindType::RAW

    # datatype        type     size prec scale
    # -------------------------------------------------
    # LONG          SQLT_LNG      0    0    0
    Mapping[:long] = ::OCI8::BindType::String

    # datatype        type     size prec scale
    # -------------------------------------------------
    # LONG RAW      SQLT_LBI      0    0    0
    Mapping[:long_raw] = ::OCI8::BindType::RAW

    # datatype        type     size prec scale
    # -------------------------------------------------
    # CLOB          SQLT_CLOB  4000    0    0
    Mapping[:clob] = ::OCI8::BindType::CLOB

    # datatype        type     size prec scale
    # -------------------------------------------------
    # BLOB          SQLT_BLOB  4000    0    0
    Mapping[:blob] = ::OCI8::BindType::BLOB

    # datatype        type     size prec scale
    # -------------------------------------------------
    # DATE          SQLT_DAT      7    0    0
    Mapping[:date] = ::OCI8::BindType::OraDate

    # datatype        type     size prec scale
    # -------------------------------------------------
    # ROWID         SQLT_RDD      4    0    0
    Mapping[:rowid] = ::OCI8::BindType::OCIRowid

    # datatype           type     size prec scale
    # -----------------------------------------------------
    # FLOAT            SQLT_NUM     22  126 -127
    # FLOAT(1)         SQLT_NUM     22    1 -127
    # FLOAT(126)       SQLT_NUM     22  126 -127
    # DOUBLE PRECISION SQLT_NUM     22  126 -127
    # REAL             SQLT_NUM     22   63 -127
    # NUMBER           SQLT_NUM     22    0    0
    # NUMBER(1)        SQLT_NUM     22    1    0
    # NUMBER(38)       SQLT_NUM     22   38    0
    # NUMBER(1, 0)     SQLT_NUM     22    1    0
    # NUMBER(38, 0)    SQLT_NUM     22   38    0
    # NUMERIC          SQLT_NUM     22   38    0
    # INT              SQLT_NUM     22   38    0
    # INTEGER          SQLT_NUM     22   38    0
    # SMALLINT         SQLT_NUM     22   38    0
    Mapping[:number] = ::OCI8::BindType::Number
  end # BindType

  # The instance of this class corresponds to cursor in the term of
  # Oracle, which corresponds to java.sql.Statement of JDBC and statement
  # handle $sth of Perl/DBI.
  #
  # Don't create the instance by calling 'new' method. Please create it by
  # calling OCI8#exec or OCI8#parse.
  class Cursor

    # number column declared without its scale and precision. (Oracle 9.2.0.3 or above)
    @@bind_default_number = OCI8::BindType::Float
    # number value whose scale and precision is unknown
    # or
    # number column declared without its scale and precision. (Oracle 9.2.0.2 or below)
    @@bind_unknown_number = OCI8::BindType::Float

    def self.bind_default_number
      @@bind_default_number
    end

    def self.bind_default_number=(val)
      @@bind_default_number = val
    end

    def self.bind_unknown_number
      @@bind_unknown_number
    end

    def self.bind_unknown_number=(val)
      @@bind_unknown_number = val
    end

    def parse(sql)
      free_binds()
      @names = []
      __prepare(sql)
    end # parse

    # explicitly indicate the date type of fetched value. run this
    # method within parse and exec. pos starts from 1. lentgh is used
    # when type is String.
    # 
    # example:
    #   cursor = conn.parse("SELECT ename, hiredate FROM emp")
    #  cursor.define(1, String, 20) # fetch the first column as String.
    #  cursor.define(2, Time)       # fetch the second column as Time.
    #  cursor.exec()
    def define(pos, type, length = nil)
      @defns ||= []
      if type == String and length.nil?
	length = 4000
      end
      b = bind_or_define(:define, pos, nil, type, length, nil, nil, false)
      @defns[pos - 1].nil? or @defns[pos - 1].free()
      @defns[pos - 1] = b
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
    def bind_param(key, val, type = nil, length = nil)
      @binds = {} if @binds.nil?
      b = bind_or_define(:bind, key, val, type, length, nil, nil, false)
      @binds[key].free() unless @binds[key].nil?
      @binds[key] = b
      self
    end # bind_param

    # Gets the value of the bind variable.
    #
    # In case of binding explicitly, use same key with that of
    # OCI8::Cursor#bind_param. A placeholder can be bound by 
    # name or position. If you bind by name, use that name. If you bind
    # by position, use the position.
    #
    # example:
    #   cursor = conn.parse("BEGIN :out := 'BAR'; END;")
    #   cursor.bind_param(':out', 'FOO') # bind by name
    #   p cursor[':out'] # => 'FOO'
    #   p cursor[1] # => nil
    #   cursor.exec()
    #   p cursor[':out'] # => 'BAR'
    #   p cursor[1] # => nil
    #
    # example:
    #   cursor = conn.parse("BEGIN :out := 'BAR'; END;")
    #   cursor.bind_param(1, 'FOO') # bind by position
    #   p cursor[':out'] # => nil
    #   p cursor[1] # => 'FOO'
    #   cursor.exec()
    #   p cursor[':out'] # => nil
    #   p cursor[1] # => 'BAR'
    #
    # In case of binding by OCI8#exec or OCI8::Cursor#exec,
    # get the value by position, which starts from 1.
    #
    # example:
    #   cursor = conn.exec("BEGIN :out := 'BAR'; END;", 'FOO')
    #   # 1st bind variable is bound as String with width 3. Its initial value is 'FOO'
    #   # After execute, the value become 'BAR'.
    #   p cursor[1] # => 'BAR'
    def [](key)
      if @binds.nil? or @binds[key].nil?
	return nil 
      end
      @binds[key].get()
    end

    # Sets the value to the bind variable. The way to specify the
    # +key+ is same with OCI8::Cursor#[]. This is available
    # to replace the value and execute many times.
    #
    # example1:
    #   cursor = conn.parse("INSERT INTO test(col1) VALUES(:1)")
    #   cursor.bind_params(1, nil, String, 3)
    #   ['FOO', 'BAR', 'BAZ'].each do |key|
    #     cursor[1] = key
    #     cursor.exec
    #   end
    #   cursor.close()
    #
    # example2:
    #   ['FOO', 'BAR', 'BAZ'].each do |key|
    #     conn.exec("INSERT INTO test(col1) VALUES(:1)", key)
    #   end
    #
    # Both example's results are same. But the former will use less resources.
    def []=(key, val)
      if @binds.nil? or @binds[key].nil?
	return nil 
      end
      @binds[key].set(val)
    end

    # Returns the keys of bind variables as array.
    def keys
      if @binds.nil?
	[]
      else
	@binds.keys
      end
    end

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
      __execute()
      case type
      when :select_stmt
        define_columns()
      when :update_stmt, :delete_stmt, :insert_stmt
	row_count
      else
	true
      end
    end # exec

    # Gets the names of select-list as array. Please use this
    # method after exec.
    def get_col_names
      @names
    end # get_col_names

    # Gets fetched data as array. This is available for select
    # statement only.
    #
    # example:
    #   conn = OCI8.new('scott', 'tiger')
    #   cursor = conn.exec('SELECT * FROM emp')
    #   while r = cursor.fetch()
    #     puts r.join(',')
    #   end
    #   cursor.close
    #   conn.logoff
    def fetch
      if iterator?
	while ret = fetch_a_row()
	  yield(ret)
	end
      else
	fetch_a_row()
      end
    end # fetch

    def fetch_hash
      if rs = fetch_a_row()
        ret = {}
        @names.each do |name|
          ret[name] = rs.shift
        end
        ret
      else 
        nil
      end
    end # fetch_hash

    # close the cursor.
    def close
      free_defns()
      free_binds()
      free()
      @names = nil
    end # close

    private

    def bind_or_define(bind_type, key, val, type, length, precision, scale, strict_check)
      if type.nil?
	if val.nil?
	  raise "bind type is not given." if type.nil?
	else
          if val.class == Class
            type = val
            val = nil
          else
            type = val.class
          end
	end
      end

      bindclass = OCI8::BindType::Mapping[type]
      if bindclass.nil?
        raise "unsupported datatype: #{type}"
      end
      if bindclass.respond_to?(:dispatch)
        bindclass = bindclass.dispatch(val, length, precision, scale)
      end
      bindobj = bindclass.new(__connection, val, length, precision, scale)

      case bind_type
      when :bind
        b = __bind(key, bindobj)
      when :define
	b = __defineByPos(key, bindobj)
      end
      b
    end # bind_or_define

    def define_columns
      num_cols = __param_count
      @defns ||= Array.new(num_cols)
      1.upto(num_cols) do |i|
        parm = __paramGet(i)
        @defns[i - 1] = define_a_column(i, parm) if @defns[i - 1].nil?
        @names[i - 1] = parm.name
        parm.free
      end
      num_cols
    end # define_columns

    def define_a_column(i, p)
      datatype = p.data_type
      datasize = p.data_size
      precision = p.precision
      scale = p.scale

      case datatype
      when :varchar2, :char
        # character size may become large on character set conversion.
        # The length of a half-width kana is one in Shift_JIS, two in EUC-JP,
        # three in UTF-8.
        datasize *= 3
      when :long, :long_raw
        # TODO: use OCI_DYNAMIC_FETCH
        datasize = 65535
      end

      bind_or_define(:define, i, nil, datatype, datasize, precision, scale, true)
    end # define_a_column

    def bind_params(*bindvars)
      bindvars.each_with_index do |val, i|
	if val.is_a? Array
	  bind_param(i + 1, val[0], val[1], val[2])
	else
	  bind_param(i + 1, val)
	end
      end
    end # bind_params

    def fetch_a_row
      res = __fetch()
      return nil if res.nil?
      @defns.collect do |r| r.get() end
    end # fetch_a_row

    def free_defns
      unless @defns.nil?
	@defns.each do |b|
	  b.free() unless b.nil?
	end
      end
      @defns = nil
    end # free_defns

    def free_binds
      unless @binds.nil?
	@binds.each_value do |b|
	  b.free()
	end
      end
      @binds = nil
    end # free_binds
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
end
