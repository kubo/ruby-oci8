#
# DBD::OCI8
#
# Copyright (c) 2002 KUBO Takehiro <kubo@jiubao.org>
#
# copied some code from DBD::Oracle.

require 'oci8'

module DBI
module DBD
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

  def raise_dbierror(err)
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

class Driver < DBI::BaseDriver
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

  class DummyQuoter
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
end

end # module OCI8
end # module DBD
end # module DBI
