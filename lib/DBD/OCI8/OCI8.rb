#
# DBD::OCI8
#
# Copyright (c) 2002-2007 KUBO Takehiro <kubo@jiubao.org>
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

  def initialize
    super(USED_DBD_VERSION)
  end

  # external OS authentication
  # (contributed by Dan Fitch)
  def default_user
    [nil, nil]
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

if defined? ::OCI8::BindType::Base
  ##
  ## ruby-oci8 0.2 bind classes.
  ##

  module BindType # :nodoc:

    # helper class to define/bind DBI::Date.
    class DBIDate < ::OCI8::BindType::OraDate
      def set(val)
        # convert val to an OraDate,
        # then set it to the bind handle.
        super(val && OraDate.new(val.year, val.month, val.day))
      end
      def get()
        # get an Oradate from the bind handle,
        # then convert it to a DBI::Date.
        val = super()
        return nil if val.nil?
        DBI::Date.new(val.year, val.month, val.day)
      end
    end

    # helper class to define/bind DBI::Timestamp.
    #
    # To fetch all Oracle's DATE columns as DBI::Timestamp:
    #    ::OCI8::BindType::Mapping[OCI8::SQLT_DAT] = ::DBI::DBD::OCI8::BindType::DBITimestamp
    #
    class DBITimestamp < ::OCI8::BindType::OraDate
      def set(val)
        # convert val to an OraDate,
        # then set it to the bind handle.
        super(val && OraDate.new(val.year, val.month, val.day,
                                 val.respond_to?(:hour) ? val.hour : 0,
                                 val.respond_to?(:min) ? val.min : 0,
                                 val.respond_to?(:sec) ? val.sec : 0))
      end
      def get()
        # get an Oradate from the bind handle,
        # then convert it to a DBI::Timestamp.
        val = super()
        return nil if val.nil?
        DBI::Timestamp.new(val.year, val.month, val.day, val.hour, val.minute, val.second)
      end
    end

    # helper class to bind ref cursor as DBI::StatementHandle.
    #
    #   # Create package
    #   dbh.execute(<<EOS)
    #   create or replace package test_pkg is
    #     type ref_cursor is ref cursor;
    #     procedure tab_table(csr out ref_cursor);
    #   end;
    #   EOS
    #
    #   # Create package body
    #   dbh.execute(<<EOS)
    #   create or replace package body test_pkg is
    #     procedure tab_table(csr out ref_cursor) is
    #     begin
    #       open csr for select * from tab;
    #     end;
    #   end;
    #   EOS
    #
    #   # Execute test_pkg.tab_table.
    #   # The first parameter is bound as DBI::StatementHandle.
    #   plsql = dbh.execute("begin test_pkg.tab_table(?); end;", DBI::StatementHandle)
    #
    #   # Get the first parameter, which is a DBI::StatementHandle.
    #   sth = plsql.func(:bind_value, 1)
    #
    #   # fetch column data.
    #   sth.fetch_all
    #
    class DBIStatementHandle < ::OCI8::BindType::Cursor
      def set(val)
        if val.is_a? DBI::StatementHandle
          # get OCI8::Cursor
          val = val.instance_eval do @handle end
          val = val.instance_eval do @cursor end
        end
        super(val)
      end
      def get()
        val = super
        return nil if val.nil?
        stmt = DBI::DBD::OCI8::Statement.new(val)
        DBI::StatementHandle.new(stmt, true, false)
      end
    end
  end # BindType

else
  ##
  ## ruby-oci8 0.1 bind classes.
  ##

  module BindType # :nodoc:
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
          super(val && OraDate.new(val.year, val.month, val.day,
                                   val.respond_to?(:hour) ? val.hour : 0,
                                   val.respond_to?(:min) ? val.min : 0,
                                   val.respond_to?(:sec) ? val.sec : 0))
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
end

::OCI8::BindType::Mapping[DBI::Date] = BindType::DBIDate
::OCI8::BindType::Mapping[DBI::Timestamp] = BindType::DBITimestamp
::OCI8::BindType::Mapping[DBI::StatementHandle] = BindType::DBIStatementHandle

end # module OCI8
end # module DBD
end # module DBI
