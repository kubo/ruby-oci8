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

case OCI8::CLIENT_VERSION
when "805"
  $describe_need_object_mode = true
  $test_clob = false
when /80./
  $describe_need_object_mode = false
  $test_clob = false
else
  $describe_need_object_mode = false
  $test_clob = true
end

# don't modify below.

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
