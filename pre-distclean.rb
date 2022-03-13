rm_f "#{curr_objdir}/ext/oci8/oci8lib_*.map"
if RUBY_PLATFORM =~ /cygwin/
  rm_f "#{curr_objdir}/ext/oci8/OCI.def"
  rm_f "#{curr_objdir}/ext/oci8/libOCI.a"
end
