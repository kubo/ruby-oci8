#--
# bindtype.rb -- OCI8::BindType
#
# Copyright (C) 2009-2010 KUBO Takehiro <kubo@jiubao.org>
#++

class OCI8
  module BindType
    Mapping = {}

    class Base
      def self.create(con, val, param, max_array_size)
        self.new(con, val, param, max_array_size)
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

    class BigDecimal < OCI8::BindType::OraNumber
      @@bigdecimal_is_required = false
      def get()
        unless @@bigdecimal_is_required
          require 'bigdecimal'
          @@bigdecimal_is_required = true
        end
        (val = super()) && val.to_d
      end
    end

    class Rational < OCI8::BindType::OraNumber
      @@rational_is_required = false
      def get()
        unless @@rational_is_required
          require 'rational'
          @@rational_is_required = true
        end
        (val = super()) && val.to_r
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
            # use BigDecimal instead
            klass = OCI8::BindType::BigDecimal
          end
        end
        klass.new(con, val, nil, max_array_size)
      end
    end

    class String
      # 1333 <= ceil(4000 / 3). 4000 is max size of char. 3 is NLS ratio of UTF-8.
      @@minimum_bind_length = 1333

      def self.minimum_bind_length
        @@minimum_bind_length
      end

      def self.minimum_bind_length=(val)
        @@minimum_bind_length = val
      end

      def self.create(con, val, param, max_array_size)
        case param
        when Hash
          param[:char_semantics] = true unless param.has_key? :char_semantics
          unless param[:length]
            if val.respond_to? :to_str
              val = val.to_str
              if param[:char_semantics]
                param[:length] = val.size
              else
                if OCI8.respond_to? :encoding and OCI8.encoding != val.encoding
                  # If the string encoding is different with NLS_LANG character set,
                  # convert it to get the length.
                  val = val.encode(OCI8.encoding)
                end
                if val.respond_to? :bytesize
                  # ruby 1.8.7 or upper
                  param[:length] = val.bytesize
                else
                  # ruby 1.8.6 or lower
                  param[:length] = val.size
                end
              end
            else
              param[:length] = @@minimum_bind_length
            end
          end
        when OCI8::Metadata::Base
          case param.data_type
          when :char, :varchar2
            if param.charset_form == :nchar or param.char_used?
              param = {:length => param.char_size, :char_semantics => true}
            else
              param = {:length => param.data_size}
            end
          when :raw
            # HEX needs twice space.
            param = {:length => param.data_size * 2}
          else
            param = {:length => @@minimum_bind_length}
          end
        end
        self.new(con, val, param, max_array_size)
      end
    end

    class RAW
      def self.create(con, val, param, max_array_size)
        case param
        when Hash
          unless param[:length]
            if val.respond_to? :to_str
              val = val.to_str
              if val.respond_to? :bytesize
                param[:length] = val.bytesize
              else
                param[:length] = val.size
              end
            else
              param[:length] = 400
            end
          end
        when OCI8::Metadata::Base
          param = {:length => param.data_size}
        end
        self.new(con, val, param, max_array_size)
      end
    end

    class Long < OCI8::BindType::String
      def self.create(con, val, param, max_array_size)
        param = {:length => con.long_read_len, :char_semantics => true}
        self.new(con, val, param, max_array_size)
      end
    end

    class LongRaw < OCI8::BindType::RAW
      def self.create(con, val, param, max_array_size)
        param = {:length => con.long_read_len, :char_semantics => false}
        self.new(con, val, param, max_array_size)
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
end

# bind or explicitly define
OCI8::BindType::Mapping[String]       = OCI8::BindType::String
OCI8::BindType::Mapping[OraNumber]    = OCI8::BindType::OraNumber
OCI8::BindType::Mapping['BigDecimal'] = OCI8::BindType::BigDecimal
OCI8::BindType::Mapping['Rational']   = OCI8::BindType::Rational
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
OCI8::BindType::Mapping[:date] = OCI8::BindType::Time

if OCI8.oracle_client_version >= OCI8::ORAVER_9_0
  OCI8::BindType::Mapping[:timestamp] = OCI8::BindType::Time
  OCI8::BindType::Mapping[:timestamp_tz] = OCI8::BindType::Time
  OCI8::BindType::Mapping[:timestamp_ltz] = OCI8::BindType::Time
  OCI8::BindType::Mapping[:interval_ym] = OCI8::BindType::IntervalYM
  OCI8::BindType::Mapping[:interval_ds] = OCI8::BindType::IntervalDS
end

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
# The default mapping is Float for ruby-oci8 1.0, OraNumber for 2.0.0 ~ 2.0.2,
# BigDecimal for 2.0.3 ~.
OCI8::BindType::Mapping[:number_unknown_prec] = OCI8::BindType::BigDecimal

# mapping for number without precision and scale.
#
# for example:
#   create table tab1 (col1 number);
#   select col1 from tab1;
#
# note: This is available only on Oracle 9.2.0.3 or above.
# see:  Oracle 9.2.0.x Patch Set Notes.
#
# The default mapping is Float for ruby-oci8 1.0, OraNumber for 2.0.0 ~ 2.0.2,
# BigDecimal for 2.0.3 ~.
OCI8::BindType::Mapping[:number_no_prec_setting] = OCI8::BindType::BigDecimal

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
