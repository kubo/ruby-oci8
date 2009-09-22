#
# Here is user configurations
#
$dbuser = "ruby"
$dbpass = "oci8"
$dbname = nil


############################################################
## Don't modify below.
############################################################

unless defined? OCI8
  $LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
  $LOAD_PATH << File.join(File.dirname(__FILE__), '../ext/oci8')
  require 'oci8'
end

#
# setup $oracle_server_version
#
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

#
# define get_oracle_connection()
#
def get_oracle_connection()
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
