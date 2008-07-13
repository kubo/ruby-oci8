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

  module BindType
    Mapping = {}

    class Base
      def self.create(con, val, param, max_array_size)
        self.new(con, val, param, max_array_size)
      end
    end

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

    # get/set Number (for OCI8::SQLT_NUM)
    class Number
      def self.create(con, val, param, max_array_size)
        if param.is_a? OCI8::Metadata::Base
          precision = param.precision
          scale = param.scale
        end
        if scale == -127
          if precision == 0
            # NUMBER declared without its scale and precision. (Oracle 9.2.0.3 or above)
            klass = OCI8::BindType::Mapping[:number_no_prec_setting]
          else
            # FLOAT or FLOAT(p)
            klass = OCI8::BindType::Float
          end
        elsif scale == 0
          if precision == 0
            # NUMBER whose scale and precision is unknown
            # or
            # NUMBER declared without its scale and precision. (Oracle 9.2.0.2 or below)
            klass = OCI8::BindType::Mapping[:number_unknown_prec]
          else
            # NUMBER(p, 0)
            klass = OCI8::BindType::Integer
          end
        else
          # NUMBER(p, s)
          if precision < 15 # the precision of double.
            klass = OCI8::BindType::Float
          else
            # use BigDecimal instead?
            klass = OCI8::BindType::OraNumber
          end
        end
        klass.new(con, val, nil, max_array_size)
      end
    end

    class String
      def self.create(con, val, param, max_array_size)
        case param
        when Hash
          length = param[:length] || 4000
        when OCI8::Metadata::Base
          case param.data_type
          when :char, :varchar2
            length = param.data_size
            # character size may become large on character set conversion.
            # The length of a Japanese half-width kana is one in Shift_JIS,
            # two in EUC-JP, three in UTF-8.
            length *= 3 unless param.char_used?
          when :raw
            # HEX needs twice space.
            length = param.data_size * 2
          else
            length = 100
          end
        end
        self.new(con, val, length, max_array_size)
      end
    end

    class RAW
      def self.create(con, val, param, max_array_size)
        case param
        when Hash
          length = param[:length] || 4000
        when OCI8::Metadata::Base
          length = param.data_size
        end
        self.new(con, val, length, max_array_size)
      end
    end

    class Long
      def self.create(con, val, param, max_array_size)
        self.new(con, val, con.long_read_len, max_array_size)
      end
    end

    class LongRaw
      def self.create(con, val, param, max_array_size)
        self.new(con, val, con.long_read_len, max_array_size)
      end
    end

    class CLOB
      def self.create(con, val, param, max_array_size)
        if param.is_a? OCI8::Metadata::Base and param.charset_form == :nchar
          OCI8::BindType::NCLOB.new(con, val, nil, max_array_size)
        else
          OCI8::BindType::CLOB.new(con, val, nil, max_array_size)
        end
      end
    end
  end # BindType

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
        define_a_column(i, parm) unless __defined?(i)
        @column_metadata[i - 1] = parm
      end
      num_cols
    end # define_columns

    def define_a_column(pos, param)
      __define(pos, make_bind_object(param))
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

# bind or explicitly define
OCI8::BindType::Mapping[String]       = OCI8::BindType::String
OCI8::BindType::Mapping[OraNumber]    = OCI8::BindType::OraNumber
OCI8::BindType::Mapping[Fixnum]       = OCI8::BindType::Integer
OCI8::BindType::Mapping[Float]        = OCI8::BindType::Float
OCI8::BindType::Mapping[Integer]      = OCI8::BindType::Integer
OCI8::BindType::Mapping[Bignum]       = OCI8::BindType::Integer
OCI8::BindType::Mapping[OraDate]      = OCI8::BindType::OraDate
OCI8::BindType::Mapping[Time]         = OCI8::BindType::Time
OCI8::BindType::Mapping[Date]         = OCI8::BindType::Date
OCI8::BindType::Mapping[DateTime]     = OCI8::BindType::DateTime
OCI8::BindType::Mapping[OCI8::CLOB]   = OCI8::BindType::CLOB
OCI8::BindType::Mapping[OCI8::NCLOB]  = OCI8::BindType::NCLOB
OCI8::BindType::Mapping[OCI8::BLOB]   = OCI8::BindType::BLOB
OCI8::BindType::Mapping[OCI8::BFILE]  = OCI8::BindType::BFILE
OCI8::BindType::Mapping[OCI8::Cursor] = OCI8::BindType::Cursor

# implicitly define

# datatype        type     size prec scale
# -------------------------------------------------
# CHAR(1)       SQLT_AFC      1    0    0
# CHAR(10)      SQLT_AFC     10    0    0
OCI8::BindType::Mapping[:char] = OCI8::BindType::String

# datatype        type     size prec scale
# -------------------------------------------------
# VARCHAR(1)    SQLT_CHR      1    0    0
# VARCHAR(10)   SQLT_CHR     10    0    0
# VARCHAR2(1)   SQLT_CHR      1    0    0
# VARCHAR2(10)  SQLT_CHR     10    0    0
OCI8::BindType::Mapping[:varchar2] = OCI8::BindType::String

# datatype        type     size prec scale
# -------------------------------------------------
# RAW(1)        SQLT_BIN      1    0    0
# RAW(10)       SQLT_BIN     10    0    0
OCI8::BindType::Mapping[:raw] = OCI8::BindType::RAW

# datatype        type     size prec scale
# -------------------------------------------------
# LONG          SQLT_LNG      0    0    0
OCI8::BindType::Mapping[:long] = OCI8::BindType::Long

# datatype        type     size prec scale
# -------------------------------------------------
# LONG RAW      SQLT_LBI      0    0    0
OCI8::BindType::Mapping[:long_raw] = OCI8::BindType::LongRaw

# datatype        type     size prec scale
# -------------------------------------------------
# CLOB          SQLT_CLOB  4000    0    0
OCI8::BindType::Mapping[:clob] = OCI8::BindType::CLOB
OCI8::BindType::Mapping[:nclob] = OCI8::BindType::NCLOB

# datatype        type     size prec scale
# -------------------------------------------------
# BLOB          SQLT_BLOB  4000    0    0
OCI8::BindType::Mapping[:blob] = OCI8::BindType::BLOB

# datatype        type     size prec scale
# -------------------------------------------------
# BFILE         SQLT_BFILE 4000    0    0
OCI8::BindType::Mapping[:bfile] = OCI8::BindType::BFILE

# datatype        type     size prec scale
# -------------------------------------------------
# DATE          SQLT_DAT      7    0    0
OCI8::BindType::Mapping[:date] = OCI8::BindType::DateTime

OCI8::BindType::Mapping[:timestamp] = OCI8::BindType::DateTime
OCI8::BindType::Mapping[:timestamp_tz] = OCI8::BindType::DateTime
OCI8::BindType::Mapping[:timestamp_ltz] = OCI8::BindType::DateTime
OCI8::BindType::Mapping[:interval_ym] = OCI8::BindType::IntervalYM
OCI8::BindType::Mapping[:interval_ds] = OCI8::BindType::IntervalDS

# datatype        type     size prec scale
# -------------------------------------------------
# ROWID         SQLT_RDD      4    0    0
OCI8::BindType::Mapping[:rowid] = OCI8::BindType::String

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
OCI8::BindType::Mapping[:number] = OCI8::BindType::Number

# mapping for calculated number values.
#
# for example:
#   select col1 * 1.1 from tab1;
#
# For Oracle 9.2.0.2 or below, this is also used for NUMBER
# datatypes that have no explicit setting of their precision
# and scale.
#
# The default mapping is Float for ruby-oci8 1.0. It is OraNumber
# for ruby-oci8 2.0.
OCI8::BindType::Mapping[:number_unknown_prec] = OCI8::BindType::OraNumber

# mapping for number without precision and scale.
#
# for example:
#   create table tab1 (col1 number);
#   select col1 from tab1;
#
# note: This is available only on Oracle 9.2.0.3 or above.
# see:  Oracle 9.2.0.x Patch Set Notes.
#
# The default mapping is Float for ruby-oci8 1.0. It is OraNumber
# for ruby-oci8 2.0.
OCI8::BindType::Mapping[:number_no_prec_setting] = OCI8::BindType::OraNumber

if defined? OCI8::BindType::BinaryDouble
  OCI8::BindType::Mapping[:binary_float] = OCI8::BindType::BinaryDouble
  OCI8::BindType::Mapping[:binary_double] = OCI8::BindType::BinaryDouble
else
  OCI8::BindType::Mapping[:binary_float] = OCI8::BindType::Float
  OCI8::BindType::Mapping[:binary_double] = OCI8::BindType::Float
end

# Cursor
OCI8::BindType::Mapping[:cursor] = OCI8::BindType::Cursor

# XMLType (This mapping will be changed before release.)
OCI8::BindType::Mapping[:xmltype] = OCI8::BindType::Long
