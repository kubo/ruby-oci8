# $dbuser must have permission to run DBMS_LOCK.SLEEP
#   connect as sys
#     GRANT EXECUTE ON dbms_lock TO ruby;
$dbuser = "ruby"
$dbpass = "oci8"
$dbname = nil

# for test_bind_string_as_nchar in test_encoding.rb
ENV['ORA_NCHAR_LITERAL_REPLACE'] = 'TRUE' if OCI8.client_charset_name.include? 'UTF8'

# test_clob.rb

nls_lang = ENV['NLS_LANG']
nls_lang = nls_lang.split('.')[1] unless nls_lang.nil?
nls_lang = nls_lang.upcase unless nls_lang.nil?
$lobfile = File.dirname(__FILE__) + '/../setup.rb'
$lobreadnum = 256 # counts in charactors

# don't modify below.

# $oracle_server_version: database compatible level of the Oracle server.
# $oracle_client_version: Oracle client library version for which oci8 is compiled.
# $oracle_version: lower value of $oracle_server_version and $oracle_client_version.
conn = OCI8.new($dbuser, $dbpass, $dbname)
begin
  conn.exec('select value from database_compatible_level') do |row|
    $oracle_server_version = OCI8::OracleVersion.new(row[0])
  end
rescue OCIError
  raise if $!.code != 942 # ORA-00942: table or view does not exist
  $oracle_server_version = OCI8::ORAVER_8_0
end
conn.logoff

if $oracle_server_version < OCI8.oracle_client_version
  $oracle_version = $oracle_server_version
else
  $oracle_version = OCI8.oracle_client_version
end

if $oracle_version < OCI8::ORAVER_8_1
  $test_clob = false
else
  $test_clob = true
end

begin
  Time.new(2001, 1, 1, 0, 0, 0, '+00:00')
  # ruby 1.9.2 or later
  def convert_to_time(year, month, day, hour, minute, sec, subsec, timezone)
    subsec ||= 0
    if timezone
      # with time zone
      Time.new(year, month, day, hour, minute, sec + subsec, timezone)
    else
      # without time zone
      Time.local(year, month, day, hour, minute, sec, subsec * 1000000)
    end
  end
rescue
  # ruby 1.9.1 or former
  def convert_to_time(year, month, day, hour, minute, sec, subsec, timezone)
    subsec ||= 0
    if timezone
      # with time zone
      /([+-])(\d+):(\d+)/ =~ timezone
      offset = ($1 + '1').to_i * ($2.to_i * 60 + $3.to_i)
      if offset == 0
        Time.utc(year, month, day, hour, minute, sec, subsec * 1000000)
      else
        tm = Time.local(year, month, day, hour, minute, sec, subsec * 1000000)
        raise "Failed to convert #{str} to Time" if tm.utc_offset != offset * 60
        tm
      end
    else
      # without time zone
      Time.local(year, month, day, hour, minute, sec, subsec * 1000000)
    end
  end
end

def convert_to_datetime(year, month, day, hour, minute, sec, subsec, timezone)
  subsec ||= 0
  utc_offset = if timezone
                 # with time zone
                 /([+-])(\d+):(\d+)/ =~ timezone
                 ($1 + '1').to_i * ($2.to_i * 60 + $3.to_i) * 60
               else
                 Time.local(year, month, day, hour, minute, sec).utc_offset
               end
  begin
    DateTime.civil(year, month, day, hour, minute, sec + subsec, utc_offset.to_r / 86400)
  rescue ArgumentError
    raise $! if $!.to_s != 'invalid date'
    # for ruby 1.8.6.
    # convert to a DateTime via a String as a workaround.
    if utc_offset >= 0
      sign = ?+
    else
      sign = ?-
      utc_offset = - utc_offset;
    end
    tz_min = utc_offset / 60
    tz_hour, tz_min = tz_min.divmod 60
    time_str = format("%04d-%02d-%02dT%02d:%02d:%02d.%09d%c%02d:%02d",
                      year, month, day, hour, minute, sec, (subsec * 1000_000_000).to_i, sign, tz_hour, tz_min)
    ::DateTime.parse(time_str)
  end
end

module Test
  module Unit
    class TestCase

      def get_oci8_connection()
        OCI8.new($dbuser, $dbpass, $dbname)
      rescue OCIError
        raise if $!.code != 12516 && $!.code != 12520
        # sleep a few second and try again if
        # the error code is ORA-12516 or ORA-12520.
        #
        # ORA-12516 - TNS:listener could not find available handler with
        #             matching protocol stack
        # ORA-12520 - TNS:listener could not find available handler for
        #             requested type of server
        #
        # Thanks to Christopher Jones.
        #
        # Ref: The Underground PHP and Oracle Manual (page 175 in vesion 1.4)
        #      http://www.oracle.com/technology/tech/php/pdf/underground-php-oracle-manual.pdf
        #
        sleep(5)
        OCI8.new($dbuser, $dbpass, $dbname)
      end

      def get_dbi_connection()
        DBI.connect("dbi:OCI8:#{$dbname}", $dbuser, $dbpass, 'AutoCommit' => false)
      rescue DBI::DatabaseError
        raise if $!.err != 12516 && $!.err != 12520
        # same as get_oci8_connection()
        sleep(5)
        DBI.connect("dbi:OCI8:#{$dbname}", $dbuser, $dbpass, 'AutoCommit' => false)
      end

      def drop_table(table_name)
        if $oracle_server_version < OCI8::ORAVER_10_1
          # Oracle 8 - 9i
          sql = "DROP TABLE #{table_name}"
        else
          # Oracle 10g -
          sql = "DROP TABLE #{table_name} PURGE"
        end

        if defined? @conn
          begin
            @conn.exec(sql)
          rescue OCIError
            raise if $!.code != 942 # table or view does not exist
          end
        elsif instance_variable_get(:@dbh)
          begin
            @dbh.do(sql)
          rescue DBI::DatabaseError
            raise if $!.err != 942 # table or view does not exist
          end
        end
      end # drop_table

      def drop_type(type_name)
        begin
          @conn.exec("DROP TYPE BODY #{type_name}")
        rescue OCIError
          raise if $!.code != 4043
        end
        begin
          @conn.exec("DROP TYPE #{type_name}")
        rescue OCIError
          raise if $!.code != 4043
        end
      end # drop_type
    end
  end
end

