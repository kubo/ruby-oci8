#
# OCI8::NamedType
#
require 'oci8/metadata.rb'

class OCI8

  def get_tdo_by_class(klass)
    @id_to_tdo ||= {}
    @name_to_tdo ||= {}
    tdo = @name_to_tdo[klass.typename]
    return tdo if tdo

    metadata = describe_any(klass.typename)
    if metadata.is_a? OCI8::Metadata::Synonym
      metadata = describe_any(metadata.translated_name)
    end
    unless metadata.is_a? OCI8::Metadata::Type
      raise "unknown typename #{klass.typename}"
    end
    OCI8::TDO.new(self, metadata, klass)
  end

  def get_tdo_by_metadata(metadata)
    @id_to_tdo ||= {}
    @name_to_tdo ||= {}
    tdo = @id_to_tdo[metadata.tdo_id]
    return tdo if tdo

    schema_name = metadata.schema_name
    name = metadata.name
    full_name = schema_name + '.' + name

    klass = OCI8::NamedType::Base.get_class_by_typename(full_name)
    klass = OCI8::NamedType::Base.get_class_by_typename(name) if klass.nil?
    if klass.nil?
      if schema_name == username
        eval <<EOS
module NamedType
  class #{name.downcase.gsub(/(^|_)(.)/) { $2.upcase }} < OCI8::NamedType::Base
  end
end
EOS
        klass = OCI8::NamedType::Base.get_class_by_typename(name)
      else
        eval <<EOS
module NamedType
  module #{schema_name.downcase.gsub(/(^|_)(.)/) { $2.upcase }}
    class #{name.downcase.gsub(/(^|_)(.)/) { $2.upcase }} < OCI8::NamedType::Base
      set_typename('#{full_name}')
    end
  end
end
EOS
        klass = OCI8::NamedType::Base.get_class_by_typename(full_name)
      end
    end
    OCI8::TDO.new(self, metadata, klass)
  end

  def username
    @username || begin
      exec('select user from dual') do |row|
        @username = row[0]
      end
      @username
    end
  end

  module NamedType
    class Base
      @@class_to_name = {}
      @@name_to_class = {}
      @@default_connection = nil

      def self.inherited(klass)
        name = klass.to_s.gsub(/^.*::/, '').gsub(/([a-z\d])([A-Z])/,'\1_\2').upcase
        @@class_to_name[klass] = name
        @@name_to_class[name] = klass
      end

      def self.get_class_by_typename(name)
        @@name_to_class[name]
      end

      def self.typename
        @@class_to_name[self]
      end

      def self.set_typename(name)
        # delete an old name-to-class mapping.
        @@name_to_class[@@class_to_name[self]] = nil
        # set a new name-to-class mapping.
        name = name.upcase
        @@class_to_name[self] = name
        @@name_to_class[name] = self
      end

      def self.default_connection=(con)
        @@default_connection = con
      end

      def initialize(*args)
        if args[0].is_a? OCI8
          @con = args.shift
        else
          @con = @@default_connection
        end
        raise "no connection is specified." if @con.nil?
        # setup a TDO object.
        @tdo = @con.get_tdo_by_class(self.class)
        fini_funcs = @tdo.attrs.collect do |attr|
          if attr.fini_func
            [attr.fini_func, attr.val_offset, attr.ind_offset]
          end
        end.compact
        initialize_internal(@tdo.val_size, @tdo.ind_size, fini_funcs)
        # initialize attributes.
        @tdo.attrs.each do |attr|
          if attr.init_func
            send attr.init_func, attr.val_offset, attr.ind_offset
          end
        end
        return if args.empty?
        # call constructor.
        arg_str, bind_vars = self.class.make_sql_argument(*args)
        sql = <<EOS
BEGIN
  :self := #{@tdo.typename}(#{arg_str});
END;
EOS
        csr = @con.parse(sql)
        begin
          csr.bind_param(:self, self)
          bind_vars.each do |key, val|
            csr.bind_param(key, val)
          end
          csr.exec
        ensure
          csr.close
        end
      end

      # class method
      def self.method_missing(method_id, *args)
        if args[0].is_a? OCI8
          con = args.shift
        else
          con = @@default_connection
        end
        tdo = con.get_tdo_by_class(self)
        return_type = tdo.class_methods[method_id]
        if return_type == :none
          # procedure
          arg_str, bind_vars = make_sql_argument(*args)
          sql = <<EOS
BEGIN
  #{tdo.typename}.#{method_id}(#{arg_str});
END;
EOS
          csr = con.parse(sql)
          begin
            bind_vars.each do |key, val|
              csr.bind_param(key, val)
            end
            csr.exec
          ensure
            csr.close
          end
          return nil
        elsif return_type
          # function
          arg_str, bind_vars = make_sql_argument(*args)
          sql = <<EOS
BEGIN
  :rv := #{tdo.typename}.#{method_id}(#{arg_str});
END;
EOS
          csr = con.parse(sql)
          begin
            csr.bind_param(:rv, nil, return_type)
            bind_vars.each do |key, val|
              csr.bind_param(key, val)
            end
            csr.exec
            rv = csr[:rv]
          ensure
            csr.close
          end
          return rv
        end
        super # The method is not found.
      end

      # instance method
      def method_missing(method_id, *args)
        return_type = @tdo.instance_methods[method_id]
        if return_type == :none
          # procedure
          arg_str, bind_vars = self.class.make_sql_argument(*args)
          sql = <<EOS
DECLARE
  val #{@tdo.typename} := :self;
BEGIN
  val.#{method_id}(#{arg_str});
  :self := val;
END;
EOS
          csr = @con.parse(sql)
          begin
            csr.bind_param(:self, self)
            bind_vars.each do |key, val|
              csr.bind_param(key, val)
            end
            csr.exec
          ensure
            csr.close
          end
          return nil
        elsif return_type
          # function
          arg_str, bind_vars = self.class.make_sql_argument(*args)
          sql = <<EOS
DECLARE
  val #{@tdo.typename} := :self;
BEGIN
  :rv := val.#{method_id}(#{arg_str});
  :self := val;
END;
EOS
          csr = @con.parse(sql)
          begin
            csr.bind_param(:self, self)
            csr.bind_param(:rv, nil, return_type)
            bind_vars.each do |key, val|
              csr.bind_param(key, val)
            end
            csr.exec
            rv = csr[:rv]
          ensure
            csr.close
          end
          return rv
        end
        attr = @tdo.attr_methods[method_id]
        if attr
          # attribute
          return send(attr[0], attr[1], attr[2], *args)
        end
        super # The method is not found.
      end # method_missing

      def self.make_sql_argument(*args)
        if args.length == 1 and args[0].is_a? Hash
          [args[0].keys.collect do |key| "#{key}=>:#{key}"; end.join(', '), args[0]]
        else
          str_ary = []
          bind_vars = {}
          args.each_with_index do |obj, idx|
            key = ':' + (idx + 1).to_s
            str_ary << key
            bind_vars[key] = obj
          end
          [str_ary.join(', '), bind_vars]
        end
      end

      def inspect
        return super if @tdo.nil?
        args = []
        if is_null?
          args << 'null'
        else
          @tdo.attr_methods.each do |key, val|
            next if key.to_s[-1] == ?=
            val = send(*val)
            if val
              args << "#{key}=>#{val}"
            else
              args << "#{key}=>null"
            end
          end
        end
        "<##{self.class}: #{args.join(', ')}>"
      end
    end # OCI8::NamedType::Base
  end # OCI8::NamedType

  class TDO
    attr_reader :typename
    attr_reader :val_size
    attr_reader :ind_size
    attr_reader :alignment
    attr_reader :attrs
    attr_reader :attr_methods
    attr_reader :class_methods
    attr_reader :instance_methods

    def initialize(con, metadata, klass)
      setup(con, metadata)
      con.instance_variable_get(:@id_to_tdo)[metadata.tdo_id] = self
      con.instance_variable_get(:@name_to_tdo)[metadata.schema_name + '.' + metadata.name] = self
      if metadata.schema_name == con.username
        con.instance_variable_get(:@name_to_tdo)[metadata.name] = self
      end

      @con = con
      @ruby_class = klass
      @typename = metadata.schema_name + '.' + metadata.name

      @val_size = 0
      @ind_size = 2
      @alignment = 1
      @attrs = metadata.type_attrs.collect do |type_attr|
        attr = Attr.new(con, type_attr, @val_size, @ind_size)
        @val_size, @ind_size = attr.next_offset
        if @alignment < attr.alignment
          @alignment = attr.alignment
        end
        attr
      end
      # fix alignment
      @val_size = (@val_size + @alignment - 1) & ~(@alignment - 1)

      # set attr_methods
      @attr_methods = {}
      @attrs.each do |attr|
        @attr_methods[attr.name.intern] = [attr.get_func, attr.val_offset, attr.ind_offset]
        @attr_methods[(attr.name + '=').intern] = [attr.set_func, attr.val_offset, attr.ind_offset]
      end

      # set class_methods and instance_methods
      @class_methods = {}
      @instance_methods = {}
      metadata.type_methods.each_with_index do |type_method, i|
        next if type_method.is_constructor? or type_method.is_destructor?
        args = type_method.arguments

        result_type = nil
        if type_method.has_result?
          # function
          @con.exec("select result_type_owner, result_type_name from all_method_results where OWNER = :1 and TYPE_NAME = :2 and METHOD_NO = :3", metadata.schema_name, metadata.name, i + 1) do |r|
            if r[0].nil?
              result_type = @@result_type_to_bindtype[r[1]]
            else
              result_type = con.get_tdo_by_metadata(con.describe_type("#{r[0]}.#{r[1]}"))
            end
          end
        else
          # procedure
          result_type = :none
        end
        if result_type
          if type_method.is_selfish?
            @instance_methods[type_method.name.downcase.intern] = result_type
          else
            @class_methods[type_method.name.downcase.intern] = result_type
          end
        else
          puts "unsupported return type (#{schema_name}.#{name}.#{type_method.name})" if $VERBOSE
        end
      end
    end

    def inspect
      "<##{self.class}:#@typename>"
    end

    @@result_type_to_bindtype = {
      'FLOAT'                   => Float,
      'NUMBER'                  => OCINumber,
      'BINARY_FLOAT'            => :bfloat,
      'BINARY_DOUBLE'           => :bdouble,
      'TIMESTAMP'               => :timestamp,
      'TIMESTAMP WITH TZ'       => :timestamp_tz,
      'TIMESTAMP WITH LOCAL TZ' => :timestamp_ltz,
      'INTERVAL YEAR TO MONTH'  => :interval_ym,
      'INTERVAL DAY TO SECOND'  => :interval_ds,
    }

    class Attr
      attr_reader :name
      attr_reader :val_offset
      attr_reader :ind_offset
      attr_reader :alignment
      attr_reader :init_func
      attr_reader :fini_func
      attr_reader :get_func
      attr_reader :set_func
      attr_reader :tdo
      def initialize(con, metadata, val_offset, ind_offset)
        @name = metadata.name.downcase
        case metadata.typecode
        when :char, :varchar, :varchar2
          @val_size = SIZE_OF_POINTER
          @ind_size = 2
          @alignment = ALIGNMENT_OF_POINTER
          @init_func = nil
          @fini_func = :__fini_string
          @get_func = :__get_string
          @set_func = :__set_string
        when :raw
          @val_size = SIZE_OF_POINTER
          @ind_size = 2
          @alignment = ALIGNMENT_OF_POINTER
          @init_func = nil
          @fini_func = :__fini_raw
          @get_func = :__get_raw
          @set_func = :__set_raw
        when :number, :decimal
          @val_size = SIZE_OF_OCINUMBER
          @ind_size = 2
          @alignment = ALIGNMENT_OF_OCINUMBER
          @init_func = nil
          @fini_func = nil
          @get_func = :__get_number
          @set_func = :__set_number
        when :integer, :smallint
          @val_size = SIZE_OF_OCINUMBER
          @ind_size = 2
          @alignment = ALIGNMENT_OF_OCINUMBER
          @init_func = nil
          @fini_func = nil
          @get_func = :__get_integer
          @set_func = :__set_number
        when :real, :double, :float
          @val_size = SIZE_OF_OCINUMBER
          @ind_size = 2
          @alignment = ALIGNMENT_OF_OCINUMBER
          @init_func = nil
          @fini_func = nil
          @get_func = :__get_float
          @set_func = :__set_number
        else
          raise "unsupported typecode #{metadata.typecode}"
        end
        @val_offset = (val_offset + @alignment - 1) & ~(@alignment - 1)
        @ind_offset = ind_offset
      end
      def next_offset
        [@val_offset + @val_size, @ind_offset + @ind_size]
      end
    end
  end
end
