#
# DBD::OCI8
#
# Copyright (c) 2002-2005 KUBO Takehiro <kubo@jiubao.org>
#
# copied some code from DBD::Oracle.

require 'oci8'

module DBI # :nodoc:
module DBD # :nodoc:
module OCI8

VERSION          = "0.1"
USED_DBD_VERSION = "0.2"

module Util

  ERROR_MAP = {
    1 => DBI::IntegrityError, # unique constraint violated
    900 => DBI::ProgrammingError, # invalid SQL statement
    904 => DBI::ProgrammingError, # invalid identifier
    905 => DBI::ProgrammingError, # missing keyword
    923 => DBI::ProgrammingError, # FROM keyword not found where expected 
    936 => DBI::ProgrammingError, # missing expression
    942 => DBI::ProgrammingError, # table or view does not exist
    2290 => DBI::IntegrityError, # check constraint violated
    2291 => DBI::IntegrityError, # parent key not found
    2292 => DBI::IntegrityError, # child record found
    2293 => DBI::IntegrityError, # check constraint violated
  }

  def raise_dbierror(err) # :nodoc:
    if err.is_a? OCIError
      exc = ERROR_MAP[err.code] || DBI::DatabaseError
      raise exc.new(err.message, err.code)
    else
      raise DBI::DatabaseError.new(err.message, -1)
    end
  rescue DBI::DatabaseError => exc
    exc.set_backtrace(err.backtrace)
    raise
  end
end

class Driver < DBI::BaseDriver # :nodoc:
  include Util
  @@env = nil

  def initialize
    super(USED_DBD_VERSION)
  end

  def connect( dbname, user, auth, attr )
    handle = ::OCI8.new(user, auth, dbname, attr['Privilege'])
    handle.non_blocking = true if attr['NonBlocking']
    return Database.new(handle, attr)
  rescue OCIException => err
    raise_dbierror(err)
  end
end

class Database < DBI::BaseDatabase
  include Util

  def disconnect
    @handle.logoff
  rescue OCIException => err
    raise_dbierror(err)
  end

  def prepare( statement )
    # convert ?-style parameters to :1, :2 etc.
    prep_statement = DBI::SQL::PreparedStatement.new(DummyQuoter.new, statement)
    if prep_statement.unbound.size > 0
      arr = (1..(prep_statement.unbound.size)).collect{|i| ":#{i}"}
      statement = prep_statement.bind( arr ) 
    end
    cursor = @handle.parse(statement)
    Statement.new(cursor)
  rescue OCIException => err
    raise_dbierror(err)
  end

  def ping
    @handle.exec("BEGIN NULL; END;")
    true
  rescue
    false
  end

  def commit
    @handle.commit()
  rescue OCIException => err
    raise_dbierror(err)
  end

  def rollback
    @handle.rollback()
  rescue OCIException => err
    raise_dbierror(err)
  end

  def tables
    stmt = execute("SELECT object_name FROM user_objects where object_type in ('TABLE', 'VIEW')")
    rows = stmt.fetch_all || []
    stmt.finish
    rows.collect {|row| row[0]} 
  end

  def [](attr)
    case attr
    when 'AutoCommit'
      @handle.autocommit?
    end
  end

  def []=(attr, value)
    case attr
    when 'AutoCommit'
      @handle.autocommit = value
    end
  end

  private

  class DummyQuoter # :nodoc:
    # dummy to substitute ?-style parameter markers by :1 :2 etc.
    def quote(str)
      str
    end
  end
end

class Statement < DBI::BaseStatement
  include Util

  def initialize(cursor)
    @cursor = cursor
  end

  def bind_param( param, value, attribs)
    if attribs.nil? || attribs['type'].nil?
      if value.nil?
	@cursor.bind_param(param, nil, String, 1)
      else
	@cursor.bind_param(param, value)
      end
    else
      case attribs['type']
      when SQL_BINARY
	type = OCI_TYPECODE_RAW
      else
	type = attribs['type']
      end
      @cursor.bind_param(param, value, type)
    end
  rescue OCIException => err
    raise_dbierror(err)
  end

  def execute
    @cursor.exec
  rescue OCIException => err
    raise_dbierror(err)
  end

  def finish
    @cursor.close
  rescue OCIException => err
    raise_dbierror(err)
  end

  def fetch
    @cursor.fetch
  rescue OCIException => err
    raise_dbierror(err)
  end

  def column_info
    # minimum implementation.
    @cursor.get_col_names.collect do |name| {'name' => name} end
  rescue OCIException => err
    raise_dbierror(err)
  end

  def rows
    @cursor.row_count
  rescue OCIException => err
    raise_dbierror(err)
  end

  def __rowid
    @cursor.rowid
  end

  def __define(pos, type, length = nil)
    @cursor.define(pos, type, length)
    self
  end

  def __bind_value(param)
    @cursor[param]
  end
end

module BindType
  DBIDate = Object.new
  class << DBIDate
    def fix_type(env, val, length, precision, scale)
      # bind as an OraDate
      [::OCI8::SQLT_DAT, val, nil]
    end
    def decorate(b)
      def b.set(val)
        # convert val to an OraDate,
        # then set it to the bind handle.
        super(val && OraDate.new(val.year, val.month, val.day))
      end
      def b.get()
        # get an Oradate from the bind handle,
        # then convert it to a DBI::Date.
        (val = super()) && DBI::Date.new(val.year, val.month, val.day)
      end
    end
  end

  DBITimestamp = Object.new
  class << DBITimestamp
    def fix_type(env, val, length, precision, scale)
      # bind as an OraDate
      [::OCI8::SQLT_DAT, val, nil]
    end
    def decorate(b)
      def b.set(val)
        # convert val to an OraDate,
        # then set it to the bind handle.
        super(val && OraDate.new(val.year, val.month, val.day, val.hour, val.minute, val.second))
      end
      def b.get()
        # get an Oradate from the bind handle,
        # then convert it to a DBI::Timestamp.
        (val = super()) && DBI::Timestamp.new(val.year, val.month, val.day, val.hour, val.minute, val.second)
      end
    end
  end

  DBIStatementHandle = Object.new
  class << DBIStatementHandle
    def fix_type(env, val, length, precision, scale)
      raise NotImplementedError unless val.nil?
      [::OCI8::SQLT_RSET, nil, env.alloc(OCIStmt)]
    end
    def decorate(b)
      def b.set(val)
      raise NotImplementedError
      end
      def b.get()
        val = super
        return val if val.nil?
        cur = ::OCI8::Cursor.new(@env, @svc, @ctx, val)
        stmt = DBI::DBD::OCI8::Statement.new(cur)
        DBI::StatementHandle.new(stmt, true, false)
      end
    end
  end
end # BindType

::OCI8::BindType::Mapping[DBI::Date] = BindType::DBIDate
::OCI8::BindType::Mapping[DBI::Timestamp] = BindType::DBITimestamp
::OCI8::BindType::Mapping[DBI::StatementHandle] = BindType::DBIStatementHandle

end # module OCI8
end # module DBD
end # module DBI
