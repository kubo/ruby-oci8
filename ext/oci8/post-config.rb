File.foreach("#{curr_objdir}/extconf.h") do |line|
  if /^#define OCI8_CLIENT_VERSION "(...)"/ =~ line
    set_config('oracle_version', $1)
  end
end
