# @title Working with Legacy Encodings

Working with Legacy Encodings
==================

The following parameters are available since ruby-oci8 2.2.11.

* force_string_encoding

For example:

    OCI8.properties[:force_string_encoding] = true

Today, the default encoding for Ruby and Rails applications is UTF-8. This can be a problem for applications with older Oracle databases, which are encoded in much narrower character sets like ASCII.

For applications that want to maintain UTF-8 compatibility in spite of an ASCII-encoded database, they may want to force UTF-8 strings into ASCII for persistence, and then take on the responsiblity of re-encoding those strings into UTF-8 on read:

```
> val = "“Who thought smart quotes were a good idea?!”"
=> "“Who thought smart quotes were a good idea?!”"
> val = val.force_encoding Encoding::ASCII
=> "\xE2\x80\x9CWho thought smart quotes were a good idea?!\xE2\x80\x9D"
> ::ActiveSupport::Multibyte.proxy_class.new(val).tidy_bytes.to_s
=> "“Who thought smart quotes were a good idea?!”"
```

Normally, a string with an encoding that does not match that of Oracle/OCI8 would raise an `Encoding::UndefinedConversionError`.  By setting `force_string_encoding` to `true`, the application will have strings unmatched strings take on that endcoding regardless.

See also:
[gigo](https://github.com/customink/gigo) - gem that converts improperly-encoded strings with escaped characters to their proper encoding.
