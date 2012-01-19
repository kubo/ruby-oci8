rm_f "#{curr_objdir}/lib/oci8.rb"
rm_f "#{curr_objdir}/ext/oci8/oci8lib_18.map"
rm_f "#{curr_objdir}/ext/oci8/oci8lib_191.map"
if RUBY_PLATFORM =~ /cygwin/
  rm_f "#{curr_objdir}/ext/oci8/OCI.def"
  rm_f "#{curr_objdir}/ext/oci8/libOCI.a"
end
