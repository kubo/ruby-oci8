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

    klass = OCI8::Object::Base.get_class_by_typename(full_name)
    klass = OCI8::Object::Base.get_class_by_typename(name) if klass.nil?
    if klass.nil?
      if schema_name == username
        eval <<EOS
module Object
  class #{name.downcase.gsub(/(^|_)(.)/) { $2.upcase }} < OCI8::Object::Base
    set_typename('#{name}')
  end
end
EOS
        klass = OCI8::Object::Base.get_class_by_typename(name)
      else
        eval <<EOS
module Object
  module #{schema_name.downcase.gsub(/(^|_)(.)/) { $2.upcase }}
    class #{name.downcase.gsub(/(^|_)(.)/) { $2.upcase }} < OCI8::Object::Base
      set_typename('#{full_name}')
    end
  end
end
EOS
        klass = OCI8::Object::Base.get_class_by_typename(full_name)
      end
    end
    OCI8::TDO.new(self, metadata, klass)
  end

  class BindArgumentHelper
    attr_reader :arg_str
    def initialize(*args)
      if args.length == 1 and args[0].is_a? Hash
        @arg_str = args[0].keys.collect do |key| "#{key}=>:#{key}"; end.join(', ')
        @bind_vars = args[0]
      else
        ary = []
        @bind_vars = {}
        args.each_with_index do |obj, idx|
          key = ':' + (idx + 1).to_s
          ary << key
          @bind_vars[key] = obj
        end
        @arg_str = ary.join(', ')
      end
    end

    def exec(con, csr)
      @bind_vars.each do |key, val|
        if val.is_a? OCI8::Object::Base
          tdo = con.get_tdo_by_class(val.class)
          csr.bind_param(key, nil, :named_type_internal, tdo)
          csr[key].attributes = val
        else
          csr.bind_param(key, val)
        end
      end
      csr.exec
      @bind_vars.each do |key, val|
        if val.is_a? OCI8::Object::Base
          val.instance_variable_set(:@attributes, csr[key].attributes)
        end
      end
    end
  end

  module Object
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
        @attributes = {}
        if args[0].is_a? OCI8
          @con = args.shift
        else
          @con = @@default_connection
        end
        return if args.empty?
        raise "no connection is specified." if @con.nil?
        # setup a TDO object.
        tdo = @con.get_tdo_by_class(self.class)
        # call constructor.
        bind_arg_helper = BindArgumentHelper.new(*args)
        sql = <<EOS
BEGIN
  :self := #{tdo.typename}(#{bind_arg_helper.arg_str});
END;
EOS
        csr = @con.parse(sql)
        begin
          csr.bind_param(:self, nil, :named_type_internal, tdo)
          bind_arg_helper.exec(@con, csr)
          @attributes = csr[:self].attributes
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
          bind_arg_helper = BindArgumentHelper.new(*args)
          sql = <<EOS
BEGIN
  #{tdo.typename}.#{method_id}(#{bind_arg_helper.arg_str});
END;
EOS
          csr = con.parse(sql)
          begin
            bind_arg_helper.exec(con, csr)
          ensure
            csr.close
          end
          return nil
        elsif return_type
          # function
          return_type = tdo.class_methods[method_id]
          bind_arg_helper = BindArgumentHelper.new(*args)
          sql = <<EOS
BEGIN
  :rv := #{tdo.typename}.#{method_id}(#{bind_arg_helper.arg_str});
END;
EOS
          csr = con.parse(sql)
          begin
            csr.bind_param(:rv, nil, return_type)
            bind_arg_helper.exec(con, csr)
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
        if @attributes.is_a? Array
          return @attributes if method_id == :to_ary
          super
        end
        # getter func
        if @attributes.has_key?(method_id)
          if args.length != 0
            raise ArgumentError, "wrong number of arguments (#{args.length} for 0)"
          end
          return @attributes[method_id]
        end
        # setter func
        method_name = method_id.to_s
        if method_name[-1] == ?=
          attr = method_name[0...-1].intern
          if @attributes.has_key?(attr)
            if args.length != 1
              raise ArgumentError, "wrong number of arguments (#{args.length} for 1)"
            end
            return @attributes[attr] = args[0]
          end
        end

        super if @con.nil?

        tdo = @con.get_tdo_by_class(self.class)
        return_type = tdo.instance_methods[method_id]
        if return_type == :none
          # procedure
          bind_arg_helper = BindArgumentHelper.new(*args)
          sql = <<EOS
DECLARE
  val #{tdo.typename} := :self;
BEGIN
  val.#{method_id}(#{bind_arg_helper.arg_str});
  :self := val;
END;
EOS
          csr = @con.parse(sql)
          begin
            csr.bind_param(:self, nil, :named_type_internal, tdo)
            csr[:self].attributes = self
            bind_arg_helper.exec(@con, csr)
            @attributes = csr[:self].attributes
          ensure
            csr.close
          end
          return nil
        elsif return_type
          # function
          bind_arg_helper = BindArgumentHelper.new(*args)
          sql = <<EOS
DECLARE
  val #{tdo.typename} := :self;
BEGIN
  :rv := val.#{method_id}(#{bind_arg_helper.arg_str});
  :self := val;
END;
EOS
          csr = @con.parse(sql)
          begin
            csr.bind_param(:self, nil, :named_type_internal, tdo)
            csr.bind_param(:rv, nil, return_type)
            csr[:self].attributes = self
            bind_arg_helper.exec(@con, csr)
            @attributes = csr[:self].attributes
            rv = csr[:rv]
          ensure
            csr.close
          end
          return rv
        end
        # The method is not found.
        super
      end # method_missing
    end # OCI8::Object::Base
  end # OCI8::Object

  class TDO
    # full-qualified object type name.
    #  e.g.
    #   MDSYS.SDO_GEOMETRY
    attr_reader :typename

    # named_type
    attr_reader :ruby_class

    attr_reader :val_size
    attr_reader :ind_size
    attr_reader :alignment

    attr_reader :attributes
    attr_reader :coll_attr
    attr_reader :attr_getters
    attr_reader :attr_setters

    # mapping between class method's ids and their return types.
    # :none means a procedure.
    #    CREATE OR REPLACE TYPE foo AS OBJECT (
    #      STATIC FUNCTION bar RETURN INTEGER,
    #      STATIC PROCEDURE baz,
    #    );
    #  => {:bar => Integer, :baz => :none}

    attr_reader :class_methods
    # mapping between instance method's ids and their return types.
    # :none means a procedure.
    #    CREATE OR REPLACE TYPE foo AS OBJECT (
    #      MEMBER FUNCTION bar RETURN INTEGER,
    #      MEMBER PROCEDURE baz,
    #    );
    #  => {:bar => Integer, :baz => :none}
    attr_reader :instance_methods

    def is_collection?
      @coll_attr ? true : false
    end

    def initialize(con, metadata, klass)
      @ruby_class = klass
      @typename = metadata.schema_name + '.' + metadata.name

      setup(con, metadata)
      con.instance_variable_get(:@id_to_tdo)[metadata.tdo_id] = self
      con.instance_variable_get(:@name_to_tdo)[@typename] = self
      con.instance_variable_get(:@name_to_tdo)[klass.typename] = self
      if metadata.schema_name == con.username
        con.instance_variable_get(:@name_to_tdo)[metadata.name] = self
      end

      case metadata.typecode
      when :named_type
        initialize_named_type(con, metadata)
      when :named_collection
        initialize_named_collection(con, metadata)
      end
    end

    def initialize_named_type(con, metadata)
      @val_size = 0
      @ind_size = 2
      @alignment = 1
      @attributes = metadata.type_attrs.collect do |type_attr|
        attr = Attr.new(con, type_attr, @val_size, @ind_size)
        @val_size, @ind_size = attr.next_offset
        if @alignment < attr.alignment
          @alignment = attr.alignment
        end
        attr
      end
      # fix alignment
      @val_size = (@val_size + @alignment - 1) & ~(@alignment - 1)

      # setup attr_getters and attr_setters
      @attr_getters = {}
      @attr_setters = {}
      @attributes.each do |attr|
        @attr_getters[attr.name] = attr
        @attr_setters[(attr.name.to_s + '=').intern] = attr
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
          con.exec("select result_type_owner, result_type_name from all_method_results where OWNER = :1 and TYPE_NAME = :2 and METHOD_NO = :3", metadata.schema_name, metadata.name, i + 1) do |r|
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
          warn "unsupported return type (#{schema_name}.#{name}.#{type_method.name})" if $VERBOSE
        end
      end
    end
    private :initialize_named_type

    def initialize_named_collection(con, metadata)
      @val_size = SIZE_OF_POINTER
      @ind_size = 2
      @alignment = ALIGNMENT_OF_POINTER
      @coll_attr = Attr.new(con, metadata.collection_element, 0, 0)
    end
    private :initialize_named_collection

    def inspect
      "#<#{self.class}:#@typename>"
    end

    @@result_type_to_bindtype = {
      'FLOAT'                   => Float,
      'INTEGER'                 => Integer,
      'NUMBER'                  => OCINumber,
      'BINARY_FLOAT'            => :bfloat,
      'BINARY_DOUBLE'           => :bdouble,
      'TIMESTAMP'               => :timestamp,
      'TIMESTAMP WITH TZ'       => :timestamp_tz,
      'TIMESTAMP WITH LOCAL TZ' => :timestamp_ltz,
      'INTERVAL YEAR TO MONTH'  => :interval_ym,
      'INTERVAL DAY TO SECOND'  => :interval_ds,
    }

    def self.check_metadata(con, metadata)
      case metadata.typecode
      when :char, :varchar, :varchar2
        [ATTR_STRING,    nil, SIZE_OF_POINTER, 2, ALIGNMENT_OF_POINTER]
      when :raw
        [ATTR_RAW,       nil, SIZE_OF_POINTER, 2, ALIGNMENT_OF_POINTER]
      when :number, :decimal
        [ATTR_OCINUMBER, nil, SIZE_OF_OCINUMBER, 2, ALIGNMENT_OF_OCINUMBER]
      when :integer, :smallint
        [ATTR_INTEGER,   nil, SIZE_OF_OCINUMBER, 2, ALIGNMENT_OF_OCINUMBER]
      when :real, :double, :float
        [ATTR_FLOAT,     nil, SIZE_OF_OCINUMBER, 2, ALIGNMENT_OF_OCINUMBER]
      when :bdouble
        [ATTR_BINARY_DOUBLE, nil, SIZE_OF_DOUBLE, 2, ALIGNMENT_OF_DOUBLE]
      when :bfloat
        [ATTR_BINARY_FLOAT, nil, SIZE_OF_FLOAT, 2, ALIGNMENT_OF_FLOAT]
      when :named_type
        tdo = con.get_tdo_by_metadata(metadata.type_metadata)
        [ATTR_NAMED_TYPE, tdo, tdo.val_size, tdo.ind_size, tdo.alignment]
      when :named_collection
        #datatype, typeinfo, = OCI8::TDO.check_metadata(con, metadata.type_metadata.collection_element)
        #[ATTR_NAMED_COLLECTION, [datatype, typeinfo], SIZE_OF_POINTER, 2, ALIGNMENT_OF_POINTER]
        tdo = con.get_tdo_by_metadata(metadata.type_metadata)
        [ATTR_NAMED_COLLECTION, tdo, tdo.val_size, tdo.ind_size, tdo.alignment]
      else
        raise "unsupported typecode #{metadata.typecode}"
      end
    end

    class Attr
      attr_reader :name
      attr_reader :val_offset
      attr_reader :ind_offset
      attr_reader :alignment
      attr_reader :datatype
      attr_reader :typeinfo
      def initialize(con, metadata, val_offset, ind_offset)
        if metadata.respond_to? :name
          @name = metadata.name.downcase.intern
        end
        @datatype, @typeinfo, @val_size, @ind_size, @alignment, = OCI8::TDO.check_metadata(con, metadata)
        @val_offset = (val_offset + @alignment - 1) & ~(@alignment - 1)
        @ind_offset = ind_offset
      end
      def next_offset
        [@val_offset + @val_size, @ind_offset + @ind_size]
      end
    end
  end

  class NamedType
    def to_value
      obj = tdo.ruby_class.new
      obj.instance_variable_set(:@attributes, self.attributes)
      obj
    end

    def attributes
      attrs = {}
      tdo.attributes.each do |attr|
        attrs[attr.name] = get_attribute(attr.datatype, attr.typeinfo, attr.val_offset, attr.ind_offset)
      end
      attrs
    end

    def attributes=(obj)
      obj = obj.instance_variable_get(:@attributes) unless obj.is_a? Hash
      tdo.attributes.each do |attr|
        set_attribute(attr.datatype, attr.typeinfo, attr.val_offset, attr.ind_offset, obj[attr.name])
      end
    end
  end

  class NamedCollection
    def to_value
      obj = tdo.ruby_class.new
      obj.instance_variable_set(:@attributes, self.attributes)
      obj
    end

    def attributes
      attr = tdo.coll_attr
      get_coll_element(attr.datatype, attr.typeinfo)
    end

    def attributes=(obj)
      attr = tdo.coll_attr
      set_coll_element(attr.datatype, attr.typeinfo, obj.to_ary)
    end
  end
end

class OCI8
  module BindType
    class Object < OCI8::BindType::NamedType
      alias :get_orig get
      def set(val)
        get_orig.attributes = val
        nil
      end
      def get()
        (obj = super()) && obj.to_value
      end
    end
  end
end

OCI8::BindType::Mapping[:named_type] = OCI8::BindType::Object
OCI8::BindType::Mapping[:named_type_internal] = OCI8::BindType::NamedType
