#
# setup default OCI encoding from NLS_LANG.
#

# get the environment variable NLS_LANG.
nls_lang = ENV['NLS_LANG']

if nls_lang.is_a? String and nls_lang.length == 0
  nls_lang = nil
end

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

# extract the charset name.
if nls_lang
  if nls_lang =~ /\.([[:alnum:]]+)$/
    charset = $1.upcase
  else
    raise "Invalid NLS_LANG format: #{nls_lang}"
  end
else
  warn "Warning: NLS_LANG is not set. fallback to US7ASCII."
  charset = 'US7ASCII'
end

# NLS ratio, maximum number of bytes per one chracter
case charset
when 'UTF8'
  OCI8.nls_ratio = 3
when 'AL16UTF16'
  OCI8.nls_ratio = 4
when /^[[:alpha:]]+(\d+)/
  # convert maximum number of bits per one chracter to NLS ratio.
  #   charset name  max bits  max bytes
  #   ------------  --------  ---------
  #   US7ASCII          7         1
  #   WE8ISO8859P1      8         1
  #   JA16SJIS         16         2
  #   AL32UTF8         32         4
  #   ...
  OCI8.nls_ratio = (($1.to_i + 7) >> 3)
else
  raise "Unknown NLS character set name format: #{charset}"
end

# Ruby encoding name for ruby 1.9.
if OCI8.respond_to? :encoding
  if defined? DEFAULT_OCI8_ENCODING
    enc = DEFAULT_OCI8_ENCODING
  else
    require 'yaml'
    enc = YAML::load_file(File.dirname(__FILE__) + '/encoding.yml')[charset]
    if enc.nil?
      raise "Ruby encoding name is not found in encoding.yml for NLS_LANG #{nls_lang}."
    end
    if enc.is_a? Array
      # Use the first available encoding in the array.
      enc = enc.find do |e| Encoding.find(e) rescue false; end
    end
  end
  OCI8.encoding = enc
end
