#
# setup default OCI encoding from NLS_LANG.
#

# try to get NLS_LANG.
nls_lang = ENV['NLS_LANG']

# if NLS_LANG is not set, get it from the Windows registry.
if nls_lang.nil? and defined? OCI8::Win32Util
  dll_path = OCI8::Win32Util.dll_path.upcase
  if dll_path =~ %r{\\BIN\\OCI.DLL}
    oracle_home = $`
    OCI8::Win32Util.enum_homes do |home, lang|
      if oracle_home == home.upcase
        nls_lang = lang
        break
      end
    end
  end
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
  if enc.is_a? Array
    # Use the first available encoding in the array.
    enc = enc.find do |e| Encoding.find(e) rescue false; end
  end
else
  warn "Warning: NLS_LANG is not set. fallback to US-ASCII."
  enc = 'US-ASCII'
end
OCI8.encoding = enc
