require 'rdoc/rdoc'

module RDoc
  class C_Parser
    alias :do_classes_orig :do_classes
    def do_classes
      # oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject)
      handle_class_module('oci8_cOCIHandle', 'class', 'OCIHandle', 'rb_cObject', nil)
      # cOCI8 = rb_define_class("OCI8", oci8_cOCIHandle)
      handle_class_module('cOCI8', 'class', 'OCI8', 'oci8_cOCIHandle', nil)
      # mOCI8BindType = rb_define_module_under(cOCI8, "BindType");
      handle_class_module('mOCI8BindType', 'module', 'BindType', nil, 'cOCI8')
      # cOCI8BindTypeBase = rb_define_class_under(mOCI8BindType, "Base", oci8_cOCIHandle);
      handle_class_module('cOCI8BindTypeBase', 'class', 'Base', nil, 'mOCI8BindType')

      # var_name = oci8_define_class_under(in_module, "class_name", &xxxx);
      @body.scan(/(\w+)\s*=\s*oci8_define_class_under\s*\(\s*(\w+)\s*,\s*"(\w+)",\s*&\w+?\s*\)/) do
        |var_name, in_module, class_name|
        handle_class_module(var_name, "class", class_name, 'oci8_cOCIHandle', in_module)
      end

      # var_name = oci8_define_bind_class("class_name", &xxxx);
      @body.scan(/(?:(\w+)\s*=\s*)?oci8_define_bind_class\s*\(\s*"(\w+)",\s*&\w+?\s*\)/) do
        |var_name, class_name|
        var_name ||= 'dummy'
        handle_class_module(var_name, "class", class_name, 'cOCI8BindTypeBase', 'mOCI8BindType')
      end

      do_classes_orig
    end
  end
end

begin
  r = RDoc::RDoc.new
  r.document(ARGV)
rescue RDoc::RDocError => e
  $stderr.puts e.message
  exit(1)
end
