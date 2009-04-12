#
# DBD::OCI8
#
# Copyright (c) 2002-2007 KUBO Takehiro <kubo@jiubao.org>
#
# copied some code from DBD::Oracle.
# DBD::Oracle's copyright is as follows:
# --------------------- begin -------------------
#
# Copyright (c) 2001, 2002, 2003, 2004 Michael Neumann <mneumann@ntecs.de>
# 
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions 
# are met:
# 1. Redistributions of source code must retain the above copyright 
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright 
#    notice, this list of conditions and the following disclaimer in the 
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
# THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# --------------------- end -------------------

require 'oci8'

module DBI # :nodoc:
module DBD # :nodoc:
module OCI8

VERSION          = "0.1"
USED_DBD_VERSION = "0.4"

def self.driver_name
  "OCI8"
end

# type converstion handler to bind values. (ruby-dbi 0.4)
if DBI.const_defined?(:TypeUtil)
  DBI::TypeUtil.register_conversion("OCI8") do |obj|
    case obj
    when ::TrueClass
      ['1', false]
    when ::FalseClass
      ['0', false]
    else
      [obj, false]
    end
  end
end

# no type converstion is required for result set. (ruby-dbi 0.4)
class NoTypeConversion
  def self.parse(obj)
    obj
  end
end

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

  def column_metadata_to_column_info(col)
    sql_type, type_name, precision, scale =
      case col.data_type
      when :char
        [SQL_CHAR, col.charset_form == :nchar ? "NCHAR" : "CHAR", col.data_size, nil]
      when :varchar2
        [SQL_VARCHAR, col.charset_form == :nchar ? "NVARCHAR2" : "VARCHAR2", col.data_size, nil]
      when :raw
        [SQL_VARBINARY, "RAW", col.data_size, nil]
      when :long
        [SQL_LONGVARCHAR, "LONG", 4000, nil]
      when :long_raw
        [SQL_LONGVARBINARY, "LONG RAW", 4000, nil]
      when :clob
        [SQL_CLOB, col.charset_form == :nchar ? "NCLOB" : "CLOB", 4000, nil]
      when :blob
        [SQL_BLOB, "BLOB", 4000, nil]
      when :bfile
        [SQL_BLOB, "BFILE", 4000, nil]
      when :number
        if col.scale == -127 && col.precision != 0
          # To convert from binary to decimal precision, multiply n by 0.30103.
          [SQL_FLOAT, "FLOAT", (col.precision * 0.30103).ceil , nil]
        elsif col.precision == 0
          # NUMBER or calculated value (eg. col * 1.2).
          [SQL_NUMERIC, "NUMBER", 38, nil]
        else
          [SQL_NUMERIC, "NUMBER", col.precision, col.scale]
        end
      when :binary_float
        # (23 * 0.30103).ceil => 7
        [SQL_FLOAT, "BINARY_FLOAT", 7, nil]
      when :binary_double
        # (52 * 0.30103).ceil => 16
        [SQL_DOUBLE, "BINARY_DOUBLE", 16, nil]
      when :date
        # yyyy-mm-dd hh:mi:ss
        [SQL_DATE, "DATE", 19, nil]
      when :timestamp
        # yyyy-mm-dd hh:mi:ss.SSSS
        [SQL_TIMESTAMP, "TIMESTAMP", 20 + col.fsprecision, nil]
      when :timestamp_tz
        # yyyy-mm-dd hh:mi:ss.SSSS +HH:MM
        [SQL_TIMESTAMP, "TIMESTAMP WITH TIME ZONE", 27 + col.fsprecision, nil]
      when :timestamp_ltz
        # yyyy-mm-dd hh:mi:ss.SSSS
        [SQL_TIMESTAMP, "TIMESTAMP WITH LOCAL TIME ZONE", 20 + col.fsprecision, nil]
      when :interval_ym
        # yyyy-mm
        [SQL_OTHER, 'INTERVAL YEAR TO MONTH', col.lfprecision + 3, nil]
      when :interval_ds
        # dd hh:mi:ss.SSSSS
        [SQL_OTHER, 'INTERVAL DAY TO SECOND', col.lfprecision + 10 + col.fsprecision, nil]
      else
        [SQL_OTHER, col.data_type.to_s, nil, nil]
      end
    {'name' => col.name,
      'sql_type' => sql_type,
      'type_name' => type_name,
      'nullable' => col.nullable?,
      'precision' => precision,
      'scale' => scale,
      'dbi_type' => NoTypeConversion,
    }
  end
  private :column_metadata_to_column_info
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

  # SQLs are copied from DBD::Oracle.
  def columns(table)
    tab = @handle.describe_table(table)
    cols = tab.columns
    cols.collect! do |col|
      column_metadata_to_column_info(col)
    end

    dbh = DBI::DatabaseHandle.new(self)

    primaries = {}
    dbh.select_all(<<EOS, tab.obj_schema, tab.obj_name) do |row|
select column_name
  from all_cons_columns a, all_constraints b
 where a.owner = b.owner
   and a.constraint_name = b.constraint_name
   and a.table_name = b.table_name
   and b.constraint_type = 'P'
   and b.owner = :1
   and b.table_name = :2
EOS
      primaries[row[0]] = true
    end

    indices = {}
    uniques = {}
    dbh.select_all(<<EOS, tab.obj_schema, tab.obj_name) do |row|
select a.column_name, a.index_name, b.uniqueness
  from all_ind_columns a, all_indexes b
 where a.index_name = b.index_name
   and a.index_owner = b.owner
   and a.table_owner = :1
   and a.table_name = :2
EOS
      col_name, index_name, uniqueness = row
      indices[col_name] = true
      uniques[col_name] = true if uniqueness == 'UNIQUE'
    end

    dbh.select_all(<<EOS, tab.obj_schema, tab.obj_name).collect do |row|
select column_id, column_name, data_default
  from all_tab_columns
 where owner = :1
   and table_name = :2
EOS
      col_id, col_name, default = row

      col = cols[col_id.to_i - 1]
      col_name = col['name']

      if default && default[0] == ?'
        default = default[1..-2].gsub(/''/, "'")
      end

      col['indexed']   = indices[col_name]   || false
      col['primary']   = primaries[col_name] || false
      col['unique']    = uniques[col_name]   || false
      col['default']   = default
      col
    end
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
    @cursor.column_metadata.collect do |md|
      col = column_metadata_to_column_info(md)
      col['indexed']   = nil
      col['primary']   = nil
      col['unique']    = nil
      col['default']   = nil
      col
    end
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

# DBI_STMT_NEW_ARGS is DBI::StatementHandle.new's arguments except +handle+.
#
# FYI: DBI::StatementHandle.new method signatures are follows:
#   0.2.2: handle, fetchable=false, prepared=true
#   0.4.0: handle, fetchable=false, prepared=true, convert_types=true
#   0.4.1: handle, fetchable=false, prepared=true, convert_types=true, executed=false
begin
  DBI::StatementHandle.new(nil, false, true, true, true)
  # dbi 0.4.1
  DBI_STMT_NEW_ARGS = [true, true, true, true] # :nodoc:
rescue ArgumentError
  # dbi 0.4.0 or lower
  DBI_STMT_NEW_ARGS = [true] # :nodoc:
end

if defined? ::OCI8::BindType::Base
  ##
  ## ruby-oci8 2.0 bind classes.
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
        DBI::StatementHandle.new(stmt, *DBI_STMT_NEW_ARGS)
      end
    end
  end # BindType

else
  ##
  ## ruby-oci8 1.0 bind classes.
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
          DBI::StatementHandle.new(stmt, *DBI_STMT_NEW_ARGS)
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
