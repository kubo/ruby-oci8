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
      if bindclass.nil? and type.is_a? Class
        bindclass = OCI8::BindType::Mapping[type.to_s]
        OCI8::BindType::Mapping[type] = bindclass if bindclass
      end
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
    # @return [String]
    def statement
      # The magic number 144 is OCI_ATTR_STATEMENT.
      # See http://docs.oracle.com/cd/E11882_01/appdev.112/e10646/ociaahan.htm#sthref5503
      attr_get_string(144)
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

  end
end
