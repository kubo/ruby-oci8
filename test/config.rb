# $dbuser must have permission to run DBMS_LOCK.SLEEP
#   connect as sys
#     GRANT EXECUTE ON dbms_lock TO ruby;
$dbuser = "ruby"
$dbpass = "oci8"
$dbname = nil

# test_clob.rb

nls_lang = ENV['NLS_LANG']
nls_lang = nls_lang.split('.')[1] unless nls_lang.nil?
nls_lang = nls_lang.upcase unless nls_lang.nil?
case nls_lang
when 'JA16EUC'
  $lobfile = '../doc/api.ja.rd' # EUC-JP file
else
  $lobfile = '../doc/api.en.rd' # ASCII file
end
$lobreadnum = 256 # counts in charactors

# don't modify below.

# $oracle_server_version: database compatible level of the Oracle server.
# $oracle_client_version: Oracle client library version for which oci8 is compiled.
# $oracle_version: lower value of $oracle_server_version and $oracle_client_version.
conn = OCI8.new($dbuser, $dbpass, $dbname)
conn.exec('select value from database_compatible_level') do |row|
  ver = row[0].split('.')
  $oracle_server_version = (ver[0] + ver[1] + ver[2]).to_i
end
conn.logoff
$oracle_client_version = OCI8::CLIENT_VERSION.to_i
if $oracle_server_version < $oracle_client_version
  $oracle_version = $oracle_server_version
else
  $oracle_version = $oracle_client_version
end

if $oracle_version <= 805
  $describe_need_object_mode = true
  $test_clob = false
elsif $oracle_version < 810
  $describe_need_object_mode = false
  $test_clob = false
else
  $describe_need_object_mode = false
  $test_clob = true
end

$env_is_initialized = false
def setup_lowapi()
  if ! $env_is_initialized
    if $describe_need_object_mode
      OCIEnv.initialise(OCI_OBJECT)
    else
      OCIEnv.initialise(OCI_DEFAULT)
    end
    $env_is_initialized = true
  end
  env = OCIEnv.init()
  svc = env.logon($dbuser, $dbpass, $dbname)
  stmt = env.alloc(OCIStmt)
  return env, svc, stmt
end

module Test
  module Unit
    class TestCase
      def drop_table(table_name)
        if $oracle_server_version < 1000
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
    end
  end
end

