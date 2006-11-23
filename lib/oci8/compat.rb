# 
# add compatible code with old versions.
# 

OCI_STMT_SELECT = :select_stmt
OCI_STMT_UPDATE = :update_stmt
OCI_STMT_DELETE = :delete_stmt
OCI_STMT_INSERT = :insert_stmt
OCI_STMT_CREATE = :create_stmt
OCI_STMT_DROP = :drop_stmt
OCI_STMT_ALTER = :alter_stmt
OCI_STMT_BEGIN = :begin_stmt
OCI_STMT_DECLARE = :declare_stmt

class OCI8

  STMT_SELECT = :select_stmt
  STMT_UPDATE = :update_stmt
  STMT_DELETE = :delete_stmt
  STMT_INSERT = :insert_stmt
  STMT_CREATE = :create_stmt
  STMT_DROP = :drop_stmt
  STMT_ALTER = :alter_stmt
  STMT_BEGIN = :begin_stmt
  STMT_DECLARE = :declare_stmt

  RAW = :raw

  # varchar, varchar2
  SQLT_CHR = :varchar2
  # number, double precision, float, real, numeric, int, integer, smallint
  SQLT_NUM = :number
  # long
  SQLT_LNG = :long
  # date
  SQLT_DAT = :date
  # raw
  SQLT_BIN = :raw
  # long raw
  SQLT_LBI = :long_raw
  # char
  SQLT_AFC = :char
  # rowid
  SQLT_RDD = :rowid
  # clob
  SQLT_CLOB = :clob
  # blob
  SQLT_BLOB = :blob
  # ref cursor
  SQLT_RSET = 116
  # timestamp
  SQLT_TIMESTAMP = :timestamp
  # timestamp with time zone
  SQLT_TIMESTAMP_TZ = :timestamp_tz
  # interval year to month
  SQLT_INTERVAL_YM = :interval_ym
  # interval day to second
  SQLT_INTERVAL_DS = :interval_ds
  # timestamp with local time zone
  SQLT_TIMESTAMP_LTZ = :timestamp_ltz

  # mapping of sql type number to sql type name.
  SQLT_NAMES = {}
  constants.each do |name|
    next if name.index("SQLT_") != 0
    val = const_get name.intern
    if val.is_a? Fixnum
      SQLT_NAMES[val] = name
    end
  end

  # add alias compatible with 'Oracle7 Module for Ruby'.
  alias autocommit autocommit?

  class Cursor
    def self.select_number_as=(val)
      if val == Fixnum
        @@bind_unknown_number = OCI8::BindType::Fixnum
      elsif val == Integer
        @@bind_unknown_number = OCI8::BindType::Integer
      elsif val == Float
        @@bind_unknown_number = OCI8::BindType::Float
      else
        raise ArgumentError, "must be Fixnum, Integer or Float"
      end
    end

    def self.select_number_as
      case @@bind_unknown_number
      when OCI8::BindType::Fixnum
        return Fixnum
      when OCI8::BindType::Integer
        return Integer
      when OCI8::BindType::Float
        return Float
      end
    end

    # add alias compatible with 'Oracle7 Module for Ruby'.
    alias getColNames get_col_names
  end
end

class OraNumber < OCINumber
  def yaml_initialize(type, val) # :nodoc:
    initialize(val)
  end

  def to_yaml(opts = {}) # :nodoc:
    YAML.quick_emit(object_id, opts) do |out|
      out.scalar(taguri, self.to_s, :plain)
    end
  end
end

class OCI8
  module BindType
    OraNumber = OCINumber
  end
end

OCI8::BindType::Mapping[OraNumber] = OCI8::BindType::OraNumber
