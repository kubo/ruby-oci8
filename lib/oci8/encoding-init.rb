#
# setup default OCI encoding from NLS_LANG.
#

# try to get NLS_LANG.
nls_lang = ENV['NLS_LANG']

if nls_lang.nil? and RUBY_PLATFORM =~ /mswin32|cygwin|mingw32|bccwin32/
  # TODO
end

if nls_lang
  # Extract character set name from NLS_LANG.
  if nls_lang =~ /\.([[:alnum:]]+)$/
    charset = $1.upcase
  else
    raise "Invalid NLS_LANG format: #{nls_lang}"
  end

  # Convert the Oracle character set name to Ruby encoding name by
  # querying the yaml data.
  require 'yaml'
  enc = YAML::load_file(File.dirname(__FILE__) + '/encoding.yml')[charset]
  if enc.nil?
    raise "Ruby encoding name is not found in encoding.yml for NLS_LANG #{nls_lang}."
  end
else
  warn "Warning: NLS_LANG is not set. fallback to US-ASCII."
  enc = 'US-ASCII'
end
OCI8.encoding = enc
