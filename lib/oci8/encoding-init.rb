#
# setup default OCI encoding from NLS_LANG.
#

#
class OCI8

  @@client_charset_name = charset_id2name(@@environment_handle.send(:attr_get_ub2, 31))
  # 31 is OCI_ATTR_ENV_CHARSET_ID.

  if @@client_charset_name == 'US7ASCII'
    # Check whether US7ASCII is explicitly set by NLS_LANG or not.
    nls_lang = ENV['NLS_LANG']
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
    if nls_lang.nil?
      warn "Warning: NLS_LANG is not set. fallback to US7ASCII."
    end
  end

  if defined? DEFAULT_OCI8_ENCODING
    enc = DEFAULT_OCI8_ENCODING
  else
    require 'yaml'
    yaml_file = File.dirname(__FILE__) + '/encoding.yml'
    enc = YAML::load_file(yaml_file)[@@client_charset_name]
    if enc.nil?
      raise "Cannot convert Oracle charset name #{@@client_charset_name} to Ruby encoding name in #{yaml_file}."
    end
    if enc.is_a? Array
      # Use the first available encoding in the array.
      enc = enc.find do |e| Encoding.find(e) rescue false; end
    end
  end
  OCI8.encoding = enc
end
