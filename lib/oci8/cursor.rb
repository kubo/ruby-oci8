# oci8.rb -- OCI8::Cursor
#
# Copyright (C) 2002-2012 KUBO Takehiro <kubo@jiubao.org>
#

#
class OCI8

  # The instance of this class corresponds to cursor in the term of
  # Oracle, which corresponds to java.sql.Statement of JDBC and statement
  # handle $sth of Perl/DBI.
  #
  # Don't create the instance by calling 'new' method. Please create it by
  # calling OCI8#exec or OCI8#parse.
  class Cursor

    # @note Don't use this constructor. Use {OCI8#parse} instead.
    #
    # @param [OCI8] conn   connection
    # @param [String] sql  SQL statement
    def initialize(conn, sql = nil)
      @bind_handles = {}
      @define_handles = []
      @column_metadata = []
      @names = nil
      @con = conn
      @max_array_size = nil
      __initialize(conn, sql) # Initialize the internal C structure.
    end

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
      bindobj = make_bind_object(:type => type, :length => length)
      __define(pos, bindobj)
      if old = @define_handles[pos - 1]
        old.send(:free)
      end
      @define_handles[pos - 1] = bindobj
      self
    end

    # Binds variables explicitly.
    # 
    # When key is number, it binds by position, which starts from 1.
    # When key is string, it binds by the name of placeholder.
    #
    # example:
    #   cursor = conn.parse("SELECT * FROM emp WHERE ename = :ename")
    #   cursor.bind_param(1, 'SMITH') # bind by position
    #   # ...or...
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
      bindobj = make_bind_object(param)
      __bind(key, bindobj)
      if old = @bind_handles[key]
        old.send(:free)
      end
      @bind_handles[key] = bindobj
      self
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
      case type
      when :select_stmt
        __execute(0)
        define_columns()
      else
        __execute(1)
        row_count
      end
    end

    # Gets fetched data as array. This is available for select
    # statement only.
    #
    # @example
    #   conn = OCI8.new('scott', 'tiger')
    #   cursor = conn.exec('SELECT * FROM emp')
    #   while r = cursor.fetch()
    #     puts r.join(',')
    #   end
    #   cursor.close
    #   conn.logoff
    #
    # @return [Array]
    def fetch
      if block_given?
        while row = fetch_one_row_as_array
          yield row
        end
        self
      else
        fetch_one_row_as_array
      end
    end

    # Gets fetched data as a Hash. The hash keys are column names.
    # If a block is given, acts as an iterator.
    #
    # @return [Hash] the hash keys are column names and hash values are column values
    def fetch_hash
      if block_given?
        while row = fetch_one_row_as_hash()
          yield row
        end
      else
        fetch_one_row_as_hash
      end
    end

    # Gets the value of the bind variable.
    #
    # When bind variables are explicitly bound by {OCI8::Cursor#bind_param},
    # the subscript +key+ must be same with the parameter +key+ passed to {OCI8::Cursor#bind_param}.
    #
    # When they are implicitly bound by {OCI8#exec} or {OCI8::Cursor#exec},
    # the subscript +key+ is the position which starts from one.
    #
    # @example explicitly bind by name
    #   cursor = conn.parse("BEGIN :out := 'BAR'; END;")
    #   cursor.bind_param(:out, 'FOO') # bind by name
    #   p cursor[:out] # => 'FOO'  - The subscript must be :out.
    #   cursor.exec()
    #   p cursor[:out] # => 'BAR'
    #
    # @example explicitly bind by position
    #   cursor = conn.parse("BEGIN :out := 'BAR'; END;")
    #   cursor.bind_param(1, 'FOO') # bind by position
    #   p cursor[1] # => 'FOO'  - The subscript must be 1.
    #   cursor.exec()
    #   p cursor[1] # => 'BAR'
    #
    # @example implicitly bind
    #   cursor = conn.exec("BEGIN :out := 'BAR'; END;", 'FOO')
    #   # 1st bind variable is bound as String with width 3. Its initial value is 'FOO'
    #   # After execute, the value become 'BAR'.
    #   p cursor[1] # => 'BAR'
    #
    # @param [Object] key bind key
    # @return [Object] the value of the bind variable
    #
    def [](key)
      handle = @bind_handles[key]
      handle && handle.send(:get_data)
    end

    # Changes the bind variable value.
    #
    # When bind variables are explicitly bound by {OCI8::Cursor#bind_param},
    # the subscript +key+ must be same with the parameter +key+ passed to {OCI8::Cursor#bind_param}.
    #
    # When they are implicitly bound by {OCI8#exec} or {OCI8::Cursor#exec},
    # the subscript +key+ is the position which starts from one.
    #
    # @example
    #   # Inserts three rows whose values are 'FOO', 'BAR' and 'BAZ.'
    #   cursor = conn.parse("INSERT INTO test(col1) VALUES(:1)")
    #   begin
    #     cursor.bind_params(1, nil, String, 3)
    #     ['FOO', 'BAR', 'BAZ'].each do |column_value|
    #       cursor[1] = column_value  # Change the bind value
    #       cursor.exec               # and insert it.
    #     end
    #   ensure
    #     cursor.close()
    #   end
    #   # This makes same result with the following but is more efficient.
    #   #
    #   #  ['FOO', 'BAR', 'BAZ'].each do |column_value|
    #   #    conn.exec("INSERT INTO test(col1) VALUES(:1)", column_value)
    #   #  end
    #   #
    #
    # @param [Object] key bind key
    # @param [Object] val bind value
    #
    def []=(key, val)
      handle = @bind_handles[key]
      return nil if handle.nil?

      if val.is_a? Array
        if @actual_array_size > 0 && val.length != @actual_array_size
          raise RuntimeError, "all binding arrays hould be the same size"
        end
        if @actual_array_size == 0 && val.length <= @max_array_size
          @actual_array_size = val.length
        end
      end
      handle.send(:set_data, val)
      val
    end

    # Set the maximum array size for bind_param_array
    #
    # All the binds will be clean from cursor if instance variable max_array_size is set before
    #
    # Instance variable actual_array_size holds the size of the arrays users actually binds through bind_param_array
    #  all the binding arrays are required to be the same size
    def max_array_size=(size)
      raise "expect positive number for max_array_size." if size.nil? && size <=0
      free_bind_handles()  if !@max_array_size.nil?
      @max_array_size = size
      @actual_array_size = nil
    end

    # Binds array explicitly
    #
    # When key is number, it binds by position, which starts from 1.
    # When key is string, it binds by the name of placeholder.
    # 
    # The max_array_size should be set before calling bind_param_array
    #
    # @example
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
      if bindclass.nil? and type.is_a? Class
        bindclass = OCI8::BindType::Mapping[type.to_s]
        OCI8::BindType::Mapping[type] = bindclass if bindclass
      end
      raise "unsupported dataType: #{type}" if bindclass.nil?
      bindobj = bindclass.create(@con, var_array, param, @max_array_size)
      __bind(key, bindobj)
      #
      if old = @bind_handles[key]
        old.send(:free)
      end
      @bind_handles[key] = bindobj
      self
    end

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
    end

    # Gets the names of select-list as array. Please use this
    # method after exec.
    def get_col_names
      @names ||= @column_metadata.collect { |md| md.name }
    end

    # Gets an array of OCI8::Metadata::Column of a select statement.
    #
    # @example
    #   cursor = conn.exec('select * from tab')
    #   puts ' Name                                      Type'
    #   puts ' ----------------------------------------- ----------------------------'
    #   cursor.column_metadata.each do |colinfo|
    #     puts format(' %-41s %s',
    #                 colinfo.name,
    #                 colinfo.type_string)
    #   end
    #
    # @return [Array of OCI8::Metadata::Column]
    #
    # @since 1.0.0
    def column_metadata
      @column_metadata
    end

    # close the cursor.
    def close
      free()
      @names = nil
      @column_metadata = nil
    end

    # Returns the keys of bind variables.
    #
    # @return [Array] bind variable keys
    def keys
      @bind_handles.keys
    end

    # Set the number of rows to be prefetched.
    # This can reduce the number of network round trips when fetching
    # many rows. The default value is one.
    #
    # FYI: Rails oracle adaptor uses 100 by default.
    #
    # @param [Fixnum] rows The number of rows to be prefetched
    def prefetch_rows=(rows)
      attr_set_ub4(11, rows) # OCI_ATTR_PREFETCH_ROWS(11)
    end

    # Returns the number of processed rows.
    #
    # @return [Integer]
    def row_count
      # http://docs.oracle.com/cd/E11882_01/appdev.112/e10646/ociaahan.htm#sthref5498
      attr_get_ub4(9) # OCI_ATTR_ROW_COUNT(9)
    end

    # Returns the text of the SQL statement prepared in the cursor.
    #
    # @note
    #   When {http://docs.oracle.com/cd/E11882_01/server.112/e10729/ch7progrunicode.htm#CACHHIFE
    #   NCHAR String Literal Replacement} is turned on, it returns the modified SQL text,
    #   instead of the original SQL text.
    #
    # @example
    #    cursor = conn.parse("select * from country where country_code = 'ja'")
    #    cursor.statement # => "select * from country where country_code = 'ja'"
    #
    # @return [String] prepared SQL statement
    #
    # @since 2.1.3
    #
    def statement
      # The magic number 144 is OCI_ATTR_STATEMENT.
      # See http://docs.oracle.com/cd/E11882_01/appdev.112/e10646/ociaahan.htm#sthref5503
      attr_get_string(144)
    end

    # gets the type of SQL statement as follows.
    # * OCI8::STMT_SELECT
    # * OCI8::STMT_UPDATE
    # * OCI8::STMT_DELETE
    # * OCI8::STMT_INSERT
    # * OCI8::STMT_CREATE
    # * OCI8::STMT_DROP
    # * OCI8::STMT_ALTER
    # * OCI8::STMT_BEGIN (PL/SQL block which starts with a BEGIN keyword)
    # * OCI8::STMT_DECLARE (PL/SQL block which starts with a DECLARE keyword)
    # * Other Fixnum value undocumented in Oracle manuals.
    #
    # <em>Changes between ruby-oci8 1.0 and 2.0.</em>
    #
    # [ruby-oci8 2.0] OCI8::STMT_* are Symbols. (:select_stmt, :update_stmt, etc.)
    # [ruby-oci8 1.0] OCI8::STMT_* are Fixnums. (1, 2, 3, etc.)
    #
    def type
      # http://docs.oracle.com/cd/E11882_01/appdev.112/e10646/ociaahan.htm#sthref5506
      stmt_type = attr_get_ub2(24) # OCI_ATTR_STMT_TYPE(24)
      case stmt_type
      when 1 # OCI_STMT_SELECT
        :select_stmt
      when 2 # OCI_STMT_UPDATE
        :update_stmt
      when 3 # OCI_STMT_DELETE
        :delete_stmt
      when 4 # OCI_STMT_INSERT
        :insert_stmt
      when 5 # OCI_STMT_CREATE
        :create_stmt
      when 6 # OCI_STMT_DROP
        :drop_stmt
      when 7 # OCI_STMT_ALTER
        :alter_stmt
      when 8 # OCI_STMT_BEGIN
        :begin_stmt
      when 9 # OCI_STMT_DECLARE
        :declare_stmt
      else
        stmt_type
      end
    end

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
        elsif key == :named_type
          param = @con.get_tdo_by_typename(param[:length])
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
      if bindclass.nil? and key.is_a? Class
        bindclass = OCI8::BindType::Mapping[key.to_s]
        OCI8::BindType::Mapping[key] = bindclass if bindclass
      end
      raise "unsupported datatype: #{key}" if bindclass.nil?
      bindclass.create(@con, val, param, max_array_size)
    end

    def define_columns
      # http://docs.oracle.com/cd/E11882_01/appdev.112/e10646/ociaahan.htm#sthref5494
      num_cols = attr_get_ub4(18) # OCI_ATTR_PARAM_COUNT(18)
      1.upto(num_cols) do |i|
        parm = __paramGet(i)
        define_one_column(i, parm) unless @define_handles[i - 1]
        @column_metadata[i - 1] = parm
      end
      num_cols
    end

    def define_one_column(pos, param)
      bindobj = make_bind_object(param)
      __define(pos, bindobj)
      if old = @define_handles[pos - 1]
        old.send(:free)
      end
      @define_handles[pos - 1] = bindobj
    end

    def bind_params(*bindvars)
      bindvars.each_with_index do |val, i|
        if val.is_a? Array
          bind_param(i + 1, val[0], val[1], val[2])
        else
          bind_param(i + 1, val)
        end
      end
    end

    def fetch_one_row_as_array
      if __fetch(@con)
        @define_handles.collect do |handle|
          handle.send(:get_data)
        end
      else
        nil
      end
    end

    def fetch_one_row_as_hash
      if __fetch(@con)
        ret = {}
        get_col_names.each_with_index do |name, idx|
          ret[name] = @define_handles[idx].send(:get_data)
        end
        ret
      else
        nil
      end
    end

    def free_bind_handles
      @bind_handles.each_value do |val|
        val.send(:free)
      end
      @bind_handles.clear
    end
  end
end
